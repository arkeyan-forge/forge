// Translates invented PowerPC samples with XenonRecomp into a header for xenonrecomp_test.cpp. No game code.
#include "recompiler.h"
#include <utility>
#include <vector>

namespace
{
    constexpr uint32_t Blr = 0x4E800020;
    constexpr uint32_t Record = 0x400; // the Rc bit of a vector compare: the dot form, which sets CR6

// Altivec VX/VC form: primary opcode 4, vD, vA, vB, extended opcode.
    constexpr uint32_t Vx(uint32_t xo, uint32_t d, uint32_t a, uint32_t b)
    {
        return (4u << 26) | (d << 21) | (a << 16) | (b << 11) | xo;
    }

    // The 750CL's forms [PS]: A (xo in bits 26-30), X (bits 21-30), the quantized D form (W, I, a 12-bit
    // displacement) and its indexed form (W, I, xo in bits 25-30).
    constexpr uint32_t PsA(uint32_t xo, uint32_t d, uint32_t a, uint32_t b, uint32_t c)
    {
        return (4u << 26) | (d << 21) | (a << 16) | (b << 11) | (c << 6) | (xo << 1);
    }
    constexpr uint32_t PsX(uint32_t xo, uint32_t d, uint32_t a, uint32_t b)
    {
        return (4u << 26) | (d << 21) | (a << 16) | (b << 11) | (xo << 1);
    }
    constexpr uint32_t Psq(uint32_t op, uint32_t d, uint32_t a, uint32_t w, uint32_t i, int32_t displacement)
    {
        return (op << 26) | (d << 21) | (a << 16) | (w << 15) | (i << 12) | (uint32_t(displacement) & 0xFFF);
    }
    constexpr uint32_t PsqX(uint32_t xo, uint32_t d, uint32_t a, uint32_t b, uint32_t w, uint32_t i)
    {
        return (4u << 26) | (d << 21) | (a << 16) | (b << 11) | (w << 10) | (i << 7) | (xo << 1);
    }
    constexpr uint32_t Spr(uint32_t xo, uint32_t r, uint32_t spr) // mtspr 467, mfspr 339: SPR halves swapped
    {
        return (31u << 26) | (r << 21) | ((spr & 0x1F) << 16) | ((spr >> 5) << 11) | (xo << 1);
    }
    constexpr uint32_t Cr(uint32_t xo, uint32_t bt, uint32_t ba, uint32_t bb) // the CR logical forms, opcode 19
    {
        return (19u << 26) | (bt << 21) | (ba << 16) | (bb << 11) | (xo << 1);
    }
    constexpr uint32_t D(uint32_t op, uint32_t t, uint32_t a, int32_t displacement)
    {
        return (op << 26) | (t << 21) | (a << 16) | (uint32_t(displacement) & 0xFFFF);
    }

