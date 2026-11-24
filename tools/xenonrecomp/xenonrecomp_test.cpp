// Runs XenonRecomp's translation of invented samples (sample_gen.cpp) on the host against the PowerPC semantics.
#include <xenonrecomp_sample.h>
#include <cmath>
#include <cstdio>
#include <cstring>

static int checks = 0, failures = 0;
#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

// vslo/vsro with vD == vB: the octet count (bits 121:124 of vB, u8[0] in the reversed layout) is read before vD is
// written. Big-endian left is toward higher u8 indices here.
static void Shifts(PPCContext& ctx)
{
    for (int left = 0; left < 2; ++left)
    {
        for (uint32_t shift = 0; shift < 16; ++shift)
        {
            for (uint32_t i = 0; i < 16; ++i)
                ctx.v4.u8[i] = uint8_t(0x10 + i);
            ctx.v3 = {};
            ctx.v3.u8[0] = uint8_t(shift << 3);
            (left ? sample_vslo : sample_vsro)(ctx, nullptr);
            bool same = true;
            for (uint32_t i = 0; i < 16; ++i)
            {
                int from = left ? int(i) - int(shift) : int(i + shift);
                uint8_t expected = from >= 0 && from < 16 ? uint8_t(0x10 + from) : 0;
                same = same && ctx.v3.u8[i] == expected;
            }
            CHECK(same);
        }
    }
}

// The dot forms set CR6: lt when every element compares true, eq when none does. A stale CR6 (both set) must go.
static void Compare(PPCContext& ctx, PPCFunc* compare, uint32_t elementBytes, uint8_t greater, uint8_t lower)
{
    for (int pattern = 0; pattern < 3; ++pattern) // every element true, none, only the lowest element of each word
    {
        for (uint32_t i = 0; i < 16; i += elementBytes)
        {
            bool truth = pattern == 0 || (pattern == 2 && i % 4 == 0);
            for (uint32_t j = 0; j < elementBytes; ++j)
            {
                // Only the element's high byte decides; the others are equal on both sides.
                bool high = j == elementBytes - 1;
                ctx.v4.u8[i + j] = high ? (truth ? greater : lower) : 0x22;
                ctx.v5.u8[i + j] = high ? lower : 0x22;
            }
        }
        ctx.cr6.lt = 1;
        ctx.cr6.eq = 1;
        compare(ctx, nullptr);
        CHECK(ctx.cr6.lt == (pattern == 0));
        CHECK(ctx.cr6.eq == (pattern == 1));
    }
}

// vadduhs saturates each unsigned halfword at 0xFFFF.
static void AddSaturate(PPCContext& ctx)
{
    for (uint32_t i = 0; i < 8; ++i)
    {
        ctx.v4.u16[i] = uint16_t(i % 2 ? 0xFFF0 : i);
        ctx.v5.u16[i] = uint16_t(i % 2 ? 0x0020 : 2 * i);
    }
    sample_vadduhs(ctx, nullptr);
    for (uint32_t i = 0; i < 8; ++i)
        CHECK(ctx.v3.u16[i] == (i % 2 ? 0xFFFF : 3 * i));
}

// mulhd keeps the high 64 bits of the signed 128-bit product; mulhd. sets CR0 from the result. The cases where it
// differs from mulhdu: a negative factor.
static void MultiplyHigh(PPCContext& ctx)
{
    struct Case
    {
        int64_t a, b, high;
    };
    const Case cases[] = {
        { -3, int64_t(0x4000000000000000), -1 },                      // -0.75 * 2^64 rounds down to -1
        { INT64_MIN, 2, -1 },                                         // -2^64 exactly
        { INT64_MAX, INT64_MAX, int64_t(0x3FFFFFFFFFFFFFFF) },        // 2^126 - 2^64 + 1
        { 5, 7, 0 },
    };
    for (const Case& c : cases)
    {
        ctx.r4.s64 = c.a;
        ctx.r5.s64 = c.b;
        sample_mulhd(ctx, nullptr);
        CHECK(ctx.r3.s64 == c.high);
        ctx.r3.s64 = 1234;
        sample_mulhd_record(ctx, nullptr);
        CHECK(ctx.r3.s64 == c.high);
        CHECK(ctx.cr0.lt == (c.high < 0) && ctx.cr0.gt == (c.high > 0) && ctx.cr0.eq == (c.high == 0));
    }
}

