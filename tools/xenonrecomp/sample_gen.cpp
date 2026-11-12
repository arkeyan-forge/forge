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

    Recompiler recompiler;
    recompiler.println("#define PPC_CONFIG_H_INCLUDED");
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
    std::ofstream(argv[1], std::ios::binary) << recompiler.out << locals.out;
    return 0;
}
