// The game's LZMA decoder loop run as host code [Alchemy] (docs/engines/alchemy/lzma.md).
//
// Alchemy's archives (IGA: Giants' and Swap Force's .arc, .pak) are chunked LZMA; the titles link the public-domain LZMA
// SDK's decoder (LzmaDec.c, 9.x: the CLzmaDec layout and the probability table offsets below match). Translated, its
// inner loop LzmaDec_DecodeReal is most of a level load's CPU time. This is the same loop, written from the public
// LZMA format (range coder, literal / match / rep states) over the guest's own decoder state: the CLzmaDec struct, its
// probability table (16-bit big-endian, in guest memory) and the dictionary stay where the game keeps them, so every
// other function of the SDK (DecodeToDic, the dummy try, WriteRem, the allocators) keeps running translated.
//
// SKY_HOST_LZMA=0 runs the translated loop instead (to measure). `[lzma] N calls (host): X MB decoded in Y ms` lines
// give the totals at 4096 calls and each doubling. SKY_LZMA_CHECK=1 runs both on every call (the
// translated one on a copy of the state as it was) and logs any difference in the result, the struct, the
// probabilities or the dictionary.
#include "game_hooks.h"
#include "host/env.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ppc_config.h>
#include <ppc_context.h>
#include <tuple>
#include <vector>

#ifdef GAME_HOOK_LzmaDec_DecodeReal
PPC_FUNC_IMPL(GAME_IMP(LzmaDec_DecodeReal));

namespace
{
    // CLzmaDec (32-bit, big-endian): CLzmaProps {lc, lp, pb, dicSize}, probs, dic, buf, range, code, dicPos, dicBufSize,
    // processedPos, checkDicSize, state, reps[4], remainLen.
    namespace Dec
    {
        constexpr uint32_t Lc = 0, Lp = 4, Pb = 8, Probs = 16, Dic = 20, Buf = 24, Range = 28, Code = 32, DicPos = 36,
                           DicBufSize = 40, ProcessedPos = 44, CheckDicSize = 48, State = 52, Reps = 56, RemainLen = 72,
                           End = 76;
    }

    // The probability table (in probabilities, 16 bits each).
    constexpr uint32_t NumStates = 12, NumLitStates = 7, PosBitsMax = 4;
    constexpr uint32_t LenLowBits = 3, LenMidBits = 3, LenHighBits = 8;
    constexpr uint32_t LenChoice = 0, LenChoice2 = 1, LenLow = 2, LenMid = LenLow + (16 << LenLowBits),
                       LenHigh = LenMid + (16 << LenMidBits), NumLenProbs = LenHigh + (1 << LenHighBits);
    constexpr uint32_t IsMatch = 0, IsRep = IsMatch + (NumStates << PosBitsMax), IsRepG0 = IsRep + NumStates,
                       IsRepG1 = IsRepG0 + NumStates, IsRepG2 = IsRepG1 + NumStates, IsRep0Long = IsRepG2 + NumStates,
                       PosSlot = IsRep0Long + (NumStates << PosBitsMax), SpecPos = PosSlot + (4 << 6),
                       Align = SpecPos + 128 - 14, LenCoder = Align + 16, RepLenCoder = LenCoder + NumLenProbs,
                       Literal = RepLenCoder + NumLenProbs;
    static_assert(Literal == 1846 && Align == 802 && LenCoder == 818, "LzmaDec.c's table layout");
    constexpr uint32_t MatchSpecLenStart = 2 + (1 << LenLowBits) + (1 << LenMidBits) + (1 << LenHighBits); // 274
    constexpr uint32_t TopValue = 1u << 24, BitModelTotal = 1u << 11, MoveBits = 5;

    struct Decoder
    {
        uint16_t* probs; // big-endian
        const uint8_t* buf;
        uint32_t range, code;

        uint32_t Prob(const uint16_t* p) const { return __builtin_bswap16(*p); }
        void Normalize()
        {
            if (range < TopValue)
            {
                range <<= 8;
                code = (code << 8) | *buf++;
            }
        }
        // One bit with the adaptive probability at `p`.
        uint32_t Bit(uint16_t* p)
        {
            uint32_t ttt = Prob(p);
            Normalize();
            uint32_t bound = (range >> 11) * ttt;
            if (code < bound)
            {
                range = bound;
                *p = __builtin_bswap16(uint16_t(ttt + ((BitModelTotal - ttt) >> MoveBits)));
                return 0;
            }
            range -= bound;
            code -= bound;
            *p = __builtin_bswap16(uint16_t(ttt - (ttt >> MoveBits)));
            return 1;
        }
        // A bit tree of `limit` symbols: the symbol minus `limit`.
        uint32_t Tree(uint16_t* p, uint32_t limit)
        {
            uint32_t i = 1;
            do
                i = i + i + Bit(p + i);
            while (i < limit);
            return i - limit;
        }
    };