// vpkuwum keeps the low halves of vA's words in elements 0-3 and of vB's in 4-7, with no saturation. In the reversed
// layout vB's words land in u16[0..3] and vA's in u16[4..7].
static void PackModulo(PPCContext& ctx)
{
    for (uint32_t i = 0; i < 4; ++i)
    {
        ctx.v4.u32[i] = 0xABCD0000u | (0x1000 + i);
        ctx.v5.u32[i] = 0x00070000u | (0xF000 + i);
    }
    sample_vpkuwum(ctx, nullptr);
    for (uint32_t i = 0; i < 4; ++i)
    {
        CHECK(ctx.v4.u16[i] == 0xF000 + i);
        CHECK(ctx.v4.u16[i + 4] == 0x1000 + i);
    }
}

// vpkd3d128 type 3 (float16_2), pack 1: x (element 0, f32[3] here) and y become the high and low halves of the word that
// `shift` names (u32[shift] here, as the D3DCOLOR form), the other words kept; vupkd3d128 type 3 reads them back.
static void PackHalf2(PPCContext& ctx)
{
    auto source = [&](PPCVRegister& v) {
        v.u32[0] = 0x11111111;
        v.u32[1] = 0x22222222;
        v.f32[2] = -2.0f; // y
        v.f32[3] = 1.5f;  // x
    };
    source(ctx.v4);
    for (uint32_t i = 0; i < 4; ++i)
        ctx.v3.u32[i] = 0xA0A0A0A0 + i;
    sample_vpkd3d128_half2(ctx, nullptr);
    CHECK(ctx.v3.u32[3] == 0x3E00C000); // 1.5 is 0x3E00, -2 is 0xC000
    CHECK(ctx.v3.u32[0] == 0xA0A0A0A0 && ctx.v3.u32[1] == 0xA0A0A0A1 && ctx.v3.u32[2] == 0xA0A0A0A2);

    source(ctx.v4);
    sample_vpkd3d128_half2_self(ctx, nullptr);
    CHECK(ctx.v4.u32[3] == 0x3E00C000 && ctx.v4.u32[0] == 0x11111111 && ctx.v4.f32[2] == -2.0f);

    source(ctx.v4);
    ctx.v4.f32[3] = 0.25f;
    ctx.v4.f32[2] = 100000.0f; // past the largest half: 0x7FFF, as the float16_4 form
    sample_vpkd3d128_half2_0(ctx, nullptr);
    CHECK(ctx.v3.u32[0] == 0x34007FFF);
    ctx.v4.f32[2] = -3.0f;
    sample_vpkd3d128_half2_0(ctx, nullptr);
    sample_vupkd3d128_half2(ctx, nullptr);
    CHECK(ctx.v5.f32[3] == 0.25f && ctx.v5.f32[2] == -3.0f && ctx.v5.f32[1] == 0.0f && ctx.v5.f32[0] == 1.0f);
}

// Registers as locals: the funclet sees the parent's r12 (100) and r30 (7), kept in the context for the call, and the
// context's own r12 and r30 are back after it (a context function further up still owns them).
static void ContextFunctions(PPCContext& ctx)
{
    ctx.r12.u64 = 9;
    ctx.r30.u64 = 55;
    sample_ctx_parent(ctx, nullptr);
    CHECK(ctx.r3.u64 == 112);
    CHECK(ctx.r12.u64 == 9 && ctx.r30.u64 == 55);
}

