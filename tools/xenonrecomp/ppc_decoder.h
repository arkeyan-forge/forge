// Our own PowerPC decoder for XenonRecomp: the Xenon's instruction set (64-bit PowerPC, VMX, VMX128), decoded into
// the structure XenonRecomp's recompiler reads, with the same mnemonics, operand values and operand text as before.
// Written from the public instruction-set documents only (see ppc_decoder.cpp).
#pragma once
#include <cstddef>
#include <cstdint>

// What the decoder tells of an instruction: its mnemonic, its id and which operands it has.
struct PpcOpcode
{
    const char* name;    // as printed: the record dot and the branch hint are part of it
    int id;              // PPC_INST_* (ppc-inst.h); the dot form shares the id of the plain one
    uint8_t operands[8]; // 1 for each operand the instruction has, in order (hidden ones included)
};

// The decoded instruction; the name and the fields are the ones XenonRecomp's recompiler uses.
struct ppc_insn
{
    uint32_t instruction;
    const PpcOpcode* opcode; // nullptr: not an instruction (op_str is then ".long 0x...")
    uint32_t operands[8];    // values in operand order: registers by number, immediates sign-extended, branch targets
    char op_str[160];        // the operand text
};

namespace ppc
{
    // Decodes one big-endian word at a guest address. Returns 4 (the bytes consumed).
    int DecodeWord(uint32_t word, uint64_t address, ppc_insn& out);
} // namespace ppc

// Fields the recompiler and the analyser read straight from the word (IBM bit numbering in the comments).
#define PPC_OP(i) (((i) >> 26) & 0x3F)                                  // primary opcode, bits 0-5
#define PPC_XOP(i) (((i) >> 1) & 0x3FF)                                 // extended opcode of X/XL forms, bits 21-30
#define PPC_BO(i) (((i) >> 21) & 0x1F)                                  // BO, bits 6-10
#define PPC_BI(i) ((int32_t)(((uint32_t)(i) & 0x03FFFFFC) << 6) >> 6)   // I-form LI||0b00, sign-extended
#define PPC_BD(i) ((int32_t)(int16_t)((uint32_t)(i) & 0xFFFC))          // B-form BD||0b00, sign-extended
#define PPC_BA(i) (((i) >> 1) & 1)                                      // AA, bit 30
#define PPC_BL(i) ((i) & 1)                                             // LK, bit 31
#define PPC_OP_BC 16
#define PPC_OP_B 18
#define PPC_OP_CTR 19