    uint32_t Read32(uint8_t* base, uint32_t address)
    {
        return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + address));
    }
    void Write32(uint8_t* base, uint32_t address, uint32_t value)
    {
        *reinterpret_cast<uint32_t*>(base + address) = __builtin_bswap32(value);
    }

    // LzmaDec_DecodeReal(p, limit, bufLimit): decodes until the dictionary position reaches `limit` or the input
    // reaches `bufLimit` (both checked after each literal or match), then saves the state. 0 = SZ_OK, 1 = SZ_ERROR_DATA.
    uint32_t DecodeReal(uint8_t* base, uint32_t p, uint32_t limit, uint32_t bufLimitAddress)
    {
        Decoder rc{ reinterpret_cast<uint16_t*>(base + Read32(base, p + Dec::Probs)), base + Read32(base, p + Dec::Buf),
            Read32(base, p + Dec::Range), Read32(base, p + Dec::Code) };
        uint16_t* probs = rc.probs;
        const uint8_t* bufLimit = base + bufLimitAddress;
        uint32_t state = Read32(base, p + Dec::State);
        uint32_t rep0 = Read32(base, p + Dec::Reps), rep1 = Read32(base, p + Dec::Reps + 4),
                 rep2 = Read32(base, p + Dec::Reps + 8), rep3 = Read32(base, p + Dec::Reps + 12);
        const uint32_t pbMask = (1u << Read32(base, p + Dec::Pb)) - 1, lpMask = (1u << Read32(base, p + Dec::Lp)) - 1;
        const uint32_t lc = Read32(base, p + Dec::Lc);
        uint8_t* dic = base + Read32(base, p + Dec::Dic);
        const uint32_t dicBufSize = Read32(base, p + Dec::DicBufSize),
                       checkDicSize = Read32(base, p + Dec::CheckDicSize);
        uint32_t dicPos = Read32(base, p + Dec::DicPos), processedPos = Read32(base, p + Dec::ProcessedPos);
        uint32_t len = 0;

        do
        {
            const uint32_t posState = processedPos & pbMask;
            if (rc.Bit(probs + IsMatch + (state << PosBitsMax) + posState) == 0)
            {
                // A literal, coded with the previous byte's top lc bits (and after a match, the byte at rep0).
                uint16_t* prob = probs + Literal;
                if (checkDicSize != 0 || processedPos != 0)
                    prob += 0x300 * (((processedPos & lpMask) << lc) +
                                        (dic[(dicPos == 0 ? dicBufSize : dicPos) - 1] >> (8 - lc)));
                uint32_t symbol = 1;
                if (state < NumLitStates)
                {
                    state -= state < 4 ? state : 3;
                    do
                        symbol = symbol + symbol + rc.Bit(prob + symbol);
                    while (symbol < 0x100);
                }
                else
                {
                    uint32_t matchByte = dic[dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0)];
                    uint32_t offs = 0x100;
                    state -= state < 10 ? 3 : 6;
                    do
                    {
                        matchByte <<= 1;
                        uint32_t bit = matchByte & offs;
                        uint32_t b = rc.Bit(prob + offs + bit + symbol);
                        symbol = symbol + symbol + b;
                        offs &= b ? bit : ~bit;
                    } while (symbol < 0x100);
                }
                dic[dicPos++] = uint8_t(symbol);
                processedPos++;
                continue;
            }

            uint16_t* prob;
            if (rc.Bit(probs + IsRep + state) == 0)
            {
                state += NumStates; // a match: its distance follows its length
                prob = probs + LenCoder;
            }
            else
            {
                if (checkDicSize == 0 && processedPos == 0)
                    return 1;
                if (rc.Bit(probs + IsRepG0 + state) == 0)
                {
                    if (rc.Bit(probs + IsRep0Long + (state << PosBitsMax) + posState) == 0)
                    {
                        // A short rep: one byte from rep0.
                        dic[dicPos] = dic[dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0)];
                        dicPos++;
                        processedPos++;
                        state = state < NumLitStates ? 9 : 11;
                        continue;
                    }
                }
                else
                {
                    uint32_t distance;
                    if (rc.Bit(probs + IsRepG1 + state) == 0)
                        distance = rep1;
                    else
                    {
                        if (rc.Bit(probs + IsRepG2 + state) == 0)
                            distance = rep2;
                        else
                        {
                            distance = rep3;
                            rep3 = rep2;
                        }
                        rep2 = rep1;
                    }
                    rep1 = rep0;
                    rep0 = distance;
                }
                state = state < NumLitStates ? 8 : 11;
                prob = probs + RepLenCoder;
            }

            // The length.
            if (rc.Bit(prob + LenChoice) == 0)
                len = rc.Tree(prob + LenLow + (posState << LenLowBits), 1 << LenLowBits);
            else if (rc.Bit(prob + LenChoice2) == 0)
                len = (1 << LenLowBits) + rc.Tree(prob + LenMid + (posState << LenMidBits), 1 << LenMidBits);
            else
                len = (1 << LenLowBits) + (1 << LenMidBits) + rc.Tree(prob + LenHigh, 1 << LenHighBits);

            if (state >= NumStates)
            {
                // A match's distance: a 6-bit slot, then direct bits and the 4 align bits for the far ones.
                uint32_t distance = rc.Tree(probs + PosSlot + ((len < 4 ? len : 3) << 6), 1 << 6);
                if (distance >= 4)
                {
                    const uint32_t posSlot = distance;
                    int numDirectBits = int((distance >> 1) - 1);
                    distance = 2 | (distance & 1);
                    if (posSlot < 14)
                    {
                        distance <<= numDirectBits;
                        uint16_t* spec = probs + SpecPos + distance - posSlot - 1;
                        uint32_t mask = 1, i = 1;
                        do
                        {
                            uint32_t b = rc.Bit(spec + i);
                            i = i + i + b;
                            if (b)
                                distance |= mask;
                            mask <<= 1;
                        } while (--numDirectBits != 0);
                    }
                    else
                    {
                        numDirectBits -= 4;
                        do
                        {
                            rc.Normalize();
                            rc.range >>= 1;
                            rc.code -= rc.range;
                            uint32_t t = 0 - (rc.code >> 31);
                            distance = (distance << 1) + (t + 1);
                            rc.code += rc.range & t;
                        } while (--numDirectBits != 0);
                        distance <<= 4;
                        uint32_t i = 1;
                        for (uint32_t bit = 1; bit <= 8; bit <<= 1)
                        {
                            uint32_t b = rc.Bit(probs + Align + i);
                            i = i + i + b;
                            if (b)
                                distance |= bit;
                        }
                        if (distance == 0xFFFFFFFF)
                        {
                            // The end marker.
                            len += MatchSpecLenStart;
                            state -= NumStates;
                            break;
                        }
                    }
                }
                rep3 = rep2;
                rep2 = rep1;
                rep1 = rep0;
                rep0 = distance + 1;
                if (checkDicSize == 0 ? distance >= processedPos : distance >= checkDicSize)
                    return 1;
                state = state < NumStates + NumLitStates ? NumLitStates : NumLitStates + 3;
            }

            // Copy the match, up to `limit`; what does not fit stays in remainLen.
            len += 2;
            if (limit == dicPos)
                return 1;
            const uint32_t rem = limit - dicPos;
            uint32_t curLen = rem < len ? rem : len;
            uint32_t pos = dicPos - rep0 + (dicPos < rep0 ? dicBufSize : 0);
            processedPos += curLen;
            len -= curLen;
            if (pos + curLen <= dicBufSize)
            {
                uint8_t* dest = dic + dicPos;
                const uint8_t* src = dic + pos;
                dicPos += curLen;
                for (uint32_t k = 0; k < curLen; ++k) // byte by byte: source and destination overlap when rep0 < len
                    dest[k] = src[k];
            }
            else
            {
                do
                {
                    dic[dicPos++] = dic[pos];
                    if (++pos == dicBufSize)
                        pos = 0;
                } while (--curLen != 0);
            }
        } while (dicPos < limit && rc.buf < bufLimit);
        rc.Normalize();

        Write32(base, p + Dec::Buf, uint32_t(rc.buf - base));
        Write32(base, p + Dec::Range, rc.range);
        Write32(base, p + Dec::Code, rc.code);
        Write32(base, p + Dec::RemainLen, len);
        Write32(base, p + Dec::DicPos, dicPos);
        Write32(base, p + Dec::ProcessedPos, processedPos);
        Write32(base, p + Dec::Reps, rep0);
        Write32(base, p + Dec::Reps + 4, rep1);
        Write32(base, p + Dec::Reps + 8, rep2);
        Write32(base, p + Dec::Reps + 12, rep3);
        Write32(base, p + Dec::State, state);
        return 0;
    }

    // SKY_LZMA_CHECK=1: the host loop, then the translated one from the same state; any difference is logged.
    void CheckedDecodeReal(PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<uint64_t> calls{ 0 }, mismatches{ 0 };
        const uint32_t p = ctx.r3.u32, limit = ctx.r4.u32, bufLimit = ctx.r5.u32;
        const uint32_t probsAddress = Read32(base, p + Dec::Probs), dicAddress = Read32(base, p + Dec::Dic);
        const uint32_t dicBufSize = Read32(base, p + Dec::DicBufSize);
        const uint32_t numProbs = Literal + (0x300u << (Read32(base, p + Dec::Lc) + Read32(base, p + Dec::Lp)));
        auto copy = [&](uint32_t address, uint32_t size) {
            return std::vector<uint8_t>(base + address, base + address + size);
        };
        const auto put = [&](uint32_t address, const std::vector<uint8_t>& bytes) {
            memcpy(base + address, bytes.data(), bytes.size());
        };
        const auto before =
            std::make_tuple(copy(p, Dec::End), copy(probsAddress, numProbs * 2), copy(dicAddress, dicBufSize));

        const uint32_t hostResult = DecodeReal(base, p, limit, bufLimit);
        const auto host =
            std::make_tuple(copy(p, Dec::End), copy(probsAddress, numProbs * 2), copy(dicAddress, dicBufSize));
        put(p, std::get<0>(before));
        put(probsAddress, std::get<1>(before));
        put(dicAddress, std::get<2>(before));
        GAME_IMP(LzmaDec_DecodeReal)(ctx, base);
        const auto guest =
            std::make_tuple(copy(p, Dec::End), copy(probsAddress, numProbs * 2), copy(dicAddress, dicBufSize));

        const uint64_t n = ++calls;
        if (hostResult != ctx.r3.u32 || host != guest)
        {
            const uint64_t m = ++mismatches;
            if (m <= 20)
                fprintf(stderr,
                    "[lzma] check: call %llu differs (result %u/%u, struct %s, probabilities %s, dictionary %s)\n",
                    (unsigned long long)n, hostResult, ctx.r3.u32,
                    std::get<0>(host) == std::get<0>(guest) ? "same" : "differs",
                    std::get<1>(host) == std::get<1>(guest) ? "same" : "differ",
                    std::get<2>(host) == std::get<2>(guest) ? "same" : "differs");
        }
        if ((n & (n - 1)) == 0 && n >= 1024)
            fprintf(stderr, "[lzma] check: %llu calls, %llu differ\n", (unsigned long long)n,
                (unsigned long long)mismatches.load());
    }
}