// The 750CL (recompiler_750cl.inl). Each FPR is a pair: ps0 (ctx.fN) and ps1 (ctx.ps1[N]).
static uint8_t memory[0x1000]; // the samples' guest memory, from address 0
extern "C" uint64_t ppc_mftb() { return 0x0123456789ABCDEF; } // the samples' timebase
static void Pair(PPCContext& ctx, int n, double ps0, double ps1)
{
    (&ctx.f0)[n].f64 = ps0;
    ctx.ps1[n] = ps1;
}
static bool Is(const PPCContext& ctx, int n, double ps0, double ps1)
{
    return (&ctx.f0)[n].f64 == ps0 && ctx.ps1[n] == ps1;
}
static void Put16(uint32_t address, uint16_t value)
{
    memory[address] = uint8_t(value >> 8);
    memory[address + 1] = uint8_t(value);
}
static uint16_t Get16(uint32_t address) { return uint16_t(memory[address] << 8 | memory[address + 1]); }
static void PutFloat(uint32_t address, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, 4);
    Put16(address, uint16_t(bits >> 16));
    Put16(address + 2, uint16_t(bits));
}
static float GetFloat(uint32_t address)
{
    const uint32_t bits = uint32_t(Get16(address)) << 16 | Get16(address + 2);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}
// A GQR: the load's type and scale in its upper half, the store's in its lower.
static uint32_t Gqr(uint32_t loadType, int loadScale, uint32_t storeType, int storeScale)
{
    return (uint32_t(loadScale) & 0x3F) << 24 | loadType << 16 | (uint32_t(storeScale) & 0x3F) << 8 | storeType;
}

// Lane by lane, rounded to a single; the scalar forms take one lane of c; d may be a source.
static void PairedArithmetic(PPCContext& ctx)
{
    auto run = [&](PPCFunc* sample) {
        Pair(ctx, 2, 3.0, -2.0);
        Pair(ctx, 3, 0.5, 4.0);
        Pair(ctx, 4, 2.0, 10.0);
        Pair(ctx, 1, 99.0, 99.0);
        sample(ctx, memory);
    };
    run(sample_ps_add);
    CHECK(Is(ctx, 1, 3.5, 2.0));
    run(sample_ps_sub);
    CHECK(Is(ctx, 1, 2.5, -6.0));
    run(sample_ps_mul);
    CHECK(Is(ctx, 1, 6.0, -20.0));
    run(sample_ps_div);
    CHECK(Is(ctx, 1, 6.0, -0.5));
    run(sample_ps_madd); // a * c + b
    CHECK(Is(ctx, 1, 6.5, -16.0));
    run(sample_ps_msub);
    CHECK(Is(ctx, 1, 5.5, -24.0));
    run(sample_ps_nmadd);
    CHECK(Is(ctx, 1, -6.5, 16.0));
    run(sample_ps_nmsub);
    CHECK(Is(ctx, 1, -5.5, 24.0));
    run(sample_ps_muls0); // both lanes times c0 = 2
    CHECK(Is(ctx, 1, 6.0, -4.0));
    run(sample_ps_muls1); // times c1 = 10
    CHECK(Is(ctx, 1, 30.0, -20.0));
    run(sample_ps_madds0);
    CHECK(Is(ctx, 1, 6.5, 0.0));
    run(sample_ps_madds1);
    CHECK(Is(ctx, 1, 30.5, -16.0));
    run(sample_ps_sum0); // d0 = a0 + b1, d1 = c1
    CHECK(Is(ctx, 1, 7.0, 10.0));
    run(sample_ps_sum1); // d0 = c0, d1 = a0 + b1
    CHECK(Is(ctx, 1, 2.0, 7.0));
    run(sample_ps_sel); // a >= 0 ? c : b
    CHECK(Is(ctx, 1, 2.0, 4.0));
    Pair(ctx, 2, -0.0, NAN);
    Pair(ctx, 4, 2.0, 10.0);
    Pair(ctx, 3, 0.5, 4.0);
    sample_ps_sel(ctx, memory); // -0 counts as zero; NaN takes b
    CHECK(Is(ctx, 1, 2.0, 4.0));
    run(sample_ps_res);
    CHECK(Is(ctx, 1, 2.0, 0.25));
    run(sample_ps_rsqrte);
    CHECK(Is(ctx, 1, double(float(1.0 / std::sqrt(0.5))), 0.5));

    // Rounded to a single: 1 + 2^-30 is 1.
    Pair(ctx, 2, 1.0, 1.0);
    Pair(ctx, 3, std::ldexp(1.0, -30), std::ldexp(1.0, -20));
    sample_ps_add(ctx, memory);
    CHECK(Is(ctx, 1, 1.0, 1.0 + std::ldexp(1.0, -20)));

    // ps_add f2, f2, f3: both lanes read f2 before it is written.
    Pair(ctx, 2, 1.0, 2.0);
    Pair(ctx, 3, 10.0, 20.0);
    sample_ps_add_self(ctx, memory);
    CHECK(Is(ctx, 2, 11.0, 22.0));
}

