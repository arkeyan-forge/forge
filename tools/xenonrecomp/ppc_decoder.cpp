// Our own PowerPC decoder for XenonRecomp. Sources, all public:
//   [B1]   IBM, PowerPC User Instruction Set Architecture, Book I, version 2.02 (encodings, instruction forms, and
//          appendix B "Assembler Extended Mnemonics").
//   [B2]   Book II, version 2.02 (storage control: sync, lwsync, eieio, dcb*, icbi, isync).
//   [B3]   Book III, version 2.02 (operating environment: mtmsr(d), rfid, hrfid, slb*, tlb*, mtsr, SPR numbers).
//   [PEM]  IBM/Freescale, PowerPC Microprocessor Family: The Programming Environments for 32-Bit Microprocessors
//          and for 64-Bit Microprocessors (chapter 8 encodings, appendix F simplified mnemonics, SPR tables).
//   [VMX]  Freescale, AltiVec Technology Programming Environments Manual (chapter 6 encodings, data stream touch).
//   [CBE]  IBM, Cell Broadband Engine Programming Handbook and the PPU's instruction set extensions (lvlx/lvrx
//          family, dcbzl, thread priority and delay "or" forms, ldbrx/stdbrx).
//   [X128] The public descriptions of the Xbox 360's VMX128 extension (primary opcodes 4, 5 and 6, 128 registers).
//   [SPR]  The SPR tables of the processor manuals: PowerPC 601 (MQ, RTC), 603e/750 (BATs, L2CR, PMCs, THRM),
//          MPC8xx (debug, MMU and cache SPRs), PPC403/405 (protection bounds, DCCR/ICCR, timers), Book E (IVORs).
// The decoder is a tree over the instruction's own fields: the primary opcode (bits 0-5) picks the form, the form's
// extended-opcode field picks the instruction, and the code of that form extracts and prints the operands. Where an
// extended mnemonic exists (li, mr, blr, mfctr...) it is tested first, on the fields it fixes.
// Mnemonics, the choice between a mnemonic and its extended forms, and the operand text follow the output the
// translator has always produced, so XenonRecomp writes the same ppc/ sources (proved word by word, see
// docs/tools/toolchain/xenonrecomp.md).
#include "ppc_decoder.h"
#include <cstring>
#include <ppc-inst.h>

namespace
{
    // A field of the word, IBM bit numbering (bit 0 is the most significant).
    constexpr uint32_t Field(uint32_t w, int first, int last)
    {
        return (w >> (31 - last)) & ((1u << (last - first + 1)) - 1);
    }

    constexpr int32_t SignExtend(uint32_t value, int bits) { return int32_t(value << (32 - bits)) >> (32 - bits); }

    // The fields most forms share [B1 1.6.28].
    constexpr uint32_t Rt(uint32_t w) { return Field(w, 6, 10); } // RT, RS, FRT, VD, TO, BO, BT, TH
    constexpr uint32_t Ra(uint32_t w) { return Field(w, 11, 15); } // RA, FRA, VA, BI, BA
    constexpr uint32_t Rb(uint32_t w) { return Field(w, 16, 20); } // RB, FRB, VB, BB, SH, NB
    constexpr uint32_t Rc(uint32_t w) { return w & 1; } // Rc, LK
    constexpr uint32_t Xo(uint32_t w) { return Field(w, 21, 30); } // X, XL, XFX forms
    constexpr int32_t Si(uint32_t w) { return SignExtend(w & 0xFFFF, 16); } // SI, D
    constexpr uint32_t Spr(uint32_t w) { return Field(w, 16, 20) << 5 | Field(w, 11, 15); } // halves swapped [B3]

    // VMX128 register numbers, split across the word [X128].
    constexpr uint32_t Vd128(uint32_t w) { return Field(w, 6, 10) | Field(w, 28, 29) << 5; }
    constexpr uint32_t Va128(uint32_t w) { return Field(w, 11, 15) | Field(w, 26, 26) << 5 | Field(w, 21, 21) << 6; }
    constexpr uint32_t Vb128(uint32_t w) { return Field(w, 16, 20) | Field(w, 30, 31) << 5; }

    constexpr PpcOpcode Insn(const char* name, int id, int operands)
    {
        PpcOpcode opcode{ name, id, {} };
        for (int i = 0; i < operands; i++)
            opcode.operands[i] = 1;
        return opcode;
    }

// One instruction (name, PPC_INST_ id, operand count); RC adds its record form ("and", "and."), OE its overflow
// forms ("add", "add.", "addo", "addo."), indexed by Rc and OE.
#define INSN(name, id, n) Insn(name, PPC_INST_##id, n)
#define RC(name, id, n) INSN(name, id, n), INSN(name ".", id, n)
#define OE(name, id, n) RC(name, id, n), RC(name "o", id##O, n)
// The constant descriptors, held where the decoding code names them.
#define OPS(...)                                                                                                       \
    ([]() -> const PpcOpcode* {                                                                                        \
        static constexpr PpcOpcode ops[] = { __VA_ARGS__ };                                                            \
        return ops;                                                                                                    \
    }())
#define OP(name, id, n) (*OPS(INSN(name, id, n)))

    const char* const ConditionNames[4] = { "lt", "gt", "eq", "so" };

    // The instruction being decoded: each operand stores its value in ppc_insn::operands and appends its text.
    class Out
    {
    public:
        Out(ppc_insn& insn, uint64_t address) : m_insn(insn), m_address(address), m_text(insn.op_str) {}

        uint32_t Address() const { return uint32_t(m_address); }

        Out& Is(const PpcOpcode& opcode)
        {
            m_insn.opcode = &opcode;
            return *this;
        }
        Out& Gpr(uint32_t r)
        {
            Begin(r);
            Put('r');
            Decimal(r);
            return *this;
        }
        // A register field where 0 means the value 0 (a base, lq/stq's pair): "0" or "rN".
        Out& GprOr0(uint32_t r)
        {
            Begin(r);
            if (r == 0)
                Put('0');
            else
            {
                Put('r');
                Decimal(r);
            }
            return *this;
        }
        // The base of a displacement, right after it: "(0)" or "(rN)".
        Out& Base(uint32_t r)
        {
            Store(r);
            Put('(');
            if (r == 0)
                Put('0');
            else
            {
                Put('r');
                Decimal(r);
            }
            Put(')');
            return *this;
        }
        Out& Fpr(uint32_t f)
        {
            Begin(f);
            Put('f');
            Decimal(f);
            return *this;
        }
        Out& Vr(uint32_t v)
        {
            Begin(v);
            Put('v');
            Decimal(v);
            return *this;
        }
        Out& Cr(uint32_t field)
        {
            Begin(field);
            Add("cr");
            Decimal(field);
            return *this;
        }
        // A condition register bit: "lt" or "4*cr7+so".
        Out& CrBit(uint32_t bit)
        {
            Begin(bit);
            if (bit >= 4)
            {
                Add("4*cr");
                Decimal(bit >> 2);
                Put('+');
            }
            Add(ConditionNames[bit & 3]);
            return *this;
        }
        Out& Number(uint32_t value)
        {
            Begin(value);
            Decimal(value);
            return *this;
        }
        Out& Signed(int32_t value)
        {
            Begin(uint32_t(value));
            if (value < 0)
                Put('-');
            Decimal(value < 0 ? 0u - uint32_t(value) : uint32_t(value));
            return *this;
        }
        Out& Target(uint32_t target)
        {
            Begin(target);
            Add("0x");
            Hex(target);
            return *this;
        }
        // An operand that is not printed: a hidden one (mr's second RS) or an optional one left at zero.
        Out& Skip(uint32_t value)
        {
            Store(value);
            return *this;
        }
        Out& OptionalCr(uint32_t field) { return field != 0 ? Cr(field) : Skip(0); }
        Out& OptionalNumber(uint32_t value) { return value != 0 ? Number(value) : Skip(0); }
        Out& OptionalGpr(uint32_t r) { return r != 0 ? Gpr(r) : Skip(0); }
        bool Done()
        {
            *m_text = 0;
            return true;
        }

        // Not an instruction.
        void Long(uint32_t word)
        {
            m_insn.opcode = nullptr;
            Add(".long 0x");
            Hex(word);
            *m_text = 0;
        }

    private:
        ppc_insn& m_insn;
        uint64_t m_address;
        char* m_text; // the operand text is far shorter than op_str: at most 5 operands of a few characters
        int m_count = 0;
        bool m_printed = false;

        void Store(uint32_t value) { m_insn.operands[m_count++] = value; }
        void Begin(uint32_t value)
        {
            Store(value);
            if (m_printed)
                Put(',');
            m_printed = true;
        }
        void Put(char c) { *m_text++ = c; }
        void Add(const char* text)
        {
            while (*text != 0)
                *m_text++ = *text++;
        }
        void Decimal(uint32_t value)
        {
            char digits[10];
            int count = 0;
            do
            {
                digits[count++] = char('0' + value % 10);
                value /= 10;
            } while (value != 0);
            while (count > 0)
                Put(digits[--count]);
        }
        void Hex(uint32_t value)
        {
            int shift = 28;
            while (shift > 0 && (value >> shift) == 0)
                shift -= 4;
            for (; shift >= 0; shift -= 4)
                Put("0123456789abcdef"[(value >> shift) & 0xF]);
        }
    };

