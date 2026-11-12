// What decoder_compare reads of a decoded word, the same for both decoders.
#pragma once
#include <cstdint>

struct DecodedWord
{
    int size;
    bool valid;
    const char* name;
    int id;
    uint32_t instruction;
    uint32_t operands[8];
    bool hasOperand[8]; // the instruction has an operand at this index
    char text[160];
};

// Binutils' PowerPC disassembler as XenonRecomp used it before (decoder_compare_reference.cpp, local only).
void ReferenceDecode(uint32_t word, uint64_t address, DecodedWord& out);