// Sign and copy, lane by lane; the merges pick a lane of a and of b.
static void PairedMoves(PPCContext& ctx)
{
    Pair(ctx, 3, -1.5, 2.0);
    sample_ps_neg(ctx, memory);
    CHECK(Is(ctx, 1, 1.5, -2.0));
    sample_ps_abs(ctx, memory);
    CHECK(Is(ctx, 1, 1.5, 2.0));
    sample_ps_nabs(ctx, memory);
    CHECK(Is(ctx, 1, -1.5, -2.0));
    sample_ps_mr(ctx, memory);
    CHECK(Is(ctx, 1, -1.5, 2.0));

    Pair(ctx, 2, 1.0, 2.0);
    Pair(ctx, 3, 3.0, 4.0);
    sample_ps_merge00(ctx, memory);
    CHECK(Is(ctx, 1, 1.0, 3.0));
    sample_ps_merge01(ctx, memory);
    CHECK(Is(ctx, 1, 1.0, 4.0));
    sample_ps_merge10(ctx, memory);
    CHECK(Is(ctx, 1, 2.0, 3.0));
    sample_ps_merge11(ctx, memory);
    CHECK(Is(ctx, 1, 2.0, 4.0));
    sample_ps_merge10_self(ctx, memory);
    CHECK(Is(ctx, 2, 2.0, 1.0));

    // ps_merge into ps1 keeps a single: a double's extra bits go.
    Pair(ctx, 3, 1.0 + std::ldexp(1.0, -40), 0.0);
    sample_ps_merge00(ctx, memory);
    CHECK(ctx.ps1[1] == 1.0);
}

// ps_cmpu0 compares lane 0, ps_cmpo1 lane 1, into cr1.
static void PairedCompares(PPCContext& ctx)
{
    Pair(ctx, 2, 1.0, 5.0);
    Pair(ctx, 3, 2.0, 5.0);
    sample_ps_cmpu0(ctx, memory);
    CHECK(ctx.cr1.lt && !ctx.cr1.gt && !ctx.cr1.eq);
    sample_ps_cmpo1(ctx, memory);
    CHECK(!ctx.cr1.lt && !ctx.cr1.gt && ctx.cr1.eq);
}