PPC_FUNC(GAME_HOOK(LzmaDec_DecodeReal))
{
    static const bool check = [] {
        const char* v = skyenv::Get("SKY_LZMA_CHECK");
        return v != nullptr && strcmp(v, "1") == 0;
    }();
    static const bool host = [] {
        const char* v = skyenv::Get("SKY_HOST_LZMA");
        return v == nullptr || strcmp(v, "0") != 0;
    }();
    // Totals over every thread, logged at 4096 calls and each doubling: the bytes decoded and the time in this loop.
    static std::atomic<uint64_t> calls{ 0 }, bytes{ 0 }, nanoseconds{ 0 };
    const uint32_t p = ctx.r3.u32, dicPos = Read32(base, p + Dec::DicPos);
    const auto start = std::chrono::steady_clock::now();
    if (check)
        CheckedDecodeReal(ctx, base);
    else if (host)
        ctx.r3.u64 = DecodeReal(base, p, ctx.r4.u32, ctx.r5.u32);
    else
        GAME_IMP(LzmaDec_DecodeReal)(ctx, base);
    nanoseconds += uint64_t(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
    bytes += Read32(base, p + Dec::DicPos) - dicPos;
    const uint64_t n = ++calls;
    if ((n & (n - 1)) == 0 && n >= 4096)
        fprintf(stderr, "[lzma] %llu calls (%s): %.1f MB decoded in %.0f ms of the decoder threads' time\n",
            (unsigned long long)n,
            check  ? "checked"
            : host ? "host"
                   : "translated",
            bytes.load() / 1048576.0, nanoseconds.load() / 1e6);
}
#endif