    struct Sample
    {
        const char* name;
        uint32_t word;
    };
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: ArkeyanXenonRecompSampleGen <output header>\n");
        return 2;
    }
    const Sample samples[] = {
        { "sample_vslo", Vx(1036, 3, 4, 3) },              // vslo v3, v4, v3: the destination is the shift operand
        { "sample_vsro", Vx(1100, 3, 4, 3) },              // vsro v3, v4, v3
        { "sample_vcmpgtsb", Vx(774 | Record, 3, 4, 5) },  // vcmpgtsb. v3, v4, v5
        { "sample_vcmpgtsh", Vx(838 | Record, 3, 4, 5) },  // vcmpgtsh. v3, v4, v5
        { "sample_vcmpgtub", Vx(518 | Record, 3, 4, 5) },  // vcmpgtub. v3, v4, v5
        { "sample_vcmpgtuh", Vx(582 | Record, 3, 4, 5) },  // vcmpgtuh. v3, v4, v5
        { "sample_vadduhs", Vx(576, 3, 4, 5) },            // vadduhs v3, v4, v5
        { "sample_mulhd", 0x7C642892 },                   // mulhd r3, r4, r5
        { "sample_mulhd_record", 0x7C642893 },            // mulhd. r3, r4, r5
        { "sample_vpkuwum", Vx(78, 4, 4, 5) },             // vpkuwum v4, v4, v5: the destination is the first source
        { "sample_vpkd3d128_half2", 0x186D26D0 },         // vpkd3d128 v3, v4, 3, 1, 3: float16_2 into word 3
        { "sample_vpkd3d128_half2_self", 0x188D26D0 },    // vpkd3d128 v4, v4, 3, 1, 3: in place
        { "sample_vpkd3d128_half2_0", 0x186D2610 },       // vpkd3d128 v3, v4, 3, 1, 0: into word 0
        { "sample_vupkd3d128_half2", 0x18AC1FF0 },        // vupkd3d128 v5, v3, 12: float16_2 back to (x, y, 0, 1)
    };

    // The 750CL's samples, translated with paired_singles (recompiler_750cl.inl): operands f1 = f2 op f3, c in f4.
    const Sample pairedSamples[] = {
        { "sample_ps_add", PsA(21, 1, 2, 3, 0) },     // ps_add f1, f2, f3
        { "sample_ps_add_self", PsA(21, 2, 2, 3, 0) }, // ps_add f2, f2, f3: the destination is a source
        { "sample_ps_sub", PsA(20, 1, 2, 3, 0) },
        { "sample_ps_mul", PsA(25, 1, 2, 0, 4) },     // ps_mul f1, f2, f4
        { "sample_ps_div", PsA(18, 1, 2, 3, 0) },
        { "sample_ps_madd", PsA(29, 1, 2, 3, 4) },    // ps_madd f1, f2, f4, f3
        { "sample_ps_msub", PsA(28, 1, 2, 3, 4) },
        { "sample_ps_nmadd", PsA(31, 1, 2, 3, 4) },
        { "sample_ps_nmsub", PsA(30, 1, 2, 3, 4) },
        { "sample_ps_muls0", PsA(12, 1, 2, 0, 4) },
        { "sample_ps_muls1", PsA(13, 1, 2, 0, 4) },
        { "sample_ps_madds0", PsA(14, 1, 2, 3, 4) },
        { "sample_ps_madds1", PsA(15, 1, 2, 3, 4) },
        { "sample_ps_sum0", PsA(10, 1, 2, 3, 4) },
        { "sample_ps_sum1", PsA(11, 1, 2, 3, 4) },
        { "sample_ps_sel", PsA(23, 1, 2, 3, 4) },
        { "sample_ps_res", PsA(24, 1, 0, 3, 0) },     // ps_res f1, f3
        { "sample_ps_rsqrte", PsA(26, 1, 0, 3, 0) },
        { "sample_ps_neg", PsX(40, 1, 0, 3) },        // ps_neg f1, f3
        { "sample_ps_abs", PsX(264, 1, 0, 3) },
        { "sample_ps_nabs", PsX(136, 1, 0, 3) },
        { "sample_ps_mr", PsX(72, 1, 0, 3) },
        { "sample_ps_merge00", PsX(528, 1, 2, 3) },
        { "sample_ps_merge01", PsX(560, 1, 2, 3) },
        { "sample_ps_merge10", PsX(592, 1, 2, 3) },
        { "sample_ps_merge11", PsX(624, 1, 2, 3) },
        { "sample_ps_merge10_self", PsX(592, 2, 2, 2) }, // ps_merge10 f2, f2, f2: swaps the lanes
        { "sample_ps_cmpu0", PsX(0, 1 << 2, 2, 3) },     // ps_cmpu0 cr1, f2, f3
        { "sample_ps_cmpo1", PsX(96, 1 << 2, 2, 3) },    // ps_cmpo1 cr1, f2, f3
        { "sample_psq_l", Psq(56, 1, 3, 0, 1, 8) },      // psq_l f1, 8(r3), 0, 1
        { "sample_psq_l_single", Psq(56, 1, 3, 1, 1, -4) }, // psq_l f1, -4(r3), 1, 1
        { "sample_psq_lu", Psq(57, 1, 3, 0, 1, 16) },    // psq_lu f1, 16(r3), 0, 1
        { "sample_psq_lx", PsqX(6, 1, 3, 4, 0, 1) },     // psq_lx f1, r3, r4, 0, 1
        { "sample_psq_lux", PsqX(38, 1, 3, 4, 0, 1) },
        { "sample_psq_st", Psq(60, 1, 3, 0, 1, 8) },     // psq_st f1, 8(r3), 0, 1
        { "sample_psq_st_single", Psq(60, 1, 3, 1, 1, 0) },
        { "sample_psq_stu", Psq(61, 1, 3, 0, 1, 16) },
        { "sample_psq_stx", PsqX(7, 1, 3, 4, 0, 1) },
        { "sample_psq_stux", PsqX(39, 1, 3, 4, 0, 1) },
        { "sample_mtgqr", Spr(467, 5, 913) },            // mtspr GQR1, r5
        { "sample_mfgqr", Spr(339, 6, 913) },            // mfspr r6, GQR1
        { "sample_dcbz_l", PsX(1014, 0, 3, 4) },         // dcbz_l r3, r4
        { "sample_lmw", D(46, 29, 3, 8) },               // lmw r29, 8(r3)
        { "sample_stmw", D(47, 29, 3, 8) },              // stmw r29, 8(r3)
        { "sample_lfs_fill", D(48, 1, 3, 0) },           // lfs f1, 0(r3): both halves
        { "sample_lfd_keep", D(50, 1, 3, 0) },           // lfd f1, 0(r3): ps0 only
        { "sample_fadds_fill", (59u << 26) | (1 << 21) | (2 << 16) | (3 << 11) | (21 << 1) }, // fadds f1, f2, f3
        { "sample_fmr_keep", (63u << 26) | (1 << 21) | (3 << 11) | (72 << 1) },               // fmr f1, f3
        // PowerPC the Xbox 360's compilers left out: fcmpo, the CR logical forms, rlwnm, mfctr, mfxer, the barriers.
        { "sample_fcmpo", (63u << 26) | (1 << 23) | (2 << 16) | (3 << 11) | (32 << 1) }, // fcmpo cr1, f2, f3
        { "sample_cror", Cr(449, 6, 0, 1) }, // cror 4*cr1+eq, lt, gt
        { "sample_crclr", Cr(193, 6, 6, 6) }, // crclr 4*cr1+eq
        { "sample_crset", Cr(289, 6, 6, 6) }, // crset 4*cr1+eq
        { "sample_crxor", Cr(193, 4, 0, 1) }, // crxor 4*cr1+lt, lt, gt
        { "sample_crnor", Cr(33, 4, 0, 1) }, // crnor 4*cr1+lt, lt, gt
        { "sample_crandc", Cr(129, 4, 0, 1) }, // crandc 4*cr1+lt, lt, gt
        { "sample_crnot", Cr(33, 4, 2, 2) }, // crnot 4*cr1+lt, eq
        { "sample_rlwnm",
            (23u << 26) | (4 << 21) | (3 << 16) | (5 << 11) | (24 << 6) | (31 << 1) }, // rlwnm r3, r4, r5, 24, 31
        { "sample_mfctr", Spr(339, 3, 9) }, // mfctr r3
        { "sample_mfxer", Spr(339, 3, 1) }, // mfxer r3
        { "sample_isync", 0x4C00012C },
        { "sample_icbi", (31u << 26) | (3 << 16) | (4 << 11) | (982 << 1) }, // icbi r3, r4
        { "sample_dcbi", (31u << 26) | (3 << 16) | (4 << 11) | (470 << 1) }, // dcbi r3, r4
    };

    Recompiler recompiler;
    recompiler.println("#define PPC_CONFIG_H_INCLUDED");
    recompiler.println("#define PPC_CONFIG_PAIRED_SINGLES // the 750CL's registers, for its samples");
    recompiler.println("#include <ppc_context.h>\n");
    std::vector<std::vector<uint8_t>> code;
    size_t base = 0x82000000;
    for (const auto& sample : samples)
    {
        auto& bytes = code.emplace_back();
        for (uint32_t word : { sample.word, Blr })
            for (int i = 0; i < 4; i++)
                bytes.push_back(uint8_t(word >> (24 - 8 * i)));
        recompiler.image.Map(sample.name, base, uint32_t(bytes.size()), SectionFlags_Code, bytes.data());
        recompiler.image.symbols.emplace(sample.name, base, bytes.size(), Symbol_Function);
        if (!recompiler.Recompile(Function(base, bytes.size())))
        {
            fprintf(stderr, "%s: not translated\n", sample.name);
            return 1;
        }
        base += 0x1000;
    }

    // Registers as locals (docs/tools/toolchain/xenonrecomp.md): a funclet reads its parent's r12 and r30, so it is
    // translated in the context, and its parent stores its locals there around the call.
    Recompiler locals;
    locals.config.ctrAsLocalVariable = locals.config.xerAsLocalVariable =
        locals.config.reservedRegisterAsLocalVariable = true;
    locals.config.crRegistersAsLocalVariables = locals.config.nonArgumentRegistersAsLocalVariables = true;
    locals.config.nonVolatileRegistersAsLocalVariables = true;
    constexpr uint32_t funclet = 0x83000000, parent = 0x83001000;
    const std::pair<const char*, std::vector<uint32_t>> bodies[] = {
        { "sample_ctx_funclet", { 0x386C0005, 0x7C63F214, Blr } }, // addi r3, r12, 5; add r3, r3, r30; blr
        // li r12, 100; li r30, 7; bl funclet; blr
        { "sample_ctx_parent", { 0x39800064, 0x3BC00007, 0x48000001 | ((funclet - (parent + 8)) & 0x03FFFFFC), Blr } },
    };
    uint32_t address = funclet;
    for (const auto& [name, words] : bodies)
    {
        auto& bytes = code.emplace_back();
        for (uint32_t word : words)
            for (int i = 0; i < 4; i++)
                bytes.push_back(uint8_t(word >> (24 - 8 * i)));
        locals.image.Map(name, address, uint32_t(bytes.size()), SectionFlags_Code, bytes.data());
        locals.image.symbols.emplace(name, address, bytes.size(), Symbol_Function);
        locals.functions.emplace_back(address, bytes.size());
        address = parent;
    }
    locals.FindContextFunctions();
    for (const auto& fn : locals.functions)
    {
        if (!locals.Recompile(fn))
        {
            fprintf(stderr, "0x%zX: not translated\n", fn.base);
            return 1;
        }
    }
    Recompiler paired;
    paired.config.pairedSingles = true;
    ppc::SetPairedSingles(true); // what Recompiler::LoadConfig does for such a config
    base = 0x84000000;
    for (const auto& sample : pairedSamples)
    {
        auto& bytes = code.emplace_back();
        for (uint32_t word : { sample.word, Blr })
            for (int i = 0; i < 4; i++)
                bytes.push_back(uint8_t(word >> (24 - 8 * i)));
        paired.image.Map(sample.name, base, uint32_t(bytes.size()), SectionFlags_Code, bytes.data());
        paired.image.symbols.emplace(sample.name, base, bytes.size(), Symbol_Function);
        if (!paired.Recompile(Function(base, bytes.size())))
        {
            fprintf(stderr, "%s: not translated\n", sample.name);
            return 1;
        }
        base += 0x1000;
    }
    std::ofstream(argv[1], std::ios::binary) << recompiler.out << locals.out << paired.out;
    return 0;
}