// The quantized loads: a float pair; the integer types divided by 2^scale; W set loads one value and 1.0.
static void QuantizedLoads(PPCContext& ctx)
{
    ctx.r3.u64 = 0x100;
    PutFloat(0x108, 1.25f);
    PutFloat(0x10C, -3.0f);
    ctx.gqr[1] = 0;
    sample_psq_l(ctx, memory);
    CHECK(Is(ctx, 1, 1.25, -3.0));

    memory[0x108] = 10;
    memory[0x109] = 200;
    ctx.gqr[1] = Gqr(4, 0, 0, 0); // u8
    sample_psq_l(ctx, memory);
    CHECK(Is(ctx, 1, 10.0, 200.0));
    memory[0x108] = 0xFE;
    memory[0x109] = 6;
    ctx.gqr[1] = Gqr(6, 1, 0, 0); // s8, halved
    sample_psq_l(ctx, memory);
    CHECK(Is(ctx, 1, -1.0, 3.0));
    Put16(0x108, 0x0100);
    Put16(0x10A, 0x0280);
    ctx.gqr[1] = Gqr(5, 8, 0, 0); // u16 / 256
    sample_psq_l(ctx, memory);
    CHECK(Is(ctx, 1, 1.0, 2.5));
    Put16(0x108, 0xFFFF);
    Put16(0x10A, 0x0003);
    ctx.gqr[1] = Gqr(7, -2, 0, 0); // s16 * 4
    sample_psq_l(ctx, memory);
    CHECK(Is(ctx, 1, -4.0, 12.0));

    PutFloat(0xFC, 7.0f);
    ctx.gqr[1] = 0;
    sample_psq_l_single(ctx, memory); // -4(r3)
    CHECK(Is(ctx, 1, 7.0, 1.0));

    PutFloat(0x110, 0.5f);
    PutFloat(0x114, 0.75f);
    sample_psq_lu(ctx, memory); // 16(r3), then r3 = 0x110
    CHECK(Is(ctx, 1, 0.5, 0.75) && ctx.r3.u32 == 0x110);

    ctx.r3.u64 = 0x100;
    ctx.r4.u64 = 0x10;
    sample_psq_lx(ctx, memory);
    CHECK(Is(ctx, 1, 0.5, 0.75) && ctx.r3.u32 == 0x100);
    sample_psq_lux(ctx, memory);
    CHECK(Is(ctx, 1, 0.5, 0.75) && ctx.r3.u32 == 0x110);
}

// The quantized stores: times 2^scale, toward zero, saturated; NaN stores 0; W set stores ps0 alone.
static void QuantizedStores(PPCContext& ctx)
{
    ctx.r3.u64 = 0x200;
    ctx.gqr[1] = 0;
    Pair(ctx, 1, 1.5, -2.25);
    sample_psq_st(ctx, memory);
    CHECK(GetFloat(0x208) == 1.5f && GetFloat(0x20C) == -2.25f);

    ctx.gqr[1] = Gqr(0, 0, 7, 8); // s16 * 256
    Pair(ctx, 1, 1.5, -0.5);
    sample_psq_st(ctx, memory);
    CHECK(Get16(0x208) == 0x0180 && Get16(0x20A) == 0xFF80);

    ctx.gqr[1] = Gqr(0, 0, 4, 0); // u8
    Pair(ctx, 1, 300.0, -5.0);
    sample_psq_st(ctx, memory);
    CHECK(memory[0x208] == 255 && memory[0x209] == 0);
    Pair(ctx, 1, 2.9, NAN);
    sample_psq_st(ctx, memory);
    CHECK(memory[0x208] == 2 && memory[0x209] == 0);

    ctx.gqr[1] = Gqr(0, 0, 6, 0); // s8
    Pair(ctx, 1, -200.0, -2.9);
    sample_psq_st(ctx, memory);
    CHECK(memory[0x208] == 0x80 && memory[0x209] == 0xFE);

    ctx.gqr[1] = Gqr(0, 0, 5, -1); // u16, halved
    Pair(ctx, 1, 100.0, 70000.0 * 2);
    sample_psq_st(ctx, memory);
    CHECK(Get16(0x208) == 50 && Get16(0x20A) == 0xFFFF);

    ctx.gqr[1] = 0;
    memset(memory + 0x200, 0xAA, 8);
    Pair(ctx, 1, 3.0, 4.0);
    sample_psq_st_single(ctx, memory);
    CHECK(GetFloat(0x200) == 3.0f && memory[0x204] == 0xAA);

    sample_psq_stu(ctx, memory);
    CHECK(GetFloat(0x210) == 3.0f && GetFloat(0x214) == 4.0f && ctx.r3.u32 == 0x210);
    ctx.r3.u64 = 0x200;
    ctx.r4.u64 = 0x20;
    sample_psq_stx(ctx, memory);
    CHECK(GetFloat(0x220) == 3.0f && ctx.r3.u32 == 0x200);
    sample_psq_stux(ctx, memory);
    CHECK(GetFloat(0x224) == 4.0f && ctx.r3.u32 == 0x220);
}