    // Trap conditions by TO [B1 B.6], each as tw<c>, td<c>, tw<c>i, td<c>i; nullptr when TO has no extended
    // mnemonic (lnl, lng, nl and ng share their TO with lge, lle, ge and le, which the translator prints).
#define TRAP(c, C)                                                                                                     \
    { INSN("tw" c, TW##C, 2), INSN("td" c, TD##C, 2), INSN("tw" c "i", TW##C##I, 2), INSN("td" c "i", TD##C##I, 2) }
    enum TrapKind
    {
        TrapWord,
        TrapDouble,
        TrapWordImmediate,
        TrapDoubleImmediate
    };
    const PpcOpcode* TrapName(uint32_t to, TrapKind kind)
    {
        static constexpr PpcOpcode traps[][4] = { TRAP("lgt", LGT), TRAP("llt", LLT), TRAP("eq", EQ), TRAP("lge", LGE),
            TRAP("lle", LLE), TRAP("gt", GT), TRAP("ge", GE), TRAP("lt", LT), TRAP("le", LE), TRAP("ne", NE) };
        int row;
        switch (to)
        {
        case 1: row = 0; break;
        case 2: row = 1; break;
        case 4: row = 2; break;
        case 5: row = 3; break;
        case 6: row = 4; break;
        case 8: row = 5; break;
        case 12: row = 6; break;
        case 16: row = 7; break;
        case 20: row = 8; break;
        case 24: row = 9; break;
        default: return nullptr;
        }
        return &traps[row][kind];
    }

    // twi, tdi (D form) [B1 3.3.10].
    bool TrapImmediate(uint32_t w, Out& o, TrapKind kind)
    {
        if (const PpcOpcode* name = TrapName(Rt(w), kind))
            return o.Is(*name).Gpr(Ra(w)).Signed(Si(w)).Done();
        const PpcOpcode& general = kind == TrapWordImmediate ? OP("twi", TWI, 3) : OP("tdi", TDI, 3);
        return o.Is(general).Number(Rt(w)).Gpr(Ra(w)).Signed(Si(w)).Done();
    }

    // tw, td (X form) [B1 3.3.10].
    bool Trap(uint32_t w, Out& o, TrapKind kind)
    {
        if (Rc(w) != 0)
            return false;
        if (const PpcOpcode* name = TrapName(Rt(w), kind))
            return o.Is(*name).Gpr(Ra(w)).Gpr(Rb(w)).Done();
        if (w == 0x7FE00008)
            return o.Is(OP("trap", TRAP, 0)).Done();
        const PpcOpcode& general = kind == TrapWord ? OP("tw", TW, 3) : OP("td", TD, 3);
        return o.Is(general).Number(Rt(w)).Gpr(Ra(w)).Gpr(Rb(w)).Done();
    }

// Branch names: the AA/LK variants (b<c>, b<c>l, b<c>a, b<c>la), each plain, "-" (not taken) and "+" (taken).
#define HINTS(name, id, n) INSN(name, id, n), INSN(name "-", id, n), INSN(name "+", id, n)
#define AALK_HINTS(name, id, n)                                                                                        \
    { HINTS(name, id, n), HINTS(name "l", id##L, n), HINTS(name "a", id##A, n), HINTS(name "la", id##LA, n) }
#define AALK(name, id, n)                                                                                              \
    { INSN(name, id, n), INSN(name "l", id##L, n), INSN(name "a", id##A, n), INSN(name "la", id##LA, n) }
#define LK_HINTS(name, id, n) { HINTS(name, id, n), HINTS(name "l", id##L, n) }
    enum Hint
    {
        NoHint,
        NotTaken, // "-"
        Taken, // "+"
    };

    // The CR-bit conditions of the extended mnemonics [B1 B.2.3], indexed by BO's "branch if true" bit (true 0,
    // false 4) plus the bit within the CR field. bge, ble, bso are the names the translator prefers to bnl, bng,
    // bun.
    int ConditionIndex(uint32_t bo, uint32_t bi) { return (bo & 8 ? 0 : 4) + int(bi & 3); }

    // bc [B1 2.4.1, B.2]: BO selects the extended mnemonic. Its hint follows the "at" bits of BO: at=10 "-",
    // at=11 "+", at=0x none.
    bool BranchConditional(uint32_t w, Out& o)
    {
        static constexpr PpcOpcode ifs[8][12] = { AALK_HINTS("blt", BLT, 2), AALK_HINTS("bgt", BGT, 2),
            AALK_HINTS("beq", BEQ, 2), AALK_HINTS("bso", BSO, 2), AALK_HINTS("bge", BGE, 2), AALK_HINTS("ble", BLE, 2),
            AALK_HINTS("bne", BNE, 2), AALK_HINTS("bns", BNS, 2) };
        static constexpr PpcOpcode ctr[2][12] = { AALK_HINTS("bdnz", BDNZ, 1), AALK_HINTS("bdz", BDZ, 1) };
        static constexpr PpcOpcode ctrIf[4][4] = { AALK("bdnzf", BDNZF, 2), AALK("bdzf", BDZF, 2),
            AALK("bdnzt", BDNZT, 2), AALK("bdzt", BDZT, 2) };
        static constexpr PpcOpcode general[12] = AALK_HINTS("bc", BC, 3);
        const uint32_t bo = Rt(w), bi = Ra(w), aalk = w & 3;
        const uint32_t target = uint32_t(SignExtend(w & 0xFFFC, 16)) + (aalk & 2 ? 0 : o.Address());
        switch (bo)
        {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B: // decrement CTR, then test a CR bit: 0z0zy
            return o.Is(ctrIf[(bo >> 2 & 2) | (bo >> 1 & 1)][aalk]).CrBit(bi).Target(target).Done();
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F: // test a CR bit: 0t1at
        {
            const int hint = bo & 2 ? (bo & 1 ? Taken : NotTaken) : NoHint;
            return o.Is(ifs[ConditionIndex(bo, bi)][aalk * 3 + hint]).OptionalCr(bi >> 2).Target(target).Done();
        }
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B: // decrement CTR, then test it: 1a0zt
        {
            const int hint = bo & 8 ? (bo & 1 ? Taken : NotTaken) : NoHint;
            if (bi == 0)
                return o.Is(ctr[bo >> 1 & 1][aalk * 3 + hint]).Target(target).Done();
            if (hint != NoHint)
                return o.Is(general[aalk * 3 + hint]).Number(bo & ~1u).CrBit(bi).Target(target).Done();
            return o.Is(general[aalk * 3]).Number(bo).CrBit(bi).Target(target).Done();
        }
        case 0x14: // always
            return o.Is(general[aalk * 3]).Number(bo).CrBit(bi).Target(target).Done();
        default: return false;
        }
    }

    // A BO value the architecture defines [B1 2.4.1]; the strict test also wants version 2.02's z bits zero.
    bool ValidBo(uint32_t bo) { return !((bo >= 21 && bo <= 23) || bo >= 28); }
    bool StrictBo(uint32_t bo) { return ValidBo(bo) && bo != 1 && bo != 3 && bo != 9 && bo != 11; }

    // bclr, bcctr (XL form) [B1 2.4.1, B.2]. With BH zero, the extended mnemonics; their hint takes the older
    // reading of BO's last bit (y): y=1 "+", and "-" for the "at" value 10.
    bool BranchToRegister(uint32_t w, Out& o, bool toCtr)
    {
        static constexpr PpcOpcode always[2][2] = { { INSN("blr", BLR, 0), INSN("blrl", BLRL, 0) },
            { INSN("bctr", BCTR, 0), INSN("bctrl", BCTRL, 0) } };
        static constexpr PpcOpcode ctr[2][6] = { LK_HINTS("bdnzlr", BDNZLR, 0), LK_HINTS("bdzlr", BDZLR, 0) };
        static constexpr PpcOpcode ifLr[8][6] = { LK_HINTS("bltlr", BLTLR, 1), LK_HINTS("bgtlr", BGTLR, 1),
            LK_HINTS("beqlr", BEQLR, 1), LK_HINTS("bsolr", BSOLR, 1), LK_HINTS("bgelr", BGELR, 1),
            LK_HINTS("blelr", BLELR, 1), LK_HINTS("bnelr", BNELR, 1), LK_HINTS("bnslr", BNSLR, 1) };
        static constexpr PpcOpcode ifCtr[8][6] = { LK_HINTS("bltctr", BLTCTR, 1), LK_HINTS("bgtctr", BGTCTR, 1),
            LK_HINTS("beqctr", BEQCTR, 1), LK_HINTS("bsoctr", BSOCTR, 1), LK_HINTS("bgectr", BGECTR, 1),
            LK_HINTS("blectr", BLECTR, 1), LK_HINTS("bnectr", BNECTR, 1), LK_HINTS("bnsctr", BNSCTR, 1) };
#define LK_PLUS(name, id)                                                                                              \
    {                                                                                                                  \
        INSN(name, id, 1), INSN(name "+", id, 1), INSN(name "l", id##L, 1), INSN(name "l+", id##L, 1)                  \
    }
        static constexpr PpcOpcode ctrIf[4][4] = { LK_PLUS("bdnzflr", BDNZFLR), LK_PLUS("bdzflr", BDZFLR),
            LK_PLUS("bdnztlr", BDNZTLR), LK_PLUS("bdztlr", BDZTLR) };
#undef LK_PLUS
        // [to CTR][LK]: "-" and "+" with BO and BI, then the general form with BO, BI and BH.
        static constexpr PpcOpcode hinted[2][2][2] = { { { INSN("bclr-", BCLR, 2), INSN("bclr+", BCLR, 2) },
                                                           { INSN("bclrl-", BCLRL, 2), INSN("bclrl+", BCLRL, 2) } },
            { { INSN("bcctr-", BCCTR, 2), INSN("bcctr+", BCCTR, 2) },
                { INSN("bcctrl-", BCCTRL, 2), INSN("bcctrl+", BCCTRL, 2) } } };
        static constexpr PpcOpcode general[2][2] = { { INSN("bclr", BCLR, 3), INSN("bclrl", BCLRL, 3) },
            { INSN("bcctr", BCCTR, 3), INSN("bcctrl", BCCTRL, 3) } };
        const uint32_t bo = Rt(w), bi = Ra(w), lk = Rc(w);
        if (Field(w, 16, 20) == 0)
        {
            if (bo == 0x14 && bi == 0)
                return o.Is(always[toCtr][lk]).Done();
            switch (bo)
            {
            case 0x10:
            case 0x11:
            case 0x12:
            case 0x13:
            case 0x18:
            case 0x19:
            case 0x1A:
            case 0x1B: // decrement CTR, then test it (to LR only)
                if (!toCtr && bi == 0)
                {
                    const int hint = bo & 1 ? Taken : (bo & 8 ? NotTaken : NoHint);
                    return o.Is(ctr[bo >> 1 & 1][lk * 3 + hint]).Done();
                }
                break;
            case 0x04:
            case 0x05:
            case 0x06:
            case 0x07:
            case 0x0C:
            case 0x0D:
            case 0x0E:
            case 0x0F: // test a CR bit
            {
                const int hint = bo & 1 ? Taken : (bo & 2 ? NotTaken : NoHint);
                const PpcOpcode(&names)[8][6] = toCtr ? ifCtr : ifLr;
                return o.Is(names[ConditionIndex(bo, bi)][lk * 3 + hint]).OptionalCr(bi >> 2).Done();
            }
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x08:
            case 0x09:
            case 0x0A:
            case 0x0B: // decrement CTR, then test a CR bit (to LR only)
                if (!toCtr)
                    return o.Is(ctrIf[(bo >> 2 & 2) | (bo >> 1 & 1)][lk * 2 + (bo & 1)]).CrBit(bi).Done();
                break;
            default: break;
            }
            if ((bo & 1) == 0 && ValidBo(bo))
                return o.Is(hinted[toCtr][lk][0]).Number(bo).CrBit(bi).Done();
            if ((bo & 0x15) == 0x11)
                return o.Is(hinted[toCtr][lk][1]).Number(bo & ~1u).CrBit(bi).Done();
        }
        if (Field(w, 16, 18) == 0 && StrictBo(bo))
            return o.Is(general[toCtr][lk]).Number(bo).CrBit(bi).Number(Field(w, 19, 20)).Done();
        return false;
    }

    // Primary opcode 19: branches to LR and CTR, CR logical, returns and isync [B1 2.4, B2, B3].
    bool Op19(uint32_t w, Out& o)
    {
        const uint32_t bt = Rt(w), ba = Ra(w), bb = Rb(w);
        // The CR logical instructions: "BT,BA,BB".
        auto logical = [&](const PpcOpcode& op) { return o.Is(op).CrBit(bt).CrBit(ba).CrBit(bb).Done(); };
        switch (Xo(w))
        {
        case 16: return BranchToRegister(w, o, false);
        case 528: return BranchToRegister(w, o, true);
        default: break;
        }
        if (Rc(w) != 0)
            return false;
        switch (Xo(w))
        {
        case 0:
            return Field(w, 9, 10) == 0 && Field(w, 14, 20) == 0 &&
                   o.Is(OP("mcrf", MCRF, 2)).Cr(Field(w, 6, 8)).Cr(Field(w, 11, 13)).Done();
        case 33:
            if (ba == bb)
                return o.Is(OP("crnot", CRNOT, 3)).CrBit(bt).CrBit(ba).Skip(0).Done();
            return logical(OP("crnor", CRNOR, 3));
        case 129: return logical(OP("crandc", CRANDC, 3));
        case 193:
            if (bt == ba && ba == bb)
                return o.Is(OP("crclr", CRCLR, 3)).CrBit(bt).Skip(0).Skip(0).Done();
            return logical(OP("crxor", CRXOR, 3));
        case 225: return logical(OP("crnand", CRNAND, 3));
        case 257: return logical(OP("crand", CRAND, 3));
        case 289:
            if (bt == ba && ba == bb)
                return o.Is(OP("crset", CRSET, 3)).CrBit(bt).Skip(0).Skip(0).Done();
            return logical(OP("creqv", CREQV, 3));
        case 417: return logical(OP("crorc", CRORC, 3));
        case 449:
            if (ba == bb)
                return o.Is(OP("crmove", CRMOVE, 3)).CrBit(bt).CrBit(ba).Skip(0).Done();
            return logical(OP("cror", CROR, 3));
        default: break;
        }
        // No operand: the whole word is fixed.
        switch (w)
        {
        case 0x4C000024: return o.Is(OP("rfid", RFID, 0)).Done();
        case 0x4C00012C: return o.Is(OP("isync", ISYNC, 0)).Done();
        case 0x4C000224: return o.Is(OP("hrfid", HRFID, 0)).Done();
        case 0x4C000064: return o.Is(OP("rfi", RFI, 0)).Done(); // PPC4xx, POWER
        case 0x4C000066: return o.Is(OP("rfci", RFCI, 0)).Done(); // PPC4xx
        default: return false;
        }
    }

    // Rotates (M form) [B1 3.3.12.1], primary opcodes 20-23; rlmi is POWER's.
    bool RotateWord(uint32_t w, Out& o)
    {
        const uint32_t ra = Ra(w), rs = Rt(w), sh = Rb(w), mb = Field(w, 21, 25), me = Field(w, 26, 30), rc = Rc(w);
        switch (w >> 26)
        {
        case 20: return o.Is(OPS(RC("rlwimi", RLWIMI, 5))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Number(me).Done();
        case 21:
            if (mb == 0 && me == 31)
                return o.Is(OPS(RC("rotlwi", ROTLWI, 3))[rc]).Gpr(ra).Gpr(rs).Number(sh).Done();
            if (sh == 0 && me == 31)
                return o.Is(OPS(RC("clrlwi", CLRLWI, 3))[rc]).Gpr(ra).Gpr(rs).Number(mb).Done();
            return o.Is(OPS(RC("rlwinm", RLWINM, 5))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Number(me).Done();
        case 22: return o.Is(OPS(RC("rlmi", RLMI, 5))[rc]).Gpr(ra).Gpr(rs).Gpr(sh).Number(mb).Number(me).Done();
        default:
            if (mb == 0 && me == 31)
                return o.Is(OPS(RC("rotlw", ROTLW, 3))[rc]).Gpr(ra).Gpr(rs).Gpr(sh).Done();
            return o.Is(OPS(RC("rlwnm", RLWNM, 5))[rc]).Gpr(ra).Gpr(rs).Gpr(sh).Number(mb).Number(me).Done();
        }
    }

    // 64-bit rotates, MD form (xo in bits 27-29) and MDS form (bits 27-30) [B1 3.3.12.2].
    bool Op30(uint32_t w, Out& o)
    {
        const uint32_t ra = Ra(w), rs = Rt(w), rc = Rc(w);
        const uint32_t sh = Field(w, 16, 20) | Field(w, 30, 30) << 5, mb = Field(w, 21, 25) | Field(w, 26, 26) << 5;
        switch (Field(w, 27, 29))
        {
        case 0:
            if (mb == 0)
                return o.Is(OPS(RC("rotldi", ROTLDI, 3))[rc]).Gpr(ra).Gpr(rs).Number(sh).Done();
            if (sh == 0)
                return o.Is(OPS(RC("clrldi", CLRLDI, 3))[rc]).Gpr(ra).Gpr(rs).Number(mb).Done();
            return o.Is(OPS(RC("rldicl", RLDICL, 4))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Done();
        case 1: return o.Is(OPS(RC("rldicr", RLDICR, 4))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Done();
        case 2: return o.Is(OPS(RC("rldic", RLDIC, 4))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Done();
        case 3: return o.Is(OPS(RC("rldimi", RLDIMI, 4))[rc]).Gpr(ra).Gpr(rs).Number(sh).Number(mb).Done();
        case 4: // MDS: bit 30 is the low bit of the xo
            if (Field(w, 30, 30) != 0)
                return o.Is(OPS(RC("rldcr", RLDCR, 4))[rc]).Gpr(ra).Gpr(rs).Gpr(Rb(w)).Number(mb).Done();
            if (mb == 0)
                return o.Is(OPS(RC("rotld", ROTLD, 3))[rc]).Gpr(ra).Gpr(rs).Gpr(Rb(w)).Done();
            return o.Is(OPS(RC("rldcl", RLDCL, 4))[rc]).Gpr(ra).Gpr(rs).Gpr(Rb(w)).Number(mb).Done();
        default: return false;
        }
    }

    // D-form loads and stores [B1 3.3.2, 3.3.3, 4.6.2, 4.6.3], primary opcodes 32-55: "rT,D(rA|0)".
    bool LoadStore(uint32_t w, Out& o)
    {
        static constexpr PpcOpcode ops[] = { INSN("lwz", LWZ, 3), INSN("lwzu", LWZU, 3), INSN("lbz", LBZ, 3),
            INSN("lbzu", LBZU, 3), INSN("stw", STW, 3), INSN("stwu", STWU, 3), INSN("stb", STB, 3),
            INSN("stbu", STBU, 3), INSN("lhz", LHZ, 3), INSN("lhzu", LHZU, 3), INSN("lha", LHA, 3),
            INSN("lhau", LHAU, 3), INSN("sth", STH, 3), INSN("sthu", STHU, 3), INSN("lmw", LMW, 3),
            INSN("stmw", STMW, 3), INSN("lfs", LFS, 3), INSN("lfsu", LFSU, 3), INSN("lfd", LFD, 3),
            INSN("lfdu", LFDU, 3), INSN("stfs", STFS, 3), INSN("stfsu", STFSU, 3), INSN("stfd", STFD, 3),
            INSN("stfdu", STFDU, 3) };
        const uint32_t primary = w >> 26;
        o.Is(ops[primary - 32]);
        if (primary >= 48)
            o.Fpr(Rt(w));
        else
            o.Gpr(Rt(w));
        return o.Signed(Si(w)).Base(Ra(w)).Done();
    }

    // DS-form loads and stores (opcodes 58, 62) and lq (DQ form, 56) [B1 3.3.2, 3.3.3; B3 3.3.1 for lq/stq].
    bool LoadStoreDouble(uint32_t w, Out& o)
    {
        const int32_t ds = SignExtend(w & 0xFFFC, 16);
        const uint32_t pair = Field(w, 6, 9) << 1; // lq/stq: an even register pair
        switch ((w >> 26) << 2 | (w & 3))
        {
        case 56 << 2:
        case 56 << 2 | 1:
        case 56 << 2 | 2:
        case 56 << 2 | 3:
            return o.Is(OP("lq", LQ, 3)).GprOr0(pair).Signed(SignExtend(w & 0xFFF0, 16)).Base(Ra(w)).Done();
        case 58 << 2: return o.Is(OP("ld", LD, 3)).Gpr(Rt(w)).Signed(ds).Base(Ra(w)).Done();
        case 58 << 2 | 1: return o.Is(OP("ldu", LDU, 3)).Gpr(Rt(w)).Signed(ds).Base(Ra(w)).Done();
        case 58 << 2 | 2: return o.Is(OP("lwa", LWA, 3)).Gpr(Rt(w)).Signed(ds).Base(Ra(w)).Done();
        case 62 << 2: return o.Is(OP("std", STD, 3)).Gpr(Rt(w)).Signed(ds).Base(Ra(w)).Done();
        case 62 << 2 | 1: return o.Is(OP("stdu", STDU, 3)).Gpr(Rt(w)).Signed(ds).Base(Ra(w)).Done();
        case 62 << 2 | 2: return o.Is(OP("stq", STQ, 3)).GprOr0(pair).Signed(ds).Base(Ra(w)).Done();
        default: return false;
        }
    }

    // SPR names [SPR]: mfspr/mtspr with these SPR numbers print as mf<name>/mt<name> rT.
#define NAMED(n, name, id)                                                                                             \
    case n: return &OP(name, id, 1)
    const PpcOpcode* MfsprName(uint32_t spr)
    {
        switch (spr)
        {
            NAMED(0, "mfmq", MFMQ);
            NAMED(1, "mfxer", MFXER);
            NAMED(4, "mfrtcu", MFRTCU);
            NAMED(5, "mfrtcl", MFRTCL);
            NAMED(8, "mflr", MFLR);
            NAMED(9, "mfctr", MFCTR);
            NAMED(18, "mfdsisr", MFDSISR);
            NAMED(19, "mfdar", MFDAR);
            NAMED(22, "mfdec", MFDEC);
            NAMED(25, "mfsdr1", MFSDR1);
            NAMED(26, "mfsrr0", MFSRR0);
            NAMED(27, "mfsrr1", MFSRR1);
            NAMED(144, "mfcmpa", MFCMPA);
            NAMED(145, "mfcmpb", MFCMPB);
            NAMED(146, "mfcmpc", MFCMPC);
            NAMED(147, "mfcmpd", MFCMPD);
            NAMED(148, "mficr", MFICR);
            NAMED(149, "mfder", MFDER);
            NAMED(150, "mfcounta", MFCOUNTA);
            NAMED(151, "mfcountb", MFCOUNTB);
            NAMED(152, "mfcmpe", MFCMPE);
            NAMED(153, "mfcmpf", MFCMPF);
            NAMED(154, "mfcmpg", MFCMPG);
            NAMED(155, "mfcmph", MFCMPH);
            NAMED(156, "mflctrl1", MFLCTRL1);
            NAMED(157, "mflctrl2", MFLCTRL2);
            NAMED(158, "mfictrl", MFICTRL);
            NAMED(159, "mfbar", MFBAR);
            NAMED(256, "mfvrsave", MFVRSAVE);
            NAMED(560, "mfic_cst", MFIC_CST);
            NAMED(561, "mfic_adr", MFIC_ADR);
            NAMED(562, "mfic_dat", MFIC_DAT);
            NAMED(568, "mfdc_cst", MFDC_CST);
            NAMED(569, "mfdc_adr", MFDC_ADR);
            NAMED(570, "mfdc_dat", MFDC_DAT);
            NAMED(630, "mfdpdr", MFDPDR);
            NAMED(631, "mfdpir", MFDPIR);
            NAMED(638, "mfimmr", MFIMMR);
            NAMED(784, "mfmi_ctr", MFMI_CTR);
            NAMED(786, "mfmi_ap", MFMI_AP);
            NAMED(787, "mfmi_epn", MFMI_EPN);
            NAMED(789, "mfmi_twc", MFMI_TWC);
            NAMED(790, "mfmi_rpn", MFMI_RPN);
            NAMED(792, "mfmd_ctr", MFMD_CTR);
            NAMED(793, "mfm_casid", MFM_CASID);
            NAMED(794, "mfmd_ap", MFMD_AP);
            NAMED(795, "mfmd_epn", MFMD_EPN);
            NAMED(796, "mfmd_twb", MFMD_TWB);
            NAMED(797, "mfmd_twc", MFMD_TWC);
            NAMED(798, "mfmd_rpn", MFMD_RPN);
            NAMED(799, "mfm_tw", MFM_TW);
            NAMED(816, "mfmi_dbcam", MFMI_DBCAM);
            NAMED(817, "mfmi_dbram0", MFMI_DBRAM0);
            NAMED(818, "mfmi_dbram1", MFMI_DBRAM1);
            NAMED(824, "mfmd_dbcam", MFMD_DBCAM);
            NAMED(825, "mfmd_dbram0", MFMD_DBRAM0);
            NAMED(826, "mfmd_dbram1", MFMD_DBRAM1);
            NAMED(936, "mfummcr0", MFUMMCR0);
            NAMED(937, "mfupmc1", MFUPMC1);
            NAMED(938, "mfupmc2", MFUPMC2);
            NAMED(939, "mfusia", MFUSIA);
            NAMED(940, "mfummcr1", MFUMMCR1);
            NAMED(941, "mfupmc3", MFUPMC3);
            NAMED(942, "mfupmc4", MFUPMC4);
            NAMED(944, "mfzpr", MFZPR);
            NAMED(945, "mfpid", MFPID);
            NAMED(947, "mfccr0", MFCCR0);
            NAMED(948, "mfiac3", MFIAC3);
            NAMED(949, "mfiac4", MFIAC4);
            NAMED(950, "mfdvc1", MFDVC1);
            NAMED(951, "mfdvc2", MFDVC2);
            NAMED(952, "mfmmcr0", MFMMCR0);
            NAMED(953, "mfpmc1", MFPMC1);
            NAMED(954, "mfpmc2", MFPMC2);
            NAMED(955, "mfsia", MFSIA);
            NAMED(956, "mfmmcr1", MFMMCR1);
            NAMED(957, "mfdbcr1", MFDBCR1);
            NAMED(958, "mfpmc4", MFPMC4);
            NAMED(979, "mficdbdr", MFICDBDR);
            NAMED(980, "mfesr", MFESR);
            NAMED(981, "mfdear", MFDEAR);
            NAMED(982, "mfevpr", MFEVPR);
            NAMED(983, "mfcdbcr", MFCDBCR);
            NAMED(984, "mftsr", MFTSR);
            NAMED(986, "mftcr", MFTCR);
            NAMED(987, "mfpit", MFPIT);
            NAMED(988, "mftbhi", MFTBHI);
            NAMED(989, "mftblo", MFTBLO);
            NAMED(990, "mfsrr2", MFSRR2);
            NAMED(991, "mfsrr3", MFSRR3);
            NAMED(1008, "mfdbsr", MFDBSR);
            NAMED(1010, "mfdbcr0", MFDBCR0);
            NAMED(1012, "mfiac1", MFIAC1);
            NAMED(1013, "mfiac2", MFIAC2);
            NAMED(1014, "mfdac1", MFDAC1);
            NAMED(1015, "mfdac2", MFDAC2);
            NAMED(1017, "mfl2cr", MFL2CR);
            NAMED(1018, "mfdccr", MFDCCR);
            NAMED(1019, "mficcr", MFICCR);
            NAMED(1020, "mfpbl1", MFPBL1);
            NAMED(1021, "mfpbu1", MFPBU1);
            NAMED(1022, "mfpbl2", MFPBL2);
            NAMED(1023, "mfpbu2", MFPBU2);
        default: return nullptr;
        }
    }
    const PpcOpcode* MtsprName(uint32_t spr)
    {
        switch (spr)
        {
            NAMED(0, "mtmq", MTMQ);
            NAMED(1, "mtxer", MTXER);
            NAMED(8, "mtlr", MTLR);
            NAMED(9, "mtctr", MTCTR);
            NAMED(18, "mtdsisr", MTDSISR);
            NAMED(19, "mtdar", MTDAR);
            NAMED(20, "mtrtcu", MTRTCU);
            NAMED(21, "mtrtcl", MTRTCL);
            NAMED(22, "mtdec", MTDEC);
            NAMED(25, "mtsdr1", MTSDR1);
            NAMED(26, "mtsrr0", MTSRR0);
            NAMED(27, "mtsrr1", MTSRR1);
            NAMED(144, "mtcmpa", MTCMPA);
            NAMED(145, "mtcmpb", MTCMPB);
            NAMED(146, "mtcmpc", MTCMPC);
            NAMED(147, "mtcmpd", MTCMPD);
            NAMED(148, "mticr", MTICR);
            NAMED(149, "mtder", MTDER);
            NAMED(150, "mtcounta", MTCOUNTA);
            NAMED(151, "mtcountb", MTCOUNTB);
            NAMED(152, "mtcmpe", MTCMPE);
            NAMED(153, "mtcmpf", MTCMPF);
            NAMED(154, "mtcmpg", MTCMPG);
            NAMED(155, "mtcmph", MTCMPH);
            NAMED(156, "mtlctrl1", MTLCTRL1);
            NAMED(157, "mtlctrl2", MTLCTRL2);
            NAMED(158, "mtictrl", MTICTRL);
            NAMED(159, "mtbar", MTBAR);
            NAMED(256, "mtvrsave", MTVRSAVE);
            NAMED(936, "mtummcr0", MTUMMCR0);
            NAMED(937, "mtupmc1", MTUPMC1);
            NAMED(938, "mtupmc2", MTUPMC2);
            NAMED(939, "mtusia", MTUSIA);
            NAMED(940, "mtummcr1", MTUMMCR1);
            NAMED(941, "mtupmc3", MTUPMC3);
            NAMED(942, "mtupmc4", MTUPMC4);
            NAMED(944, "mtzpr", MTZPR);
            NAMED(945, "mtpid", MTPID);
            NAMED(947, "mtccr0", MTCCR0);
            NAMED(948, "mtiac3", MTIAC3);
            NAMED(949, "mtiac4", MTIAC4);
            NAMED(950, "mtdvc1", MTDVC1);
            NAMED(951, "mtdvc2", MTDVC2);
            NAMED(952, "mtmmcr0", MTMMCR0);
            NAMED(953, "mtsgr", MTSGR);
            NAMED(954, "mtdcwr", MTDCWR);
            NAMED(955, "mtsler", MTSLER);
            NAMED(956, "mtsu0r", MTSU0R);
            NAMED(957, "mtdbcr1", MTDBCR1);
            NAMED(958, "mtpmc4", MTPMC4);
            NAMED(979, "mticdbdr", MTICDBDR);
            NAMED(980, "mtesr", MTESR);
            NAMED(981, "mtdear", MTDEAR);
            NAMED(982, "mtevpr", MTEVPR);
            NAMED(983, "mtcdbcr", MTCDBCR);
            NAMED(984, "mttsr", MTTSR);
            NAMED(986, "mttcr", MTTCR);
            NAMED(987, "mtpit", MTPIT);
            NAMED(988, "mttbhi", MTTBHI);
            NAMED(989, "mttblo", MTTBLO);
            NAMED(990, "mtsrr2", MTSRR2);
            NAMED(991, "mtsrr3", MTSRR3);
            NAMED(1008, "mtdbsr", MTDBSR);
            NAMED(1010, "mtdbcr0", MTDBCR0);
            NAMED(1012, "mtiac1", MTIAC1);
            NAMED(1013, "mtiac2", MTIAC2);
            NAMED(1014, "mtdac1", MTDAC1);
            NAMED(1015, "mtdac2", MTDAC2);
            NAMED(1017, "mtl2cr", MTL2CR);
            NAMED(1018, "mtdccr", MTDCCR);
            NAMED(1019, "mticcr", MTICCR);
            NAMED(1020, "mtpbl1", MTPBL1);
            NAMED(1021, "mtpbu1", MTPBU1);
            NAMED(1022, "mtpbl2", MTPBL2);
            NAMED(1023, "mtpbu2", MTPBU2);
        default: return nullptr;
        }
    }
    // PPC403 device control registers by name (PPC403GCX user's manual, DCR table).
    const PpcOpcode* MfdcrName(uint32_t dcr)
    {
        switch (dcr)
        {
            NAMED(64, "mfexisr", MFEXISR);
            NAMED(66, "mfexier", MFEXIER);
            NAMED(128, "mfbr0", MFBR0);
            NAMED(129, "mfbr1", MFBR1);
            NAMED(130, "mfbr2", MFBR2);
            NAMED(131, "mfbr3", MFBR3);
            NAMED(132, "mfbr4", MFBR4);
            NAMED(133, "mfbr5", MFBR5);
            NAMED(134, "mfbr6", MFBR6);
            NAMED(135, "mfbr7", MFBR7);
            NAMED(144, "mfbear", MFBEAR);
            NAMED(145, "mfbesr", MFBESR);
            NAMED(160, "mfiocr", MFIOCR);
            NAMED(192, "mfdmacr0", MFDMACR0);
            NAMED(193, "mfdmact0", MFDMACT0);
            NAMED(194, "mfdmada0", MFDMADA0);
            NAMED(195, "mfdmasa0", MFDMASA0);
            NAMED(196, "mfdmacc0", MFDMACC0);
            NAMED(200, "mfdmacr1", MFDMACR1);
            NAMED(201, "mfdmact1", MFDMACT1);
            NAMED(202, "mfdmada1", MFDMADA1);
            NAMED(203, "mfdmasa1", MFDMASA1);
            NAMED(204, "mfdmacc1", MFDMACC1);
            NAMED(208, "mfdmacr2", MFDMACR2);
            NAMED(209, "mfdmact2", MFDMACT2);
            NAMED(210, "mfdmada2", MFDMADA2);
            NAMED(211, "mfdmasa2", MFDMASA2);
            NAMED(212, "mfdmacc2", MFDMACC2);
            NAMED(216, "mfdmacr3", MFDMACR3);
            NAMED(217, "mfdmact3", MFDMACT3);
            NAMED(218, "mfdmada3", MFDMADA3);
            NAMED(219, "mfdmasa3", MFDMASA3);
            NAMED(220, "mfdmacc3", MFDMACC3);
            NAMED(224, "mfdmasr", MFDMASR);
        default: return nullptr;
        }
    }
    const PpcOpcode* MtdcrName(uint32_t dcr)
    {
        switch (dcr)
        {
            NAMED(64, "mtexisr", MTEXISR);
            NAMED(66, "mtexier", MTEXIER);
            NAMED(128, "mtbr0", MTBR0);
            NAMED(129, "mtbr1", MTBR1);
            NAMED(130, "mtbr2", MTBR2);
            NAMED(131, "mtbr3", MTBR3);
            NAMED(132, "mtbr4", MTBR4);
            NAMED(133, "mtbr5", MTBR5);
            NAMED(134, "mtbr6", MTBR6);
            NAMED(135, "mtbr7", MTBR7);
            NAMED(144, "mtbear", MTBEAR);
            NAMED(145, "mtbesr", MTBESR);
            NAMED(160, "mtiocr", MTIOCR);
            NAMED(192, "mtdmacr0", MTDMACR0);
            NAMED(193, "mtdmact0", MTDMACT0);
            NAMED(194, "mtdmada0", MTDMADA0);
            NAMED(195, "mtdmasa0", MTDMASA0);
            NAMED(196, "mtdmacc0", MTDMACC0);
            NAMED(200, "mtdmacr1", MTDMACR1);
            NAMED(201, "mtdmact1", MTDMACT1);
            NAMED(202, "mtdmada1", MTDMADA1);
            NAMED(203, "mtdmasa1", MTDMASA1);
            NAMED(204, "mtdmacc1", MTDMACC1);
            NAMED(208, "mtdmacr2", MTDMACR2);
            NAMED(209, "mtdmact2", MTDMACT2);
            NAMED(210, "mtdmada2", MTDMADA2);
            NAMED(211, "mtdmasa2", MTDMASA2);
            NAMED(212, "mtdmacc2", MTDMACC2);
            NAMED(216, "mtdmacr3", MTDMACR3);
            NAMED(217, "mtdmact3", MTDMACT3);
            NAMED(218, "mtdmada3", MTDMADA3);
            NAMED(219, "mtdmasa3", MTDMASA3);
            NAMED(220, "mtdmacc3", MTDMACC3);
            NAMED(224, "mtdmasr", MTDMASR);
        default: return nullptr;
        }
    }
#undef NAMED

    // mfspr, mtspr (XFX form) [B1 3.3.14, B3]: a named SPR, an SPRG (260-287, the number is SPRG's) or a BAT
    // (528-543, the number is the pair's), else the SPR number.
    bool MoveSpr(uint32_t w, Out& o, bool to)
    {
        const uint32_t spr = Spr(w), rt = Rt(w);
        if (const PpcOpcode* name = to ? MtsprName(spr) : MfsprName(spr))
            return o.Is(*name).Gpr(rt).Done();
        const PpcOpcode* special = nullptr;
        uint32_t number = 0;
        if (spr >= (to ? 272u : 260u) && spr <= 287)
        {
            special = to ? &OP("mtsprg", MTSPRG, 2) : &OP("mfsprg", MFSPRG, 2);
            number = spr & 7;
        }
        else if ((spr & 0x3F0) == 528)
        {
            static constexpr PpcOpcode bats[2][4] = { { INSN("mfibatu", MFIBATU, 2), INSN("mfibatl", MFIBATL, 2),
                                                          INSN("mfdbatu", MFDBATU, 2), INSN("mfdbatl", MFDBATL, 2) },
                { INSN("mtibatu", MTIBATU, 2), INSN("mtibatl", MTIBATL, 2), INSN("mtdbatu", MTDBATU, 2),
                    INSN("mtdbatl", MTDBATL, 2) } };
            special = &bats[to][(spr >> 2 & 2) | (spr & 1)];
            number = spr >> 1 & 3;
        }
        if (special != nullptr)
        {
            o.Is(*special);
            return to ? o.Number(number).Gpr(rt).Done() : o.Gpr(rt).Number(number).Done();
        }
        if (to)
            return o.Is(OP("mtspr", MTSPR, 2)).Number(spr).Gpr(rt).Done();
        return o.Is(OP("mfspr", MFSPR, 2)).Gpr(rt).Number(spr).Done();
    }

    // mfdcr, mtdcr (PPC4xx): a named DCR, else the DCR number.
    bool MoveDcr(uint32_t w, Out& o, bool to)
    {
        const uint32_t dcr = Spr(w), rt = Rt(w);
        if (const PpcOpcode* name = to ? MtdcrName(dcr) : MfdcrName(dcr))
            return o.Is(*name).Gpr(rt).Done();
        if (to)
            return o.Is(OP("mtdcr", MTDCR, 2)).Number(dcr).Gpr(rt).Done();
        return o.Is(OP("mfdcr", MFDCR, 2)).Gpr(rt).Number(dcr).Done();
    }

    bool OneField(uint32_t fxm) { return fxm != 0 && (fxm & (fxm - 1)) == 0; }

    // Primary opcode 31 [B1 3.3, 4.6, B2 3, B3, VMX 6, CBE; POWER and PPC4xx for the legacy ones], by its X-form
    // extended opcode (bits 21-30). XO forms (bits 22-30) appear twice, OE (bit 21) clear and set.
    bool Op31(uint32_t w, Out& o)
    {
        const uint32_t rt = Rt(w), ra = Ra(w), rb = Rb(w), rc = Rc(w), oe = Field(w, 21, 21);
        // XO-form arithmetic [B1 3.3.8]: "rT,rA,rB", unary ones "rT,rA" with RB zero.
        auto arith = [&](const PpcOpcode* ops) { return o.Is(ops[oe * 2 + rc]).Gpr(rt).Gpr(ra).Gpr(rb).Done(); };
        auto unary = [&](const PpcOpcode* ops) { return rb == 0 && o.Is(ops[oe * 2 + rc]).Gpr(rt).Gpr(ra).Done(); };
        // X-form logical and shifts [B1 3.3.11, 3.3.13]: "rA,rS,rB" and "rA,rS".
        auto logical = [&](const PpcOpcode* ops) { return o.Is(ops[rc]).Gpr(ra).Gpr(rt).Gpr(rb).Done(); };
        auto extend = [&](const PpcOpcode* ops) { return rb == 0 && o.Is(ops[rc]).Gpr(ra).Gpr(rt).Done(); };
        auto shift = [&](const PpcOpcode* ops) { return o.Is(ops[rc]).Gpr(ra).Gpr(rt).Number(rb).Done(); };
        switch (Xo(w))
        {
        // XO-form arithmetic.
        case 8:
        case 512 + 8: return arith(OPS(OE("subfc", SUBFC, 3)));
        case 9: return arith(OPS(RC("mulhdu", MULHDU, 3)));
        case 10:
        case 512 + 10: return arith(OPS(OE("addc", ADDC, 3)));
        case 11: return arith(OPS(RC("mulhwu", MULHWU, 3)));
        case 40:
        case 512 + 40: return arith(OPS(OE("subf", SUBF, 3)));
        case 73: return arith(OPS(RC("mulhd", MULHD, 3)));
        case 75: return arith(OPS(RC("mulhw", MULHW, 3)));
        case 104:
        case 512 + 104: return unary(OPS(OE("neg", NEG, 2)));
        case 136:
        case 512 + 136: return arith(OPS(OE("subfe", SUBFE, 3)));
        case 138:
        case 512 + 138: return arith(OPS(OE("adde", ADDE, 3)));
        case 200:
        case 512 + 200: return unary(OPS(OE("subfze", SUBFZE, 2)));
        case 202:
        case 512 + 202: return unary(OPS(OE("addze", ADDZE, 2)));
        case 232:
        case 512 + 232: return unary(OPS(OE("subfme", SUBFME, 2)));
        case 233:
        case 512 + 233: return arith(OPS(OE("mulld", MULLD, 3)));
        case 234:
        case 512 + 234: return unary(OPS(OE("addme", ADDME, 2)));
        case 235:
        case 512 + 235: return arith(OPS(OE("mullw", MULLW, 3)));
        case 266:
        case 512 + 266: return arith(OPS(OE("add", ADD, 3)));
        case 360:
        case 512 + 360: return unary(OPS(OE("abs", ABS, 2)));
        case 457:
        case 512 + 457: return arith(OPS(OE("divdu", DIVDU, 3)));
        case 459:
        case 512 + 459: return arith(OPS(OE("divwu", DIVWU, 3)));
        case 488:
        case 512 + 488: return unary(OPS(OE("nabs", NABS, 2)));
        case 489:
        case 512 + 489: return arith(OPS(OE("divd", DIVD, 3)));
        case 491:
        case 512 + 491: return arith(OPS(OE("divw", DIVW, 3)));
        // POWER's XO forms.
        case 107:
        case 512 + 107: return arith(OPS(OE("mul", MUL, 3)));
        case 264:
        case 512 + 264: return arith(OPS(OE("doz", DOZ, 3)));
        case 331:
        case 512 + 331: return arith(OPS(OE("div", DIV, 3)));
        case 363:
        case 512 + 363: return arith(OPS(OE("divs", DIVS, 3)));

        // Logical, shifts, extends.
        case 24: return logical(OPS(RC("slw", SLW, 3)));
        case 26: return extend(OPS(RC("cntlzw", CNTLZW, 2)));
        case 27: return logical(OPS(RC("sld", SLD, 3)));
        case 28: return logical(OPS(RC("and", AND, 3)));
        case 58: return extend(OPS(RC("cntlzd", CNTLZD, 2)));
        case 60: return logical(OPS(RC("andc", ANDC, 3)));
        case 124:
            if (rt == rb)
                return o.Is(OPS(RC("not", NOT, 3))[rc]).Gpr(ra).Gpr(rt).Skip(0).Done();
            return logical(OPS(RC("nor", NOR, 3)));
        case 284: return logical(OPS(RC("eqv", EQV, 3)));
        case 316: return logical(OPS(RC("xor", XOR, 3)));
        case 412: return logical(OPS(RC("orc", ORC, 3)));
        case 444:
            // Cell thread priority (or 1,1,1 ...) and delay (or 28,28,28 ...) forms [CBE], then mr.
            if (rt == ra && ra == rb && rc == 0)
            {
                switch (rt)
                {
                case 1: return o.Is(OP("cctpl", CCTPL, 0)).Done();
                case 2: return o.Is(OP("cctpm", CCTPM, 0)).Done();
                case 3: return o.Is(OP("cctph", CCTPH, 0)).Done();
                case 28: return o.Is(OP("db8cyc", DB8CYC, 0)).Done();
                case 29: return o.Is(OP("db10cyc", DB10CYC, 0)).Done();
                case 30: return o.Is(OP("db12cyc", DB12CYC, 0)).Done();
                case 31: return o.Is(OP("db16cyc", DB16CYC, 0)).Done();
                default: break;
                }
            }
            if (rt == rb)
                return o.Is(OPS(RC("mr", MR, 3))[rc]).Gpr(ra).Gpr(rt).Skip(0).Done();
            return logical(OPS(RC("or", OR, 3)));
        case 476: return logical(OPS(RC("nand", NAND, 3)));
        case 536: return logical(OPS(RC("srw", SRW, 3)));
        case 539: return logical(OPS(RC("srd", SRD, 3)));
        case 792: return logical(OPS(RC("sraw", SRAW, 3)));
        case 794: return logical(OPS(RC("srad", SRAD, 3)));
        case 824: return shift(OPS(RC("srawi", SRAWI, 3)));
        case 826:
        case 827: // sradi, XS form: sh[5] in bit 30 [B1 3.3.13.2]
            return o.Is(OPS(RC("sradi", SRADI, 3))[rc]).Gpr(ra).Gpr(rt).Number(rb | Field(w, 30, 30) << 5).Done();
        case 922: return extend(OPS(RC("extsh", EXTSH, 2)));
        case 954: return extend(OPS(RC("extsb", EXTSB, 2)));
        case 986: return extend(OPS(RC("extsw", EXTSW, 2)));
        // POWER's logical and shifts.
        case 29: return logical(OPS(RC("maskg", MASKG, 3)));
        case 78: return logical(OPS(RC("dlmzb", DLMZB, 3))); // PPC405
        case 152: return logical(OPS(RC("slq", SLQ, 3)));
        case 153: return logical(OPS(RC("sle", SLE, 3)));
        case 184: return shift(OPS(RC("sliq", SLIQ, 3)));
        case 216: return logical(OPS(RC("sllq", SLLQ, 3)));
        case 217: return logical(OPS(RC("sleq", SLEQ, 3)));
        case 248: return shift(OPS(RC("slliq", SLLIQ, 3)));
        case 277: return o.Is(OPS(RC("lscbx", LSCBX, 3))[rc]).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 537: return logical(OPS(RC("rrib", RRIB, 3)));
        case 541: return logical(OPS(RC("maskir", MASKIR, 3)));
        case 664: return logical(OPS(RC("srq", SRQ, 3)));
        case 665: return logical(OPS(RC("sre", SRE, 3)));
        case 696: return shift(OPS(RC("sriq", SRIQ, 3)));
        case 728: return logical(OPS(RC("srlq", SRLQ, 3)));
        case 729: return logical(OPS(RC("sreq", SREQ, 3)));
        case 760: return shift(OPS(RC("srliq", SRLIQ, 3)));
        case 920: return logical(OPS(RC("sraq", SRAQ, 3)));
        case 921: return logical(OPS(RC("srea", SREA, 3)));
        case 952: return shift(OPS(RC("sraiq", SRAIQ, 3)));

        // Load and reserve, store conditional [B2 3.3.2]: lwarx/ldarx's EH hint (bit 31) is optional.
        case 20: return o.Is(OP("lwarx", LWARX, 4)).Gpr(rt).GprOr0(ra).Gpr(rb).OptionalNumber(rc).Done();
        case 84: return o.Is(OP("ldarx", LDARX, 4)).Gpr(rt).GprOr0(ra).Gpr(rb).OptionalNumber(rc).Done();
        case 150: return rc == 1 && o.Is(OP("stwcx.", STWCX, 3)).Gpr(rt).GprOr0(ra).Gpr(rb).Done();
        case 214: return rc == 1 && o.Is(OP("stdcx.", STDCX, 3)).Gpr(rt).GprOr0(ra).Gpr(rb).Done();

        // VMX data stream touch [VMX 6]: T (bit 6) selects the transient forms; dss's A (bit 6) stops all.
        case 342:
            o.Is(Field(w, 6, 6) ? OP("dstt", DSTT, 3) : OP("dst", DST, 3));
            return o.Gpr(ra).Gpr(rb).Number(Field(w, 9, 10)).Done();
        case 374:
            o.Is(Field(w, 6, 6) ? OP("dststt", DSTSTT, 3) : OP("dstst", DSTST, 3));
            return o.Gpr(ra).Gpr(rb).Number(Field(w, 9, 10)).Done();
        case 822:
            if (Field(w, 6, 6))
                return o.Is(OP("dssall", DSSALL, 0)).Done();
            return o.Is(OP("dss", DSS, 1)).Number(Field(w, 9, 10)).Done();

        case 914: // tlbsx (PPC4xx): RT is optional
            return o.Is(OPS(RC("tlbsx", TLBSX, 3))[rc]).OptionalGpr(rt).Gpr(ra).Gpr(rb).Done();
        default: break;
        }
        // The rest have Rc reserved (zero).
        if (rc != 0)
            return false;
        // Indexed loads and stores [B1 3.3.2-3.3.6, 4.6; VMX 6; CBE]: "rT,rA|0,rB".
        auto indexed = [&](const PpcOpcode& op) { return o.Is(op).Gpr(rt).GprOr0(ra).Gpr(rb).Done(); };
        auto indexedFloat = [&](const PpcOpcode& op) { return o.Is(op).Fpr(rt).GprOr0(ra).Gpr(rb).Done(); };
        // VMX's register base is printed as a register; Cell's lvlx family takes rA|0.
        auto indexedVector = [&](const PpcOpcode& op) { return o.Is(op).Vr(rt).Gpr(ra).Gpr(rb).Done(); };
        auto indexedCell = [&](const PpcOpcode& op) { return o.Is(op).Vr(rt).GprOr0(ra).Gpr(rb).Done(); };
        // Cache management "rA,rB" with RT zero [B2 3.2].
        auto cache = [&](const PpcOpcode& op) { return rt == 0 && o.Is(op).Gpr(ra).Gpr(rb).Done(); };
        switch (Xo(w))
        {
        case 0: // cmp: L in bit 10 [B1 3.3.9]
            if (Field(w, 9, 9) != 0)
                return false;
            o.Is(Field(w, 10, 10) ? OP("cmpd", CMPD, 3) : OP("cmpw", CMPW, 3));
            return o.OptionalCr(Field(w, 6, 8)).Gpr(ra).Gpr(rb).Done();
        case 32:
            if (Field(w, 9, 9) != 0)
                return false;
            o.Is(Field(w, 10, 10) ? OP("cmpld", CMPLD, 3) : OP("cmplw", CMPLW, 3));
            return o.OptionalCr(Field(w, 6, 8)).Gpr(ra).Gpr(rb).Done();
        case 4: return Trap(w, o, TrapWord);
        case 68: return Trap(w, o, TrapDouble);
        case 122: return rb == 0 && o.Is(OP("popcntb", POPCNTB, 2)).Gpr(ra).Gpr(rt).Done();

        case 21: return indexed(OP("ldx", LDX, 3));
        case 23: return indexed(OP("lwzx", LWZX, 3));
        case 53: return indexed(OP("ldux", LDUX, 3));
        case 55: return indexed(OP("lwzux", LWZUX, 3));
        case 87: return indexed(OP("lbzx", LBZX, 3));
        case 119: return indexed(OP("lbzux", LBZUX, 3));
        case 149: return indexed(OP("stdx", STDX, 3));
        case 151: return indexed(OP("stwx", STWX, 3));
        case 181: return indexed(OP("stdux", STDUX, 3));
        case 183: return indexed(OP("stwux", STWUX, 3));
        case 215: return indexed(OP("stbx", STBX, 3));
        case 247: return indexed(OP("stbux", STBUX, 3));
        case 279: return indexed(OP("lhzx", LHZX, 3));
        case 310: return o.Is(OP("eciwx", ECIWX, 3)).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 311: return indexed(OP("lhzux", LHZUX, 3));
        case 341: return indexed(OP("lwax", LWAX, 3));
        case 343: return indexed(OP("lhax", LHAX, 3));
        case 373: return indexed(OP("lwaux", LWAUX, 3));
        case 375: return indexed(OP("lhaux", LHAUX, 3));
        case 407: return indexed(OP("sthx", STHX, 3));
        case 438: return o.Is(OP("ecowx", ECOWX, 3)).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 439: return indexed(OP("sthux", STHUX, 3));
        case 532: return indexed(OP("ldbrx", LDBRX, 3));
        case 533: return indexed(OP("lswx", LSWX, 3));
        case 534: return indexed(OP("lwbrx", LWBRX, 3));
        case 535: return indexedFloat(OP("lfsx", LFSX, 3));
        case 567: return indexedFloat(OP("lfsux", LFSUX, 3));
        case 599: return indexedFloat(OP("lfdx", LFDX, 3));
        case 631: return indexedFloat(OP("lfdux", LFDUX, 3));
        case 660: return indexed(OP("stdbrx", STDBRX, 3));
        case 661: return indexed(OP("stswx", STSWX, 3));
        case 662: return indexed(OP("stwbrx", STWBRX, 3));
        case 663: return indexedFloat(OP("stfsx", STFSX, 3));
        case 695: return indexedFloat(OP("stfsux", STFSUX, 3));
        case 727: return indexedFloat(OP("stfdx", STFDX, 3));
        case 759: return indexedFloat(OP("stfdux", STFDUX, 3));
        case 790: return indexed(OP("lhbrx", LHBRX, 3));
        case 918: return indexed(OP("sthbrx", STHBRX, 3));
        case 983: return indexedFloat(OP("stfiwx", STFIWX, 3));
        case 6: return indexedVector(OP("lvsl", LVSL, 3));
        case 7: return indexedVector(OP("lvebx", LVEBX, 3));
        case 38: return indexedVector(OP("lvsr", LVSR, 3));
        case 39: return indexedVector(OP("lvehx", LVEHX, 3));
        case 71: return indexedVector(OP("lvewx", LVEWX, 3));
        case 103: return indexedVector(OP("lvx", LVX, 3));
        case 135: return indexedVector(OP("stvebx", STVEBX, 3));
        case 167: return indexedVector(OP("stvehx", STVEHX, 3));
        case 199: return indexedVector(OP("stvewx", STVEWX, 3));
        case 231: return indexedVector(OP("stvx", STVX, 3));
        case 359: return indexedVector(OP("lvxl", LVXL, 3));
        case 487: return indexedVector(OP("stvxl", STVXL, 3));
        case 519: return indexedCell(OP("lvlx", LVLX, 3));
        case 551: return indexedCell(OP("lvrx", LVRX, 3));
        case 647: return indexedCell(OP("stvlx", STVLX, 3));
        case 679: return indexedCell(OP("stvrx", STVRX, 3));
        case 775: return indexedCell(OP("lvlxl", LVLXL, 3));
        case 807: return indexedCell(OP("lvrxl", LVRXL, 3));
        case 903: return indexedCell(OP("stvlxl", STVLXL, 3));
        case 935: return indexedCell(OP("stvrxl", STVRXL, 3));
        case 597: return o.Is(OP("lswi", LSWI, 3)).Gpr(rt).GprOr0(ra).Number(rb == 0 ? 32 : rb).Done();
        case 725: return o.Is(OP("stswi", STSWI, 3)).Gpr(rt).GprOr0(ra).Number(rb == 0 ? 32 : rb).Done();

        // Cache and storage control [B2 3].
        case 54: return cache(OP("dcbst", DCBST, 2));
        case 86: // dcbf: L in bit 10, dcbfl when L=1 alone
            if (rt == 1)
                return o.Is(OP("dcbfl", DCBFL, 2)).Gpr(ra).Gpr(rb).Done();
            return Field(w, 6, 8) == 0 &&
                   o.Is(OP("dcbf", DCBF, 3)).Gpr(ra).Gpr(rb).OptionalNumber(Field(w, 10, 10)).Done();
        case 246: return o.Is(OP("dcbtst", DCBTST, 3)).OptionalNumber(rt).Gpr(ra).Gpr(rb).Done();
        case 278: return o.Is(OP("dcbt", DCBT, 3)).OptionalNumber(rt).Gpr(ra).Gpr(rb).Done();
        case 470: return cache(OP("dcbi", DCBI, 2));
        case 630: return rb == 0 && o.Is(OP("dclst", DCLST, 2)).Gpr(rt).Gpr(ra).Done();
        case 758: return cache(OP("dcba", DCBA, 2));
        case 982: return cache(OP("icbi", ICBI, 2));
        case 1014: // dcbz; Cell's dcbzl has bit 10 set [CBE]
            if (rt == 1)
                return o.Is(OP("dcbzl", DCBZL, 2)).Gpr(ra).Gpr(rb).Done();
            return cache(OP("dcbz", DCBZ, 2));
        case 598: // sync: L in bits 9-10, lwsync (1) and ptesync (2) by name
            if (Field(w, 6, 8) != 0 || ra != 0 || rb != 0)
                return false;
            switch (Field(w, 9, 10))
            {
            case 1: return o.Is(OP("lwsync", LWSYNC, 0)).Done();
            case 2: return o.Is(OP("ptesync", PTESYNC, 0)).Done();
            default: return o.Is(OP("sync", SYNC, 1)).OptionalNumber(Field(w, 9, 10)).Done();
            }
        case 262: return cache(OP("icbt", ICBT, 2));
        case 854: return w == 0x7C0006AC && o.Is(OP("eieio", EIEIO, 0)).Done();

        // Condition register and machine state [B1 3.3.14, B3]. FXM in bits 12-19; bit 11 selects the one-field
        // forms (mfocrf, mtocrf).
        case 19:
            if (Field(w, 11, 20) == 0)
                return o.Is(OP("mfcr", MFCR, 1)).Gpr(rt).Done();
            return Field(w, 11, 11) == 1 && Field(w, 20, 20) == 0 && OneField(Field(w, 12, 19)) &&
                   o.Is(OP("mfocrf", MFOCRF, 2)).Gpr(rt).Number(Field(w, 12, 19)).Done();
        case 144:
            if (Field(w, 20, 20) != 0)
                return false;
            if (Field(w, 11, 11) == 1)
                return OneField(Field(w, 12, 19)) &&
                       o.Is(OP("mtocrf", MTOCRF, 2)).Number(Field(w, 12, 19)).Gpr(rt).Done();
            if (Field(w, 12, 19) == 0xFF)
                return o.Is(OP("mtcr", MTCR, 1)).Gpr(rt).Done();
            return o.Is(OP("mtcrf", MTCRF, 2)).Number(Field(w, 12, 19)).Gpr(rt).Done();
        case 83: return ra == 0 && rb == 0 && o.Is(OP("mfmsr", MFMSR, 1)).Gpr(rt).Done();
        case 146: return ra == 0 && rb == 0 && o.Is(OP("mtmsr", MTMSR, 1)).Gpr(rt).Done();
        case 178: // mtmsrd: L in bit 15
            return Field(w, 11, 14) == 0 && rb == 0 &&
                   o.Is(OP("mtmsrd", MTMSRD, 2)).Gpr(rt).OptionalNumber(Field(w, 15, 15)).Done();
        case 210: // mtsr: SR in bits 12-15
            return Field(w, 11, 11) == 0 && rb == 0 &&
                   o.Is(OP("mtsr", MTSR, 2)).Number(Field(w, 12, 15)).Gpr(rt).Done();
        case 82: // mtsrd (64-bit bridge)
            return Field(w, 11, 11) == 0 && rb == 0 &&
                   o.Is(OP("mtsrd", MTSRD, 2)).Number(Field(w, 12, 15)).Gpr(rt).Done();
        case 595:
            return Field(w, 11, 11) == 0 && rb == 0 &&
                   o.Is(OP("mfsr", MFSR, 2)).Gpr(rt).Number(Field(w, 12, 15)).Done();
        case 242: return ra == 0 && o.Is(OP("mtsrin", MTSRIN, 2)).Gpr(rt).Gpr(rb).Done();
        case 659: return ra == 0 && o.Is(OP("mfsrin", MFSRIN, 2)).Gpr(rt).Gpr(rb).Done();
        case 114: return ra == 0 && o.Is(OP("mtsrdin", MTSRDIN, 2)).Gpr(rt).Gpr(rb).Done();
        case 512: return Field(w, 9, 20) == 0 && o.Is(OP("mcrxr", MCRXR, 1)).Cr(Field(w, 6, 8)).Done();
        case 371: // mftb: the TBR is left out when it is 268 (TB)
            return o.Is(OP("mftb", MFTB, 2)).Gpr(rt).OptionalNumber(Spr(w) == 268 ? 0 : Spr(w)).Done();
        case 274: // tlbiel, tlbie: L in bit 10
            return Field(w, 6, 9) == 0 && ra == 0 &&
                   o.Is(OP("tlbiel", TLBIEL, 2)).Gpr(rb).OptionalNumber(Field(w, 10, 10)).Done();
        case 306:
            return Field(w, 6, 9) == 0 && ra == 0 &&
                   o.Is(OP("tlbie", TLBIE, 2)).Gpr(rb).OptionalNumber(Field(w, 10, 10)).Done();
        case 370: return w == 0x7C0002E4 && o.Is(OP("tlbia", TLBIA, 0)).Done();
        case 402: return ra == 0 && o.Is(OP("slbmte", SLBMTE, 2)).Gpr(rt).Gpr(rb).Done();
        case 434: return rt == 0 && ra == 0 && o.Is(OP("slbie", SLBIE, 1)).Gpr(rb).Done();
        case 498: return w == 0x7C0003E4 && o.Is(OP("slbia", SLBIA, 0)).Done();
        case 566: return w == 0x7C00046C && o.Is(OP("tlbsync", TLBSYNC, 0)).Done();
        case 851: return ra == 0 && o.Is(OP("slbmfev", SLBMFEV, 2)).Gpr(rt).Gpr(rb).Done();
        case 915: return ra == 0 && o.Is(OP("slbmfee", SLBMFEE, 2)).Gpr(rt).Gpr(rb).Done();
        case 339: return MoveSpr(w, o, false);
        case 467: return MoveSpr(w, o, true);

        // POWER and PPC4xx system instructions.
        case 323: return MoveDcr(w, o, false);
        case 451: return MoveDcr(w, o, true);
        case 818: return o.Is(OP("rac", RAC, 3)).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 627: return o.Is(OP("mfsri", MFSRI, 3)).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 531: return rb == 0 && o.Is(OP("clcs", CLCS, 2)).Gpr(rt).Gpr(ra).Done();
        case 131: return ra == 0 && rb == 0 && o.Is(OP("wrtee", WRTEE, 1)).Gpr(rt).Done();
        case 163: // wrteei: E in bit 16, everything else fixed
            return (w & ~0x8000u) == 0x7C000146 && o.Is(OP("wrteei", WRTEEI, 1)).Number(Field(w, 16, 16)).Done();
        case 454: return cache(OP("dccci", DCCCI, 2));
        case 966: return cache(OP("iccci", ICCCI, 2));
        case 998: return cache(OP("icread", ICREAD, 2));
        case 1010: return rt == 0 && ra == 0 && o.Is(OP("tlbli", TLBLI, 1)).Gpr(rb).Done();
        case 486: return o.Is(OP("dcread", DCREAD, 3)).Gpr(rt).Gpr(ra).Gpr(rb).Done();
        case 946: // tlbre: WS 0 and 1 by name
            if (rb <= 1)
                return o.Is(rb ? OP("tlbrelo", TLBRELO, 2) : OP("tlbrehi", TLBREHI, 2)).Gpr(rt).Gpr(ra).Done();
            return o.Is(OP("tlbre", TLBRE, 3)).Gpr(rt).Gpr(ra).Number(rb).Done();
        case 978:
            if (rb <= 1)
                return o.Is(rb ? OP("tlbwelo", TLBWELO, 2) : OP("tlbwehi", TLBWEHI, 2)).Gpr(rt).Gpr(ra).Done();
            return o.Is(OP("tlbwe", TLBWE, 3)).Gpr(rt).Gpr(ra).Number(rb).Done();
        default: return false;
        }
    }

    // Floating-point arithmetic, A form, primary opcodes 59 (single) and 63 (double) [B1 4.6.6-4.6.8]: xo in bits
    // 26-30; the field an instruction does not use must be zero.
    bool FloatArithmetic(uint32_t w, Out& o, bool single)
    {
        const uint32_t t = Rt(w), a = Ra(w), b = Rb(w), c = Field(w, 21, 25), rc = Rc(w);
        auto ab = [&](const PpcOpcode* ops) { return c == 0 && o.Is(ops[rc]).Fpr(t).Fpr(a).Fpr(b).Done(); };
        auto ac = [&](const PpcOpcode* ops) { return b == 0 && o.Is(ops[rc]).Fpr(t).Fpr(a).Fpr(c).Done(); };
        auto acb = [&](const PpcOpcode* ops) { return o.Is(ops[rc]).Fpr(t).Fpr(a).Fpr(c).Fpr(b).Done(); };
        auto root = [&](const PpcOpcode* ops) { return a == 0 && c == 0 && o.Is(ops[rc]).Fpr(t).Fpr(b).Done(); };
        // The estimates: an optional field in bit 15.
        auto estimate = [&](const PpcOpcode* ops) {
            return Field(w, 11, 14) == 0 && c == 0 &&
                   o.Is(ops[rc]).Fpr(t).Fpr(b).OptionalNumber(Field(w, 15, 15)).Done();
        };
        switch (Field(w, 26, 30))
        {
        case 18: return ab(single ? OPS(RC("fdivs", FDIVS, 3)) : OPS(RC("fdiv", FDIV, 3)));
        case 20: return ab(single ? OPS(RC("fsubs", FSUBS, 3)) : OPS(RC("fsub", FSUB, 3)));
        case 21: return ab(single ? OPS(RC("fadds", FADDS, 3)) : OPS(RC("fadd", FADD, 3)));
        case 22: return root(single ? OPS(RC("fsqrts", FSQRTS, 2)) : OPS(RC("fsqrt", FSQRT, 2)));
        case 23: return !single && acb(OPS(RC("fsel", FSEL, 4)));
        case 24: return estimate(single ? OPS(RC("fres", FRES, 3)) : OPS(RC("fre", FRE, 3)));
        case 25: return ac(single ? OPS(RC("fmuls", FMULS, 3)) : OPS(RC("fmul", FMUL, 3)));
        case 26: return estimate(single ? OPS(RC("frsqrtes", FRSQRTES, 3)) : OPS(RC("frsqrte", FRSQRTE, 3)));
        case 28: return acb(single ? OPS(RC("fmsubs", FMSUBS, 4)) : OPS(RC("fmsub", FMSUB, 4)));
        case 29: return acb(single ? OPS(RC("fmadds", FMADDS, 4)) : OPS(RC("fmadd", FMADD, 4)));
        case 30: return acb(single ? OPS(RC("fnmsubs", FNMSUBS, 4)) : OPS(RC("fnmsub", FNMSUB, 4)));
        case 31: return acb(single ? OPS(RC("fnmadds", FNMADDS, 4)) : OPS(RC("fnmadd", FNMADD, 4)));
        default: return false;
        }
    }

    // Primary opcode 63: the A forms, then the X forms by bits 21-30 [B1 4.6].
    bool Op63(uint32_t w, Out& o)
    {
        if (FloatArithmetic(w, o, false))
            return true;
        const uint32_t t = Rt(w), a = Ra(w), b = Rb(w), rc = Rc(w);
        auto unary = [&](const PpcOpcode* ops) { return a == 0 && o.Is(ops[rc]).Fpr(t).Fpr(b).Done(); };
        switch (Xo(w))
        {
        case 12: return unary(OPS(RC("frsp", FRSP, 2)));
        case 14: return unary(OPS(RC("fctiw", FCTIW, 2)));
        case 15: return unary(OPS(RC("fctiwz", FCTIWZ, 2)));
        case 40: return unary(OPS(RC("fneg", FNEG, 2)));
        case 72: return unary(OPS(RC("fmr", FMR, 2)));
        case 136: return unary(OPS(RC("fnabs", FNABS, 2)));
        case 264: return unary(OPS(RC("fabs", FABS, 2)));
        case 392: return unary(OPS(RC("frin", FRIN, 2)));
        case 424: return unary(OPS(RC("friz", FRIZ, 2)));
        case 456: return unary(OPS(RC("frip", FRIP, 2)));
        case 488: return unary(OPS(RC("frim", FRIM, 2)));
        case 814: return unary(OPS(RC("fctid", FCTID, 2)));
        case 815: return unary(OPS(RC("fctidz", FCTIDZ, 2)));
        case 846: return unary(OPS(RC("fcfid", FCFID, 2)));
        case 0:
            return rc == 0 && Field(w, 9, 10) == 0 &&
                   o.Is(OP("fcmpu", FCMPU, 3)).Cr(Field(w, 6, 8)).Fpr(a).Fpr(b).Done();
        case 32:
            return rc == 0 && Field(w, 9, 10) == 0 &&
                   o.Is(OP("fcmpo", FCMPO, 3)).Cr(Field(w, 6, 8)).Fpr(a).Fpr(b).Done();
        case 38: return a == 0 && b == 0 && o.Is(OPS(RC("mtfsb1", MTFSB1, 1))[rc]).CrBit(t).Done();
        case 70: return a == 0 && b == 0 && o.Is(OPS(RC("mtfsb0", MTFSB0, 1))[rc]).CrBit(t).Done();
        case 64:
            return rc == 0 && Field(w, 9, 10) == 0 && Field(w, 14, 20) == 0 &&
                   o.Is(OP("mcrfs", MCRFS, 2)).Cr(Field(w, 6, 8)).Cr(Field(w, 11, 13)).Done();
        case 134: // mtfsfi: BF, U (bits 16-19), W (bit 15, optional)
            return Field(w, 9, 14) == 0 && Field(w, 20, 20) == 0 &&
                   o.Is(OPS(RC("mtfsfi", MTFSFI, 3))[rc])
                       .Number(Field(w, 6, 8))
                       .Number(Field(w, 16, 19))
                       .OptionalNumber(Field(w, 15, 15))
                       .Done();
        case 583: return a == 0 && b == 0 && o.Is(OPS(RC("mffs", MFFS, 1))[rc]).Fpr(t).Done();
        case 711:
        {
            // mtfsf: FLM (bits 7-14), FRB, then L (bit 6) and W (bit 15) [ISA 2.05 fields], both printed when
            // either is set.
            const uint32_t l = Field(w, 6, 6), wide = Field(w, 15, 15);
            o.Is(OPS(RC("mtfsf", MTFSF, 4))[rc]).Number(Field(w, 7, 14)).Fpr(b);
            if (l == 0 && wide == 0)
                return o.Skip(0).Skip(0).Done();
            return o.Number(l).Number(wide).Done();
        }
        default: return false;
        }
    }

    // VMX128 loads and stores, primary opcode 4 [X128]: bits 30-31 set, bits 26-27 clear, xo in bits 21-25; VD's
    // high bits in 28-29.
    bool Vmx128Memory(uint32_t w, Out& o)
    {
        auto memory = [&](const PpcOpcode& op) { return o.Is(op).Vr(Vd128(w)).Gpr(Ra(w)).Gpr(Rb(w)).Done(); };
        switch (Field(w, 21, 25))
        {
        case 0x00: return memory(OP("lvsl128", LVSL128, 3));
        case 0x01: return memory(OP("lvsr128", LVSR128, 3));
        case 0x02: return memory(OP("lvewx128", LVEWX128, 3));
        case 0x03: return memory(OP("lvx128", LVX128, 3));
        case 0x06: return memory(OP("stvewx128", STVEWX128, 3));
        case 0x07: return memory(OP("stvx128", STVX128, 3));
        case 0x0B: return memory(OP("lvxl128", LVXL128, 3));
        case 0x0F: return memory(OP("stvxl128", STVXL128, 3));
        case 0x10: return memory(OP("lvlx128", LVLX128, 3));
        case 0x11: return memory(OP("lvrx128", LVRX128, 3));
        case 0x14: return memory(OP("stvlx128", STVLX128, 3));
        case 0x15: return memory(OP("stvrx128", STVRX128, 3));
        case 0x18: return memory(OP("lvlxl128", LVLXL128, 3));
        case 0x19: return memory(OP("lvrxl128", LVRXL128, 3));
        case 0x1C: return memory(OP("stvlxl128", STVLXL128, 3));
        case 0x1D: return memory(OP("stvrxl128", STVRXL128, 3));
        default: return false;
        }
    }

    // VMX VX and VC forms, primary opcode 4 [VMX 6]: xo in bits 21-31 (VC: Rc in bit 21).
    bool VmxVx(uint32_t w, Out& o)
    {
        const uint32_t d = Rt(w), a = Ra(w), b = Rb(w);
        auto vab = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(b).Done(); };
        auto vb = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(b).Done(); };
        auto vbu = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(b).Number(a).Done(); };
        auto vs = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Signed(SignExtend(a, 5)).Done(); };
        auto compare = [&](const PpcOpcode* ops) { return o.Is(ops[Field(w, 21, 21)]).Vr(d).Vr(a).Vr(b).Done(); };
        switch (w & 0x7FF)
        {
        case 0: return vab(OP("vaddubm", VADDUBM, 3));
        case 2: return vab(OP("vmaxub", VMAXUB, 3));
        case 4: return vab(OP("vrlb", VRLB, 3));
        case 8: return vab(OP("vmuloub", VMULOUB, 3));
        case 10: return vab(OP("vaddfp", VADDFP, 3));
        case 12: return vab(OP("vmrghb", VMRGHB, 3));
        case 14: return vab(OP("vpkuhum", VPKUHUM, 3));
        case 64: return vab(OP("vadduhm", VADDUHM, 3));
        case 66: return vab(OP("vmaxuh", VMAXUH, 3));
        case 68: return vab(OP("vrlh", VRLH, 3));
        case 72: return vab(OP("vmulouh", VMULOUH, 3));
        case 74: return vab(OP("vsubfp", VSUBFP, 3));
        case 76: return vab(OP("vmrghh", VMRGHH, 3));
        case 78: return vab(OP("vpkuwum", VPKUWUM, 3));
        case 128: return vab(OP("vadduwm", VADDUWM, 3));
        case 130: return vab(OP("vmaxuw", VMAXUW, 3));
        case 132: return vab(OP("vrlw", VRLW, 3));
        case 140: return vab(OP("vmrghw", VMRGHW, 3));
        case 142: return vab(OP("vpkuhus", VPKUHUS, 3));
        case 206: return vab(OP("vpkuwus", VPKUWUS, 3));
        case 258: return vab(OP("vmaxsb", VMAXSB, 3));
        case 260: return vab(OP("vslb", VSLB, 3));
        case 264: return vab(OP("vmulosb", VMULOSB, 3));
        case 266: return vb(OP("vrefp", VREFP, 2));
        case 268: return vab(OP("vmrglb", VMRGLB, 3));
        case 270: return vab(OP("vpkshus", VPKSHUS, 3));
        case 322: return vab(OP("vmaxsh", VMAXSH, 3));
        case 324: return vab(OP("vslh", VSLH, 3));
        case 328: return vab(OP("vmulosh", VMULOSH, 3));
        case 330: return vb(OP("vrsqrtefp", VRSQRTEFP, 2));
        case 332: return vab(OP("vmrglh", VMRGLH, 3));
        case 334: return vab(OP("vpkswus", VPKSWUS, 3));
        case 384: return vab(OP("vaddcuw", VADDCUW, 3));
        case 386: return vab(OP("vmaxsw", VMAXSW, 3));
        case 388: return vab(OP("vslw", VSLW, 3));
        case 394: return vb(OP("vexptefp", VEXPTEFP, 2));
        case 396: return vab(OP("vmrglw", VMRGLW, 3));
        case 398: return vab(OP("vpkshss", VPKSHSS, 3));
        case 452: return vab(OP("vsl", VSL, 3));
        case 458: return vb(OP("vlogefp", VLOGEFP, 2));
        case 462: return vab(OP("vpkswss", VPKSWSS, 3));
        case 512: return vab(OP("vaddubs", VADDUBS, 3));
        case 514: return vab(OP("vminub", VMINUB, 3));
        case 516: return vab(OP("vsrb", VSRB, 3));
        case 520: return vab(OP("vmuleub", VMULEUB, 3));
        case 522: return vb(OP("vrfin", VRFIN, 2));
        case 524: return vbu(OP("vspltb", VSPLTB, 3));
        case 526: return vb(OP("vupkhsb", VUPKHSB, 2));
        case 576: return vab(OP("vadduhs", VADDUHS, 3));
        case 578: return vab(OP("vminuh", VMINUH, 3));
        case 580: return vab(OP("vsrh", VSRH, 3));
        case 584: return vab(OP("vmuleuh", VMULEUH, 3));
        case 586: return vb(OP("vrfiz", VRFIZ, 2));
        case 588: return vbu(OP("vsplth", VSPLTH, 3));
        case 590: return vb(OP("vupkhsh", VUPKHSH, 2));
        case 640: return vab(OP("vadduws", VADDUWS, 3));
        case 642: return vab(OP("vminuw", VMINUW, 3));
        case 644: return vab(OP("vsrw", VSRW, 3));
        case 650: return vb(OP("vrfip", VRFIP, 2));
        case 652: return vbu(OP("vspltw", VSPLTW, 3));
        case 654: return vb(OP("vupklsb", VUPKLSB, 2));
        case 708: return vab(OP("vsr", VSR, 3));
        case 714: return vb(OP("vrfim", VRFIM, 2));
        case 718: return vb(OP("vupklsh", VUPKLSH, 2));
        case 768: return vab(OP("vaddsbs", VADDSBS, 3));
        case 770: return vab(OP("vminsb", VMINSB, 3));
        case 772: return vab(OP("vsrab", VSRAB, 3));
        case 776: return vab(OP("vmulesb", VMULESB, 3));
        case 778: return vbu(OP("vcfux", VCFUX, 3));
        case 780: return vs(OP("vspltisb", VSPLTISB, 2));
        case 782: return vab(OP("vpkpx", VPKPX, 3));
        case 832: return vab(OP("vaddshs", VADDSHS, 3));
        case 834: return vab(OP("vminsh", VMINSH, 3));
        case 836: return vab(OP("vsrah", VSRAH, 3));
        case 840: return vab(OP("vmulesh", VMULESH, 3));
        case 842: return vbu(OP("vcfsx", VCFSX, 3));
        case 844: return vs(OP("vspltish", VSPLTISH, 2));
        case 846: return vb(OP("vupkhpx", VUPKHPX, 2));
        case 896: return vab(OP("vaddsws", VADDSWS, 3));
        case 898: return vab(OP("vminsw", VMINSW, 3));
        case 900: return vab(OP("vsraw", VSRAW, 3));
        case 906: return vbu(OP("vctuxs", VCTUXS, 3));
        case 908: return vs(OP("vspltisw", VSPLTISW, 2));
        case 970: return vbu(OP("vctsxs", VCTSXS, 3));
        case 974: return vb(OP("vupklpx", VUPKLPX, 2));
        case 1024: return vab(OP("vsububm", VSUBUBM, 3));
        case 1026: return vab(OP("vavgub", VAVGUB, 3));
        case 1028: return vab(OP("vand", VAND, 3));
        case 1034: return vab(OP("vmaxfp", VMAXFP, 3));
        case 1036: return vab(OP("vslo", VSLO, 3));
        case 1088: return vab(OP("vsubuhm", VSUBUHM, 3));
        case 1090: return vab(OP("vavguh", VAVGUH, 3));
        case 1092: return vab(OP("vandc", VANDC, 3));
        case 1098: return vab(OP("vminfp", VMINFP, 3));
        case 1100: return vab(OP("vsro", VSRO, 3));
        case 1152: return vab(OP("vsubuwm", VSUBUWM, 3));
        case 1154: return vab(OP("vavguw", VAVGUW, 3));
        case 1156: return vab(OP("vor", VOR, 3));
        case 1220: return vab(OP("vxor", VXOR, 3));
        case 1282: return vab(OP("vavgsb", VAVGSB, 3));
        case 1284: return vab(OP("vnor", VNOR, 3));
        case 1346: return vab(OP("vavgsh", VAVGSH, 3));
        case 1408: return vab(OP("vsubcuw", VSUBCUW, 3));
        case 1410: return vab(OP("vavgsw", VAVGSW, 3));
        case 1536: return vab(OP("vsububs", VSUBUBS, 3));
        case 1540: return o.Is(OP("mfvscr", MFVSCR, 1)).Vr(d).Done();
        case 1544: return vab(OP("vsum4ubs", VSUM4UBS, 3));
        case 1600: return vab(OP("vsubuhs", VSUBUHS, 3));
        case 1604: return o.Is(OP("mtvscr", MTVSCR, 1)).Vr(b).Done();
        case 1608: return vab(OP("vsum4shs", VSUM4SHS, 3));
        case 1664: return vab(OP("vsubuws", VSUBUWS, 3));
        case 1672: return vab(OP("vsum2sws", VSUM2SWS, 3));
        case 1792: return vab(OP("vsubsbs", VSUBSBS, 3));
        case 1800: return vab(OP("vsum4sbs", VSUM4SBS, 3));
        case 1856: return vab(OP("vsubshs", VSUBSHS, 3));
        case 1920: return vab(OP("vsubsws", VSUBSWS, 3));
        case 1928: return vab(OP("vsumsws", VSUMSWS, 3));
        // VC form.
        case 6:
        case 1024 + 6: return compare(OPS(RC("vcmpequb", VCMPEQUB, 3)));
        case 70:
        case 1024 + 70: return compare(OPS(RC("vcmpequh", VCMPEQUH, 3)));
        case 134:
        case 1024 + 134: return compare(OPS(RC("vcmpequw", VCMPEQUW, 3)));
        case 198:
        case 1024 + 198: return compare(OPS(RC("vcmpeqfp", VCMPEQFP, 3)));
        case 454:
        case 1024 + 454: return compare(OPS(RC("vcmpgefp", VCMPGEFP, 3)));
        case 518:
        case 1024 + 518: return compare(OPS(RC("vcmpgtub", VCMPGTUB, 3)));
        case 582:
        case 1024 + 582: return compare(OPS(RC("vcmpgtuh", VCMPGTUH, 3)));
        case 646:
        case 1024 + 646: return compare(OPS(RC("vcmpgtuw", VCMPGTUW, 3)));
        case 710:
        case 1024 + 710: return compare(OPS(RC("vcmpgtfp", VCMPGTFP, 3)));
        case 774:
        case 1024 + 774: return compare(OPS(RC("vcmpgtsb", VCMPGTSB, 3)));
        case 838:
        case 1024 + 838: return compare(OPS(RC("vcmpgtsh", VCMPGTSH, 3)));
        case 902:
        case 1024 + 902: return compare(OPS(RC("vcmpgtsw", VCMPGTSW, 3)));
        case 966:
        case 1024 + 966: return compare(OPS(RC("vcmpbfp", VCMPBFP, 3)));
        default: return false;
        }
    }

    // VMX VA form, primary opcode 4 [VMX 6]: xo in bits 26-31.
    bool VmxVa(uint32_t w, Out& o)
    {
        const uint32_t d = Rt(w), a = Ra(w), b = Rb(w), c = Field(w, 21, 25);
        auto vabc = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(b).Vr(c).Done(); };
        auto vacb = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(c).Vr(b).Done(); };
        switch (w & 0x3F)
        {
        case 32: return vabc(OP("vmhaddshs", VMHADDSHS, 4));
        case 33: return vabc(OP("vmhraddshs", VMHRADDSHS, 4));
        case 34: return vabc(OP("vmladduhm", VMLADDUHM, 4));
        case 36: return vabc(OP("vmsumubm", VMSUMUBM, 4));
        case 37: return vabc(OP("vmsummbm", VMSUMMBM, 4));
        case 38: return vabc(OP("vmsumuhm", VMSUMUHM, 4));
        case 39: return vabc(OP("vmsumuhs", VMSUMUHS, 4));
        case 40: return vabc(OP("vmsumshm", VMSUMSHM, 4));
        case 41: return vabc(OP("vmsumshs", VMSUMSHS, 4));
        case 42: return vabc(OP("vsel", VSEL, 4));
        case 43: return vabc(OP("vperm", VPERM, 4));
        case 44: return o.Is(OP("vsldoi", VSLDOI, 4)).Vr(d).Vr(a).Vr(b).Number(Field(w, 22, 25)).Done();
        case 46: return vacb(OP("vmaddfp", VMADDFP, 4));
        case 47: return vacb(OP("vnmsubfp", VNMSUBFP, 4));
        default: return false;
        }
    }

    // Primary opcode 4: VMX128's loads and stores, VMX, then VMX128's vsldoi128 (bit 27). The Xenon has no PPC405
    // multiply-accumulate forms: their encodings are vsldoi128's.
    bool Op4(uint32_t w, Out& o)
    {
        if ((w & 0x33) == 0x03 && Vmx128Memory(w, o))
            return true;
        if (VmxVx(w, o) || VmxVa(w, o))
            return true;
        return Field(w, 27, 27) != 0 && o.Is(OP("vsldoi128", VSLDOI128, 4))
                                            .Vr(Vd128(w))
                                            .Vr(Va128(w))
                                            .Vr(Vb128(w))
                                            .Number(Field(w, 22, 25))
                                            .Done();
    }

    // VMX128, primary opcode 5 [X128]: vperm128 (VC in bits 23-25) when bits 22 and 27 are clear, else xo in bits
    // 22-25 and 27.
    bool Op5(uint32_t w, Out& o)
    {
        const uint32_t d = Vd128(w), a = Va128(w), b = Vb128(w);
        if (Field(w, 22, 22) == 0 && Field(w, 27, 27) == 0)
            return o.Is(OP("vperm128", VPERM128, 4)).Vr(d).Vr(a).Vr(b).Vr(Field(w, 23, 25)).Done();
        auto vab = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(b).Done(); };
        // The multiply-adds accumulate into VD, printed again.
        auto vabd = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(b).Vr(d).Done(); };
        switch (w & 0x3D0)
        {
        case 0x010: return vab(OP("vaddfp128", VADDFP128, 3));
        case 0x050: return vab(OP("vsubfp128", VSUBFP128, 3));
        case 0x090: return vab(OP("vmulfp128", VMULFP128, 3));
        case 0x0D0: return vabd(OP("vmaddfp128", VMADDFP128, 4));
        case 0x110: return o.Is(OP("vmaddcfp128", VMADDCFP128, 4)).Vr(d).Vr(a).Vr(d).Vr(b).Done();
        case 0x150: return vabd(OP("vnmsubfp128", VNMSUBFP128, 4));
        case 0x190: return vab(OP("vmsum3fp128", VMSUM3FP128, 3));
        case 0x1D0: return vab(OP("vmsum4fp128", VMSUM4FP128, 3));
        case 0x200: return vab(OP("vpkshss128", VPKSHSS128, 3));
        case 0x210: return vab(OP("vand128", VAND128, 3));
        case 0x240: return vab(OP("vpkshus128", VPKSHUS128, 3));
        case 0x250: return vab(OP("vandc128", VANDC128, 3));
        case 0x280: return vab(OP("vpkswss128", VPKSWSS128, 3));
        case 0x290: return vab(OP("vnor128", VNOR128, 3));
        case 0x2C0: return vab(OP("vpkswus128", VPKSWUS128, 3));
        case 0x2D0: return vab(OP("vor128", VOR128, 3));
        case 0x300: return vab(OP("vpkuhum128", VPKUHUM128, 3));
        case 0x310: return vab(OP("vxor128", VXOR128, 3));
        case 0x340: return vab(OP("vpkuhus128", VPKUHUS128, 3));
        case 0x350: return vabd(OP("vsel128", VSEL128, 4));
        case 0x380: return vab(OP("vpkuwum128", VPKUWUM128, 3));
        case 0x390: return vab(OP("vslo128", VSLO128, 3));
        case 0x3C0: return vab(OP("vpkuwus128", VPKUWUS128, 3));
        case 0x3D0: return vab(OP("vsro128", VSRO128, 3));
        default: return false;
        }
    }

    // VMX128, primary opcode 6 [X128]: vpermwi128, the one-source forms (xo in bits 21-27), vpkd3d128 and vrlimi128
    // (bits 21-23, 26-27), then the two-source forms (bits 22-25, 27; the compares' Rc in bit 25).
    bool Op6(uint32_t w, Out& o)
    {
        const uint32_t d = Vd128(w), a = Va128(w), b = Vb128(w), imm = Field(w, 11, 15);
        if ((w & 0x630) == 0x210)
            return o.Is(OP("vpermwi128", VPERMWI128, 3)).Vr(d).Vr(b).Number(imm | Field(w, 23, 25) << 5).Done();
        auto vbi = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(b).Number(imm).Done(); };
        auto vb = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(b).Done(); };
        switch (w & 0x7F0)
        {
        case 0x230: return vbi(OP("vcfpsxws128", VCFPSXWS128, 3));
        case 0x270: return vbi(OP("vcfpuxws128", VCFPUXWS128, 3));
        case 0x2B0: return vbi(OP("vcsxwfp128", VCSXWFP128, 3));
        case 0x2F0: return vbi(OP("vcuxwfp128", VCUXWFP128, 3));
        case 0x330: return vb(OP("vrfim128", VRFIM128, 2));
        case 0x370: return vb(OP("vrfin128", VRFIN128, 2));
        case 0x3B0: return vb(OP("vrfip128", VRFIP128, 2));
        case 0x3F0: return vb(OP("vrfiz128", VRFIZ128, 2));
        case 0x630: return vb(OP("vrefp128", VREFP128, 2));
        case 0x670: return vb(OP("vrsqrtefp128", VRSQRTEFP128, 2));
        case 0x6B0: return vb(OP("vexptefp128", VEXPTEFP128, 2));
        case 0x6F0: return vb(OP("vlogefp128", VLOGEFP128, 2));
        case 0x730: return vbi(OP("vspltw128", VSPLTW128, 3));
        case 0x770: return o.Is(OP("vspltisw128", VSPLTISW128, 2)).Vr(d).Signed(SignExtend(imm, 5)).Done();
        case 0x7F0: return vbi(OP("vupkd3d128", VUPKD3D128, 3));
        default: break;
        }
        switch (w & 0x730)
        {
        case 0x610:
            return o.Is(OP("vpkd3d128", VPKD3D128, 5))
                .Vr(d)
                .Vr(b)
                .Number(Field(w, 11, 13))
                .Number(Field(w, 14, 15))
                .Number(Field(w, 24, 25))
                .Done();
        case 0x710: return o.Is(OP("vrlimi128", VRLIMI128, 4)).Vr(d).Vr(b).Number(imm).Number(Field(w, 24, 25)).Done();
        default: break;
        }
        auto vab = [&](const PpcOpcode& op) { return o.Is(op).Vr(d).Vr(a).Vr(b).Done(); };
        switch (w & 0x3D0)
        {
        case 0x000: return vab(OP("vcmpeqfp128", VCMPEQFP128, 3));
        case 0x040: return vab(OP("vcmpeqfp128.", VCMPEQFP128, 3));
        case 0x080: return vab(OP("vcmpgefp128", VCMPGEFP128, 3));
        case 0x0C0: return vab(OP("vcmpgefp128.", VCMPGEFP128, 3));
        case 0x100: return vab(OP("vcmpgtfp128", VCMPGTFP128, 3));
        case 0x140: return vab(OP("vcmpgtfp128.", VCMPGTFP128, 3));
        case 0x180: return vab(OP("vcmpbfp128", VCMPBFP128, 3));
        case 0x1C0: return vab(OP("vcmpbfp128.", VCMPBFP128, 3));
        case 0x200: return vab(OP("vcmpequw128", VCMPEQUW128, 3));
        case 0x240: return vab(OP("vcmpequw128.", VCMPEQUW128, 3));
        case 0x050: return vab(OP("vrlw128", VRLW128, 3));
        case 0x0D0: return vab(OP("vslw128", VSLW128, 3));
        case 0x150: return vab(OP("vsraw128", VSRAW128, 3));
        case 0x1D0: return vab(OP("vsrw128", VSRW128, 3));
        case 0x280: return vab(OP("vmaxfp128", VMAXFP128, 3));
        case 0x2C0: return vab(OP("vminfp128", VMINFP128, 3));
        case 0x300: return vab(OP("vmrghw128", VMRGHW128, 3));
        case 0x340: return vab(OP("vmrglw128", VMRGLW128, 3));
        case 0x380: return o.Is(OP("vupkhsb128", VUPKHSB128, 3)).Vr(d).Vr(b).Vr(a).Done();
        case 0x3C0: return o.Is(OP("vupklsb128", VUPKLSB128, 3)).Vr(d).Vr(b).Vr(a).Done();
        default: return false;
        }
    }

    // The primary opcode, bits 0-5 [B1 1.6, appendix "Opcode Maps"].
    bool Decode(uint32_t w, Out& o)
    {
        const uint32_t rt = Rt(w), ra = Ra(w);
        const int32_t si = Si(w);
        const uint32_t ui = w & 0xFFFF;
        switch (w >> 26)
        {
        case 0: // attn (Cell, POWER4 and later)
            return Xo(w) == 256 && Rc(w) == 0 && o.Is(OP("attn", ATTN, 0)).Done();
        case 2: return TrapImmediate(w, o, TrapDoubleImmediate);
        case 3: return TrapImmediate(w, o, TrapWordImmediate);
        case 4: return Op4(w, o);
        case 5: return Op5(w, o);
        case 6: return Op6(w, o);
        // D-form arithmetic and compares [B1 3.3.8, 3.3.9]; dozi is POWER's.
        case 7: return o.Is(OP("mulli", MULLI, 3)).Gpr(rt).Gpr(ra).Signed(si).Done();
        case 8: return o.Is(OP("subfic", SUBFIC, 3)).Gpr(rt).Gpr(ra).Signed(si).Done();
        case 9: return o.Is(OP("dozi", DOZI, 3)).Gpr(rt).Gpr(ra).Signed(si).Done();
        case 10: // L in bit 10; the CR field is left out when cr0
            o.Is(Field(w, 10, 10) ? OP("cmpldi", CMPLDI, 3) : OP("cmplwi", CMPLWI, 3));
            return o.OptionalCr(Field(w, 6, 8)).Gpr(ra).Number(ui).Done();
        case 11:
            o.Is(Field(w, 10, 10) ? OP("cmpdi", CMPDI, 3) : OP("cmpwi", CMPWI, 3));
            return o.OptionalCr(Field(w, 6, 8)).Gpr(ra).Signed(si).Done();
        case 12: return o.Is(OP("addic", ADDIC, 3)).Gpr(rt).Gpr(ra).Signed(si).Done();
        case 13: return o.Is(OP("addic.", ADDIC, 3)).Gpr(rt).Gpr(ra).Signed(si).Done();
        case 14:
            if (ra == 0)
                return o.Is(OP("li", LI, 2)).Gpr(rt).Signed(si).Done();
            return o.Is(OP("addi", ADDI, 3)).Gpr(rt).GprOr0(ra).Signed(si).Done();
        case 15:
            if (ra == 0)
                return o.Is(OP("lis", LIS, 2)).Gpr(rt).Signed(si).Done();
            return o.Is(OP("addis", ADDIS, 3)).Gpr(rt).GprOr0(ra).Signed(si).Done();
        case 16: return BranchConditional(w, o);
        case 17: // sc [B1 2.4.3]: LEV in bits 20-26
            return (w & 0x03FF0003) == 2 && o.Is(OP("sc", SC, 1)).OptionalNumber(Field(w, 20, 26)).Done();
        case 18: // b [B1 2.4.1]: LI in bits 6-29, AA, LK
        {
            static constexpr PpcOpcode b[4] = AALK("b", B, 1);
            const uint32_t target = uint32_t(SignExtend(w & 0x03FFFFFC, 26)) + (w & 2 ? 0 : o.Address());
            return o.Is(b[w & 3]).Target(target).Done();
        }
        case 19: return Op19(w, o);
        case 20:
        case 21:
        case 22:
        case 23: return RotateWord(w, o);
        // D-form logical [B1 3.3.11]: "rA,rS,UI"; ori 0,0,0 is nop.
        case 24:
            if (w == 0x60000000)
                return o.Is(OP("nop", NOP, 0)).Done();
            return o.Is(OP("ori", ORI, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 25: return o.Is(OP("oris", ORIS, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 26: return o.Is(OP("xori", XORI, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 27: return o.Is(OP("xoris", XORIS, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 28: return o.Is(OP("andi.", ANDI, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 29: return o.Is(OP("andis.", ANDIS, 3)).Gpr(ra).Gpr(rt).Number(ui).Done();
        case 30: return Op30(w, o);
        case 31: return Op31(w, o);
        case 56:
        case 58:
        case 62: return LoadStoreDouble(w, o);
        case 59: return FloatArithmetic(w, o, true);
        case 63: return Op63(w, o);
        default: return w >> 26 >= 32 && w >> 26 <= 55 && LoadStore(w, o);
        }
    }
} // namespace

int ppc::DecodeWord(uint32_t word, uint64_t address, ppc_insn& out)
{
    out.instruction = word;
    std::memset(out.operands, 0, sizeof(out.operands));
    Out o(out, address);
    if (!Decode(word, o))
        o.Long(word);
    return 4;
}
