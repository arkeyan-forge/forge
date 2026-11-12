// The reference for decoder_compare: binutils' PowerPC disassembler (GPL), called as XenonRecomp's disasm.cpp called
// it. Built only by decoder_compare.sh, from a copy of the old sources in private/; never part of a kit.
#define ppc_insn ReferencePpcInsn // the old structure's name is ours now
#include <dis-asm.h>
#include <ppc.h>
#undef ppc_insn
#include "decoder_compare.h"
#include <cstdio>
#include <cstring>

namespace
{
    struct Engine
    {
        disassemble_info info{};
        Engine()
        {
            INIT_DISASSEMBLE_INFO(info, stdout, fprintf);
            info.arch = bfd_arch_powerpc;
            info.endian = BFD_ENDIAN_BIG;
            info.disassembler_options = "cell 64";
        }
    };
    thread_local Engine engine;
} // namespace

void ReferenceDecode(uint32_t word, uint64_t address, DecodedWord& out)
{
    uint8_t bytes[4] = { uint8_t(word >> 24), uint8_t(word >> 16), uint8_t(word >> 8), uint8_t(word) };
    ReferencePpcInsn insn;
    memset(&insn, 0xA5, sizeof(insn)); // a field the decoder leaves alone shows up as a difference
    engine.info.buffer = bytes;
    engine.info.buffer_vma = address;
    engine.info.buffer_length = 4;
    out.size = decode_insn_ppc(address, &engine.info, &insn);
    out.valid = insn.opcode != nullptr;
    out.name = out.valid ? insn.opcode->name : "";
    out.id = out.valid ? insn.opcode->id : -1;
    out.instruction = insn.instruction;
    for (int i = 0; i < 8; i++)
    {
        out.operands[i] = insn.operands[i];
        out.hasOperand[i] = out.valid && insn.opcode->operands[i] != 0;
    }
    memcpy(out.text, insn.op_str, sizeof(out.text));
    out.text[sizeof(out.text) - 1] = 0;
}