// GQRs through mtspr and mfspr; dcbz_l zeroes 32 bytes; lmw and stmw move r29-r31.
static void SpecialAndMultiple(PPCContext& ctx)
{
    ctx.r5.u64 = 0x12345678;
    sample_mtgqr(ctx, memory);
    CHECK(ctx.gqr[1] == 0x12345678);
    ctx.gqr[1] = 0x0A0B0C0D;
    sample_mfgqr(ctx, memory);
    CHECK(ctx.r6.u64 == 0x0A0B0C0D);

    memset(memory + 0x300, 0xFF, 0x60);
    ctx.r3.u64 = 0x300;
    ctx.r4.u64 = 0x25; // inside the line at 0x320
    sample_dcbz_l(ctx, memory);
    CHECK(memory[0x31F] == 0xFF && memory[0x320] == 0 && memory[0x33F] == 0 && memory[0x340] == 0xFF);

    ctx.r3.u64 = 0x400;
    ctx.r29.u64 = 0x11111111;
    ctx.r30.u64 = 0x22222222;
    ctx.r31.u64 = 0x33333333;
    sample_stmw(ctx, memory);
    CHECK(Get16(0x408) == 0x1111 && Get16(0x40C) == 0x2222 && Get16(0x412) == 0x3333);
    ctx.r29.u64 = ctx.r30.u64 = ctx.r31.u64 = 0;
    Put16(0x410, 0x8000);
    sample_lmw(ctx, memory);
    CHECK(ctx.r29.u64 == 0x11111111 && ctx.r30.u64 == 0x22222222 && ctx.r31.u64 == 0x80003333);
}

// lfs and the single-precision arithmetic fill both halves; lfd and fmr leave ps1.
static void BothHalves(PPCContext& ctx)
{
    ctx.r3.u64 = 0x500;
    PutFloat(0x500, 6.5f);
    Pair(ctx, 1, 0.0, 9.0);
    sample_lfs_fill(ctx, memory);
    CHECK(Is(ctx, 1, 6.5, 6.5));
    Pair(ctx, 1, 0.0, 9.0);
    sample_lfd_keep(ctx, memory);
    CHECK(ctx.ps1[1] == 9.0);
    Pair(ctx, 2, 1.0, 0.0);
    Pair(ctx, 3, 2.0, 0.0);
    sample_fadds_fill(ctx, memory);
    CHECK(Is(ctx, 1, 3.0, 3.0));
    Pair(ctx, 1, 0.0, 9.0);
    sample_fmr_keep(ctx, memory);
    CHECK(Is(ctx, 1, 2.0, 9.0));
}

