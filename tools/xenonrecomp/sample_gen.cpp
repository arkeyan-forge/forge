// Translates invented PowerPC samples with XenonRecomp into a header for xenonrecomp_test.cpp. No game code.
#include "recompiler.h"
#include <cstring>
#include <string>
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

    // A minimal big-endian PowerPC ELF: .text at `base`, its function symbols, and the section names.
    struct ElfSymbol
    {
        const char* name;
        uint32_t address, size;
    };
    std::vector<uint8_t> MakeElf(
        uint32_t base, const std::vector<uint32_t>& text, const std::vector<ElfSymbol>& symbols)
    {
        std::vector<uint8_t> out(52 + 32);
        auto put16 = [&](size_t at, uint32_t v) {
            out[at] = uint8_t(v >> 8);
            out[at + 1] = uint8_t(v);
        };
        auto put32 = [&](size_t at, uint32_t v) {
            for (int i = 0; i < 4; i++)
                out[at + i] = uint8_t(v >> (24 - 8 * i));
        };
        auto append32 = [&](uint32_t v) {
            out.resize(out.size() + 4);
            put32(out.size() - 4, v);
        };
        auto appendText = [&](const std::string& text) {
            size_t at = out.size();
            out.insert(out.end(), text.begin(), text.end());
            return at;
        };
        const size_t textOffset = out.size();
        for (uint32_t word : text)
            append32(word);
        const size_t textSize = out.size() - textOffset;
        std::string names(1, '\0');
        std::vector<uint32_t> nameOffsets;
        for (const auto& symbol : symbols)
        {
            nameOffsets.push_back(uint32_t(names.size()));
            names += symbol.name;
            names += '\0';
        }
        const size_t strtab = appendText(names);
        const size_t strtabSize = names.size();
        while (out.size() % 4)
            out.push_back(0);
        const size_t symtab = out.size();
        out.resize(out.size() + 16); // the null symbol
        for (size_t i = 0; i < symbols.size(); i++)
        {
            size_t at = out.size();
            out.resize(at + 16);
            put32(at, nameOffsets[i]);
            put32(at + 4, symbols[i].address);
            put32(at + 8, symbols[i].size);
            out[at + 12] = 0x12; // global, STT_FUNC
            put16(at + 14, 1);   // .text
        }
        const size_t symtabSize = out.size() - symtab;
        const std::string sectionNames = std::string("\0.text\0.strtab\0.symtab\0.shstrtab\0", 33);
        const size_t shstrtab = appendText(sectionNames);
        while (out.size() % 4)
            out.push_back(0);
        const size_t sections = out.size();
        out.resize(out.size() + 5 * 40);
        auto section = [&](int index, uint32_t name, uint32_t type, uint32_t flags, uint32_t address, size_t offset,
                           size_t size, uint32_t link, uint32_t entry) {
            size_t at = sections + index * 40;
            put32(at, name);
            put32(at + 4, type);
            put32(at + 8, flags);
            put32(at + 12, address);
            put32(at + 16, uint32_t(offset));
            put32(at + 20, uint32_t(size));
            put32(at + 24, link);
            put32(at + 36, entry);
        };
        section(1, 1, 1, 6, base, textOffset, textSize, 0, 0);       // .text: PROGBITS, alloc + exec
        section(2, 7, 3, 0, 0, strtab, strtabSize, 0, 0);            // .strtab
        section(3, 15, 2, 0, 0, symtab, symtabSize, 2, 16);          // .symtab, its names in .strtab
        section(4, 23, 3, 0, 0, shstrtab, sectionNames.size(), 0, 0); // .shstrtab
        memcpy(out.data(),
            "\x7F"
            "ELF\x01\x02\x01",
            7);
        put16(16, 2);  // executable
        put16(18, 20); // PowerPC
        put32(20, 1);
        put32(24, base);
        put32(28, 52);
        put32(32, uint32_t(sections));
        put16(40, 52);
        put16(42, 32);
        put16(44, 1);
        put16(46, 40);
        put16(48, 5);
        put16(50, 4);
        put32(52, 1); // PT_LOAD
        put32(56, uint32_t(textOffset));
        put32(60, base);
        put32(64, base);
        put32(68, uint32_t(textSize));
        put32(72, uint32_t(textSize));
        put32(76, 5);
        return out;
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
        { "sample_mftb", Spr(371, 3, 268) }, // mftb r3: the timebase's low half
        { "sample_mftbu", Spr(371, 3, 269) }, // mftb r3, 269 (mftbu): its high half, which 32-bit code reads apart
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
    // An ELF of the 750CL's compilers: its functions from the symbol table (b is called by no one), the register-save
    // helpers by entry (each runs to its blr, no lr saved), and rlwimi. setting CR0.
    constexpr uint32_t elfBase = 0x85000000, saveGpr = elfBase + 0x10;
    std::vector<uint32_t> text = { 0x38600007, Blr, 0x38600009, Blr }; // li r3, 7; blr; li r3, 9; blr
    for (uint32_t r = 14; r < 32; r++)
        text.push_back(D(36, r, 11, -int32_t(32 - r) * 4)); // stw rN, -4 * (32 - N)(r11)
    text.push_back(Blr);
    text.push_back(
        (20u << 26) | (4 << 21) | (3 << 16) | (8 << 11) | (16 << 6) | (23 << 1) | 1); // rlwimi. r3, r4, 8, 16, 23
    text.push_back(Blr);
    const std::vector<uint8_t> elf = MakeElf(elfBase, text,
        { { "a", elfBase, 8 }, { "b", elfBase + 8, 8 }, { "__save_gpr", saveGpr, 76 }, { "c", saveGpr + 76, 8 } });
    Recompiler fromElf;
    fromElf.config.pairedSingles = true;
    fromElf.config.saveGpr14Address = saveGpr;
    fromElf.image = Image::ParseImage(elf.data(), elf.size());
    fromElf.Analyse();
    auto size = [&](uint32_t address) -> size_t {
        for (const auto& fn : fromElf.functions)
            if (fn.base == address)
                return fn.size;
        return 0;
    };
    if (fromElf.image.functionSymbols.size() != 4 || size(elfBase) != 8 || size(elfBase + 8) != 8 ||
        size(saveGpr) != 76 || size(saveGpr + 17 * 4) != 8 || size(saveGpr + 76) != 8 || fromElf.functions.size() != 21)
    {
        fprintf(stderr, "the ELF's functions: %zu symbols, %zu functions, sizes %zu %zu %zu %zu %zu\n",
            fromElf.image.functionSymbols.size(), fromElf.functions.size(), size(elfBase), size(elfBase + 8),
            size(saveGpr), size(saveGpr + 17 * 4), size(saveGpr + 76));
        return 1;
    }
    for (const auto& fn : fromElf.functions)
    {
        if (!fromElf.Recompile(fn))
        {
            fprintf(stderr, "0x%zX: not translated\n", fn.base);
            return 1;
        }
    }
    // The write-gather pipe (write_gather_pipe): stores through a register lis, addi or mr pointed at it are the
    // host's (PPC_MM_STORE_*, PPC_MM_STORE_PAIRED); a store through any other register stays a plain one.
    Recompiler gather;
    gather.config.pairedSingles = true;
    gather.config.writeGatherPipe = 0xCC008000;
    constexpr uint32_t LisPipe4 = (15u << 26) | (4 << 21) | 0xCC01; // lis r4, 0xCC01: the pipe is -0x8000 from it
    const std::pair<const char*, std::vector<uint32_t>> gatherBodies[] = {
        { "sample_gather_stw", { LisPipe4, D(36, 3, 4, -0x8000), Blr } }, // stw r3, -0x8000(r4)
        { "sample_gather_sth", { LisPipe4, D(44, 3, 4, -0x8000), Blr } }, // sth r3, -0x8000(r4)
        { "sample_gather_stfs", { LisPipe4, D(52, 1, 4, -0x8000), Blr } }, // stfs f1, -0x8000(r4)
        { "sample_gather_psq", { LisPipe4, D(14, 4, 4, -0x8000), Psq(60, 1, 4, 0, 0, 0), Blr } }, // addi; psq_st
        // mr r7, r4; stb r3, -0x8000(r7)
        { "sample_gather_mr",
            { LisPipe4, (31u << 26) | (4 << 21) | (7 << 16) | (4 << 11) | (444 << 1), D(38, 3, 7, -0x8000), Blr } },
        { "sample_gather_none", { D(36, 3, 5, 0), Blr } }, // stw r3, 0(r5): no register of the pipe
    };
    base = 0x86000000;
    for (const auto& [name, words] : gatherBodies)
    {
        auto& bytes = code.emplace_back();
        for (uint32_t word : words)
            for (int i = 0; i < 4; i++)
                bytes.push_back(uint8_t(word >> (24 - 8 * i)));
        gather.image.Map(name, base, uint32_t(bytes.size()), SectionFlags_Code, bytes.data());
        gather.image.symbols.emplace(name, base, bytes.size(), Symbol_Function);
        if (!gather.Recompile(Function(base, bytes.size())))
        {
            fprintf(stderr, "%s: not translated\n", name);
            return 1;
        }
        base += 0x1000;
    }
    std::ofstream(argv[1], std::ios::binary) << recompiler.out << locals.out << paired.out << fromElf.out << gather.out;
    return 0;
}
