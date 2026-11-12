// Runs XenonRecomp's translation of invented samples (sample_gen.cpp) on the host against the PowerPC semantics.
#include <xenonrecomp_sample.h>
#include <cstdio>

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
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