// fcmpo as fcmpu; the CR logical forms by bit; rlwnm; mfctr, mfxer; the barriers do nothing.
static void PlainPowerPc(PPCContext& ctx)
{
    Pair(ctx, 2, 1.0, 0.0);
    Pair(ctx, 3, NAN, 0.0);
    sample_fcmpo(ctx, memory);
    CHECK(!ctx.cr1.lt && !ctx.cr1.gt && !ctx.cr1.eq && ctx.cr1.so); // unordered
    for (int lt = 0; lt < 2; lt++)
    {
        for (int gt = 0; gt < 2; gt++)
        {
            ctx.cr0.lt = uint8_t(lt);
            ctx.cr0.gt = uint8_t(gt);
            ctx.cr0.eq = 1;
            sample_cror(ctx, memory);
            CHECK(ctx.cr1.eq == (lt | gt));
            sample_crxor(ctx, memory);
            CHECK(ctx.cr1.lt == (lt ^ gt));
            sample_crnor(ctx, memory);
            CHECK(ctx.cr1.lt == !(lt | gt));
            sample_crandc(ctx, memory);
            CHECK(ctx.cr1.lt == (lt & !gt));
        }
    }
    ctx.cr1.lt = 1;
    sample_crnot(ctx, memory); // cr1.lt = !cr0.eq, cr0.eq is 1
    CHECK(ctx.cr1.lt == 0);
    sample_crset(ctx, memory);
    CHECK(ctx.cr1.eq == 1);
    sample_crclr(ctx, memory);
    CHECK(ctx.cr1.eq == 0);

    ctx.r4.u64 = 0x12345678;
    ctx.r5.u64 = 0xFFFFFF08; // 8: the low five bits only
    sample_rlwnm(ctx, memory); // rotlw by 8, keep bits 24-31
    CHECK(ctx.r3.u64 == 0x12);

    ctx.ctr.u64 = 0xABCD;
    sample_mfctr(ctx, memory);
    CHECK(ctx.r3.u64 == 0xABCD);
    ctx.xer.so = 1;
    ctx.xer.ov = 0;
    ctx.xer.ca = 1;
    sample_mfxer(ctx, memory);
    CHECK(ctx.r3.u64 == 0xA0000000);
    sample_mftb(ctx, memory);
    CHECK(ctx.r3.u32 == 0x89ABCDEF);
    sample_mftbu(ctx, memory);
    CHECK(ctx.r3.u32 == 0x01234567);

    // Nothing to see: the registers they name and the memory stay as they were.
    static uint8_t before[sizeof(memory)];
    memcpy(before, memory, sizeof(memory));
    ctx.r3.u64 = 0x300;
    ctx.r4.u64 = 0x20;
    sample_isync(ctx, memory);
    sample_icbi(ctx, memory);
    sample_dcbi(ctx, memory);
    CHECK(ctx.r3.u64 == 0x300 && ctx.r4.u64 == 0x20 && memcmp(before, memory, sizeof(memory)) == 0);
}

// An ELF's functions from its symbols; the 750CL's register-save helper by entry; rlwimi. sets CR0.
static void FromElf(PPCContext& ctx)
{
    sub_85000000(ctx, memory);
    CHECK(ctx.r3.u64 == 7);
    sub_85000008(ctx, memory); // called by no one: known from its symbol alone
    CHECK(ctx.r3.u64 == 9);
    ctx.r11.u64 = 0x600;
    ctx.r30.u64 = 0x30303030;
    ctx.r31.u64 = 0x31313131;
    memset(memory + 0x5F0, 0, 0x10);
    __savegprlr_30(ctx, memory); // stores r30 and r31 below r11
    CHECK(Get16(0x5F8) == 0x3030 && Get16(0x5FC) == 0x3131 && Get16(0x5F4) == 0);
    ctx.r3.u64 = 0x80000011;
    ctx.r4.u64 = 0xAB;
    ctx.cr0.lt = ctx.cr0.gt = ctx.cr0.eq = 0;
    sub_8500005C(ctx, memory); // rlwimi. r3, r4, 8, 16, 23
    CHECK(ctx.r3.u32 == 0x8000AB11 && ctx.cr0.lt && !ctx.cr0.gt && !ctx.cr0.eq);
}

int main()
{
    static PPCContext ctx{};
    Shifts(ctx);
    Compare(ctx, sample_vcmpgtsb, 1, 0x05, 0xF0); // signed: 5 > -16
    Compare(ctx, sample_vcmpgtsh, 2, 0x05, 0xF0);
    Compare(ctx, sample_vcmpgtub, 1, 0xF0, 0x05); // unsigned: 240 > 5
    Compare(ctx, sample_vcmpgtuh, 2, 0xF0, 0x05);
    AddSaturate(ctx);
    MultiplyHigh(ctx);
    PackModulo(ctx);
    PackHalf2(ctx);
    ContextFunctions(ctx);
    PairedArithmetic(ctx);
    PairedMoves(ctx);
    PairedCompares(ctx);
    QuantizedLoads(ctx);
    QuantizedStores(ctx);
    SpecialAndMultiple(ctx);
    BothHalves(ctx);
    PlainPowerPc(ctx);
    FromElf(ctx);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
