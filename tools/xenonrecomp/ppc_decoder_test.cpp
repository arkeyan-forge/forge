// Checks our PowerPC decoder (ppc_decoder.cpp) on words built from the instruction-set documents: every form, every
// operand field at its limits. No game code. usage: ArkeyanPpcDecoderTest
#include "ppc_decoder.h"
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <ppc-inst.h>

namespace
{
    constexpr uint64_t Address = 0x82000000; // where the decoded words are said to be
    int checks = 0, failures = 0;

    // Expects the word to decode as `name` (id) with these operand values and this operand text.
    void Expect(
        int line, uint32_t word, const char* name, int id, std::initializer_list<uint32_t> operands, const char* text)
    {
        ++checks;
        ppc_insn insn;
        memset(&insn, 0xA5, sizeof(insn));
        const int size = ppc::DecodeWord(word, Address, insn);
        bool ok = size == 4 && insn.instruction == word && insn.opcode != nullptr &&
                  strcmp(insn.opcode->name, name) == 0 && insn.opcode->id == id && strcmp(insn.op_str, text) == 0;
        size_t i = 0;
        for (uint32_t value : operands)
            ok = ok && insn.operands[i++] == value;
        for (; i < 8; i++)
            ok = ok && insn.operands[i] == 0;
        if (!ok)
        {
            ++failures;
            fprintf(stderr, "FAIL line %d: %08X decoded as %s '%s', expected %s '%s'\n", line, word,
                insn.opcode ? insn.opcode->name : "(none)", insn.op_str, name, text);
        }
    }

    void ExpectUnknown(int line, uint32_t word, const char* text)
    {
        ++checks;
        ppc_insn insn;
        ppc::DecodeWord(word, Address, insn);
        if (insn.opcode != nullptr || strcmp(insn.op_str, text) != 0)
        {
            ++failures;
            fprintf(stderr, "FAIL line %d: %08X decoded as %s '%s', expected nothing\n", line, word,
                insn.opcode ? insn.opcode->name : "(none)", insn.op_str);
        }
    }

#define EXPECT(word, name, id, operands, text) Expect(__LINE__, word, name, id, operands, text)
#define OPS(...)                                                                                                       \
    std::initializer_list<uint32_t> { __VA_ARGS__ }

    // Instruction forms [Book I 1.6], IBM bit 0 being the most significant.
    constexpr uint32_t Primary(uint32_t op) { return op << 26; }
    constexpr uint32_t D(uint32_t op, uint32_t rt, uint32_t ra, int32_t d)
    {
        return Primary(op) | rt << 21 | ra << 16 | (uint32_t(d) & 0xFFFF);
    }
    constexpr uint32_t Ds(uint32_t op, uint32_t rt, uint32_t ra, int32_t ds, uint32_t xo)
    {
        return Primary(op) | rt << 21 | ra << 16 | (uint32_t(ds) & 0xFFFC) | xo;
    }
    constexpr uint32_t X(uint32_t op, uint32_t rt, uint32_t ra, uint32_t rb, uint32_t xo, uint32_t rc = 0)
    {
        return Primary(op) | rt << 21 | ra << 16 | rb << 11 | xo << 1 | rc;
    }
    constexpr uint32_t Xo(uint32_t rt, uint32_t ra, uint32_t rb, uint32_t xo, uint32_t oe, uint32_t rc)
    {
        return Primary(31) | rt << 21 | ra << 16 | rb << 11 | oe << 10 | xo << 1 | rc;
    }
    constexpr uint32_t A(uint32_t op, uint32_t frt, uint32_t fra, uint32_t frb, uint32_t frc, uint32_t xo)
    {
        return Primary(op) | frt << 21 | fra << 16 | frb << 11 | frc << 6 | xo << 1;
    }
    constexpr uint32_t M(uint32_t op, uint32_t rs, uint32_t ra, uint32_t sh, uint32_t mb, uint32_t me)
    {
        return Primary(op) | rs << 21 | ra << 16 | sh << 11 | mb << 6 | me << 1;
    }
    // MD form: sh[0:4] in bits 16-20 and sh[5] in bit 30; mb[0:4] in bits 21-25 and mb[5] in bit 26.
    constexpr uint32_t Md(uint32_t rs, uint32_t ra, uint32_t sh, uint32_t mb, uint32_t xo)
    {
        return Primary(30) | rs << 21 | ra << 16 | (sh & 31) << 11 | (mb & 31) << 6 | (mb >> 5) << 5 | xo << 2 |
               (sh >> 5) << 1;
    }
    constexpr uint32_t B(uint32_t bo, uint32_t bi, int32_t bd, uint32_t aa, uint32_t lk)
    {
        return Primary(16) | bo << 21 | bi << 16 | (uint32_t(bd) & 0xFFFC) | aa << 1 | lk;
    }
    constexpr uint32_t Xl(uint32_t bo, uint32_t bi, uint32_t xo, uint32_t lk)
    {
        return Primary(19) | bo << 21 | bi << 16 | xo << 1 | lk;
    }
    // The SPR field holds the number's two 5-bit halves swapped [Book III 4.4.4].
    constexpr uint32_t Spr(uint32_t rt, uint32_t spr, uint32_t xo)
    {
        return Primary(31) | rt << 21 | (spr & 31) << 16 | (spr >> 5) << 11 | xo << 1;
    }
    constexpr uint32_t Vx(uint32_t vd, uint32_t va, uint32_t vb, uint32_t xo)
    {
        return Primary(4) | vd << 21 | va << 16 | vb << 11 | xo;
    }
    constexpr uint32_t Va(uint32_t vd, uint32_t va, uint32_t vb, uint32_t vc, uint32_t xo)
    {
        return Primary(4) | vd << 21 | va << 16 | vb << 11 | vc << 6 | xo;
    }
    // VMX128 register numbers [X128]: VD128 = VD | bits 28-29 << 5; VA128 = VA | bit 26 << 5 | bit 21 << 6;
    // VB128 = VB | bits 30-31 << 5.
    constexpr uint32_t Vd128(uint32_t v) { return (v & 31) << 21 | (v >> 5) << 2; }
    constexpr uint32_t Va128(uint32_t v) { return (v & 31) << 16 | ((v >> 5) & 1) << 5 | (v >> 6) << 10; }
    constexpr uint32_t Vb128(uint32_t v) { return (v & 31) << 11 | v >> 5; }

    void Fixed()
    {
        // D form [Book I 3.3.2, 3.3.8]: displacement and immediate at their limits, RA = 0 as "0".
        EXPECT(D(32, 31, 31, -32768), "lwz", PPC_INST_LWZ, OPS(31, 0xFFFF8000, 31), "r31,-32768(r31)");
        EXPECT(D(32, 0, 0, 32767), "lwz", PPC_INST_LWZ, OPS(0, 32767, 0), "r0,32767(0)");
        EXPECT(D(33, 3, 1, 4), "lwzu", PPC_INST_LWZU, OPS(3, 4, 1), "r3,4(r1)");
        EXPECT(D(37, 1, 1, -112), "stwu", PPC_INST_STWU, OPS(1, 0xFFFFFF90, 1), "r1,-112(r1)");
        EXPECT(D(50, 31, 3, 8), "lfd", PPC_INST_LFD, OPS(31, 8, 3), "f31,8(r3)");
        EXPECT(D(46, 29, 1, -12), "lmw", PPC_INST_LMW, OPS(29, 0xFFFFFFF4, 1), "r29,-12(r1)");
        EXPECT(D(14, 3, 0, -1), "li", PPC_INST_LI, OPS(3, 0xFFFFFFFF), "r3,-1");
        EXPECT(D(14, 3, 1, 32767), "addi", PPC_INST_ADDI, OPS(3, 1, 32767), "r3,r1,32767");
        EXPECT(D(15, 9, 0, -32768), "lis", PPC_INST_LIS, OPS(9, 0xFFFF8000), "r9,-32768");
        EXPECT(D(15, 9, 11, 1), "addis", PPC_INST_ADDIS, OPS(9, 11, 1), "r9,r11,1");
        EXPECT(D(13, 30, 30, -1), "addic.", PPC_INST_ADDIC, OPS(30, 30, 0xFFFFFFFF), "r30,r30,-1");
        EXPECT(D(8, 11, 4, -10002), "subfic", PPC_INST_SUBFIC, OPS(11, 4, 0xFFFFD8EE), "r11,r4,-10002");
        EXPECT(D(7, 11, 11, 252), "mulli", PPC_INST_MULLI, OPS(11, 11, 252), "r11,r11,252");
        EXPECT(0x60000000, "nop", PPC_INST_NOP, OPS(), "");
        EXPECT(D(24, 4, 3, 0xFFFF), "ori", PPC_INST_ORI, OPS(3, 4, 65535), "r3,r4,65535");
        EXPECT(D(28, 4, 3, 0x8000), "andi.", PPC_INST_ANDI, OPS(3, 4, 32768), "r3,r4,32768");
        EXPECT(D(27, 7, 6, 0x8000), "xoris", PPC_INST_XORIS, OPS(6, 7, 32768), "r6,r7,32768");

        // DS form [Book I 3.3.2]: the low two bits are the extended opcode.
        EXPECT(Ds(58, 3, 1, -32768, 0), "ld", PPC_INST_LD, OPS(3, 0xFFFF8000, 1), "r3,-32768(r1)");
        EXPECT(Ds(58, 3, 1, 32764, 1), "ldu", PPC_INST_LDU, OPS(3, 32764, 1), "r3,32764(r1)");
        EXPECT(Ds(58, 11, 1, -28, 2), "lwa", PPC_INST_LWA, OPS(11, 0xFFFFFFE4, 1), "r11,-28(r1)");
        EXPECT(Ds(62, 31, 1, 8, 0), "std", PPC_INST_STD, OPS(31, 8, 1), "r31,8(r1)");
        EXPECT(Ds(62, 9, 10, 8, 1), "stdu", PPC_INST_STDU, OPS(9, 8, 10), "r9,8(r10)");
        ExpectUnknown(__LINE__, Ds(58, 3, 1, 16, 3), ".long 0xe8610013");

        // XO form [Book I 3.3.8]: OE and Rc.
        EXPECT(Xo(31, 31, 31, 266, 0, 1), "add.", PPC_INST_ADD, OPS(31, 31, 31), "r31,r31,r31");
        EXPECT(Xo(1, 2, 3, 266, 1, 0), "addo", PPC_INST_ADDO, OPS(1, 2, 3), "r1,r2,r3");
        EXPECT(Xo(9, 10, 11, 40, 0, 0), "subf", PPC_INST_SUBF, OPS(9, 10, 11), "r9,r10,r11");
        EXPECT(Xo(3, 4, 0, 104, 0, 0), "neg", PPC_INST_NEG, OPS(3, 4), "r3,r4");
        ExpectUnknown(__LINE__, Xo(3, 4, 5, 104, 0, 0), ".long 0x7c6428d0"); // neg's RB is zero
        EXPECT(Xo(8, 11, 10, 459, 0, 0), "divwu", PPC_INST_DIVWU, OPS(8, 11, 10), "r8,r11,r10");
        EXPECT(Xo(11, 3, 7, 75, 0, 0), "mulhw", PPC_INST_MULHW, OPS(11, 3, 7), "r11,r3,r7");
        EXPECT(Xo(10, 10, 0, 234, 0, 0), "addme", PPC_INST_ADDME, OPS(10, 10), "r10,r10");

        // X form logical [Book I 3.3.11] and the extended mnemonics mr, not [B.3].
        EXPECT(X(31, 4, 3, 4, 444), "mr", PPC_INST_MR, OPS(3, 4, 0), "r3,r4");
        EXPECT(X(31, 4, 3, 4, 444, 1), "mr.", PPC_INST_MR, OPS(3, 4, 0), "r3,r4");
        EXPECT(X(31, 4, 3, 5, 444), "or", PPC_INST_OR, OPS(3, 4, 5), "r3,r4,r5");
        EXPECT(X(31, 4, 3, 4, 124), "not", PPC_INST_NOT, OPS(3, 4, 0), "r3,r4");
        EXPECT(X(31, 31, 31, 31, 444), "db16cyc", PPC_INST_DB16CYC, OPS(), ""); // Cell's delay form
        EXPECT(X(31, 1, 1, 1, 444), "cctpl", PPC_INST_CCTPL, OPS(), ""); // Cell's thread priority
        EXPECT(X(31, 3, 4, 0, 26), "cntlzw", PPC_INST_CNTLZW, OPS(4, 3), "r4,r3");
        EXPECT(X(31, 3, 4, 0, 986, 1), "extsw.", PPC_INST_EXTSW, OPS(4, 3), "r4,r3");
        EXPECT(X(31, 9, 10, 31, 824), "srawi", PPC_INST_SRAWI, OPS(10, 9, 31), "r10,r9,31");
        EXPECT(Primary(31) | 3 << 21 | 4 << 16 | 31 << 11 | 413 << 2 | 1 << 1, "sradi", PPC_INST_SRADI, OPS(4, 3, 63),
            "r4,r3,63");

        // Rotates, M and MD forms [Book I 3.3.12] and their extended mnemonics.
        EXPECT(M(21, 4, 3, 31, 0, 31), "rotlwi", PPC_INST_ROTLWI, OPS(3, 4, 31), "r3,r4,31");
        EXPECT(M(21, 4, 3, 0, 31, 31), "clrlwi", PPC_INST_CLRLWI, OPS(3, 4, 31), "r3,r4,31");
        EXPECT(M(21, 4, 3, 2, 0, 29), "rlwinm", PPC_INST_RLWINM, OPS(3, 4, 2, 0, 29), "r3,r4,2,0,29");
        EXPECT(M(20, 31, 3, 0, 0, 0), "rlwimi", PPC_INST_RLWIMI, OPS(3, 31, 0, 0, 0), "r3,r31,0,0,0");
        EXPECT(M(23, 23, 8, 27, 0, 31), "rotlw", PPC_INST_ROTLW, OPS(8, 23, 27), "r8,r23,r27");
        EXPECT(Md(4, 3, 63, 63, 0), "rldicl", PPC_INST_RLDICL, OPS(3, 4, 63, 63), "r3,r4,63,63");
        EXPECT(Md(4, 3, 0, 32, 0), "clrldi", PPC_INST_CLRLDI, OPS(3, 4, 32), "r3,r4,32");
        EXPECT(Md(10, 11, 1, 0, 0), "rotldi", PPC_INST_ROTLDI, OPS(11, 10, 1), "r11,r10,1");
        EXPECT(Md(7, 7, 63, 63, 1), "rldicr", PPC_INST_RLDICR, OPS(7, 7, 63, 63), "r7,r7,63,63");
        EXPECT(Md(4, 11, 32, 0, 3), "rldimi", PPC_INST_RLDIMI, OPS(11, 4, 32, 0), "r11,r4,32,0");

        // Compares [Book I 3.3.9]: cr0 is left out.
        EXPECT(D(11, 7 << 2, 31, -32768), "cmpwi", PPC_INST_CMPWI, OPS(7, 31, 0xFFFF8000), "cr7,r31,-32768");
        EXPECT(D(11, 0, 3, 0), "cmpwi", PPC_INST_CMPWI, OPS(0, 3, 0), "r3,0");
        EXPECT(D(11, 6 << 2 | 1, 3, -1), "cmpdi", PPC_INST_CMPDI, OPS(6, 3, 0xFFFFFFFF), "cr6,r3,-1");
        EXPECT(D(10, 1 << 2 | 1, 3, 0xFFFF), "cmpldi", PPC_INST_CMPLDI, OPS(1, 3, 65535), "cr1,r3,65535");
        EXPECT(X(31, 6 << 2, 11, 8, 0), "cmpw", PPC_INST_CMPW, OPS(6, 11, 8), "cr6,r11,r8");
        EXPECT(X(31, 6 << 2 | 1, 11, 29, 32), "cmpld", PPC_INST_CMPLD, OPS(6, 11, 29), "cr6,r11,r29");

        // Traps [Book I 3.3.10] and their extended mnemonics [B.6].
        EXPECT(D(3, 31, 0, 22), "twi", PPC_INST_TWI, OPS(31, 0, 22), "31,r0,22");
        EXPECT(D(3, 5, 6, -1), "twlgei", PPC_INST_TWLGEI, OPS(6, 0xFFFFFFFF), "r6,-1");
        EXPECT(D(2, 6, 4, 0), "tdllei", PPC_INST_TDLLEI, OPS(4, 0), "r4,0");
        EXPECT(X(31, 4, 3, 4, 4), "tweq", PPC_INST_TWEQ, OPS(3, 4), "r3,r4");
        EXPECT(X(31, 31, 0, 0, 4), "trap", PPC_INST_TRAP, OPS(), "");
        EXPECT(X(31, 3, 3, 4, 4), "tw", PPC_INST_TW, OPS(3, 3, 4), "3,r3,r4");
        EXPECT(X(31, 24, 5, 6, 68), "tdne", PPC_INST_TDNE, OPS(5, 6), "r5,r6");

        // Loads and stores, X form [Book I 3.3.2, 3.3.5]: RA = 0 printed "0".
        EXPECT(X(31, 11, 0, 3, 662), "stwbrx", PPC_INST_STWBRX, OPS(11, 0, 3), "r11,0,r3");
        EXPECT(X(31, 4, 10, 11, 21), "ldx", PPC_INST_LDX, OPS(4, 10, 11), "r4,r10,r11");
        EXPECT(X(31, 10, 0, 11, 20), "lwarx", PPC_INST_LWARX, OPS(10, 0, 11), "r10,0,r11");
        EXPECT(X(31, 3, 2, 23, 20, 1), "lwarx", PPC_INST_LWARX, OPS(3, 2, 23, 1), "r3,r2,r23,1");
        EXPECT(X(31, 10, 0, 11, 150, 1), "stwcx.", PPC_INST_STWCX, OPS(10, 0, 11), "r10,0,r11");
        EXPECT(X(31, 0, 0, 23, 695), "stfsux", PPC_INST_STFSUX, OPS(0, 0, 23), "f0,0,r23");
        EXPECT(X(31, 28, 19, 0, 725), "stswi", PPC_INST_STSWI, OPS(28, 19, 32), "r28,r19,32"); // NB 0 is 32

        // Storage control [Book II 3]: sync's L, dcbt's TH, dcbz's Cell form.
        EXPECT(X(31, 0, 0, 0, 598), "sync", PPC_INST_SYNC, OPS(0), "");
        EXPECT(X(31, 1, 0, 0, 598), "lwsync", PPC_INST_LWSYNC, OPS(), "");
        EXPECT(X(31, 2, 0, 0, 598), "ptesync", PPC_INST_PTESYNC, OPS(), "");
        EXPECT(X(31, 3, 0, 0, 598), "sync", PPC_INST_SYNC, OPS(3), "3");
        EXPECT(X(31, 0, 0, 0, 854), "eieio", PPC_INST_EIEIO, OPS(), "");
        EXPECT(X(31, 0, 0, 4, 278), "dcbt", PPC_INST_DCBT, OPS(0, 0, 4), "r0,r4");
        EXPECT(X(31, 16, 0, 4, 278), "dcbt", PPC_INST_DCBT, OPS(16, 0, 4), "16,r0,r4");
        EXPECT(X(31, 0, 0, 11, 1014), "dcbz", PPC_INST_DCBZ, OPS(0, 11), "r0,r11");
        EXPECT(X(31, 1, 0, 31, 1014), "dcbzl", PPC_INST_DCBZL, OPS(0, 31), "r0,r31");
        EXPECT(X(19, 0, 0, 0, 150), "isync", PPC_INST_ISYNC, OPS(), "");

        // SPRs [Book III, the processor manuals]: named, numbered families, and the general form.
        EXPECT(Spr(0, 8, 339), "mflr", PPC_INST_MFLR, OPS(0), "r0");
        EXPECT(Spr(12, 9, 467), "mtctr", PPC_INST_MTCTR, OPS(12), "r12");
        EXPECT(Spr(6, 1, 467), "mtxer", PPC_INST_MTXER, OPS(6), "r6");
        EXPECT(Spr(3, 20, 339), "mfspr", PPC_INST_MFSPR, OPS(3, 20), "r3,20");
        EXPECT(Spr(3, 1023, 339), "mfpbu2", PPC_INST_MFPBU2, OPS(3), "r3");
        EXPECT(Spr(3, 279, 339), "mfsprg", PPC_INST_MFSPRG, OPS(3, 7), "r3,7");
        EXPECT(Spr(3, 275, 467), "mtsprg", PPC_INST_MTSPRG, OPS(3, 3), "3,r3");
        EXPECT(Spr(3, 543, 339), "mfdbatl", PPC_INST_MFDBATL, OPS(3, 3), "r3,3");
        EXPECT(Spr(11, 268, 371), "mftb", PPC_INST_MFTB, OPS(11, 0), "r11");
        EXPECT(Spr(3, 269, 371), "mftb", PPC_INST_MFTB, OPS(3, 269), "r3,269");
        EXPECT(X(31, 4, 15, 30, 144), "mtcr", PPC_INST_MTCR, OPS(4), "r4");
        EXPECT(X(31, 3, 8, 2, 144), "mtcrf", PPC_INST_MTCRF, OPS(129, 3), "129,r3");
        EXPECT(X(31, 10, 16, 4, 19), "mfocrf", PPC_INST_MFOCRF, OPS(10, 2), "r10,2"); // bit 11 set, one field
        ExpectUnknown(__LINE__, X(31, 11, 23, 30, 19), ".long 0x7d77f026"); // mfocrf with several fields
        EXPECT(X(31, 4, 0, 0, 19), "mfcr", PPC_INST_MFCR, OPS(4), "r4");
        EXPECT(X(31, 13, 1, 0, 178), "mtmsrd", PPC_INST_MTMSRD, OPS(13, 1), "r13,1");
        EXPECT(X(31, 9, 0, 0, 83), "mfmsr", PPC_INST_MFMSR, OPS(9), "r9");
    }

    void Branches()
    {
        // I form [Book I 2.4.1]: LI at its limits, relative and absolute.
        EXPECT(Primary(18) | 0x02000000, "b", PPC_INST_B, OPS(0x80000000), "0x80000000");
        EXPECT(Primary(18) | 0x01FFFFFC, "b", PPC_INST_B, OPS(0x83FFFFFC), "0x83fffffc");
        EXPECT(Primary(18) | 0x03FFFFFD, "bl", PPC_INST_BL, OPS(0x81FFFFFC), "0x81fffffc");
        EXPECT(Primary(18) | 2, "ba", PPC_INST_BA, OPS(0), "0x0");
        EXPECT(Primary(18) | 0x03FFFFFF, "bla", PPC_INST_BLA, OPS(0xFFFFFFFC), "0xfffffffc");

        // B form: BD at its limits; the CR field is left out for cr0; the hint follows BO's "at" bits.
        EXPECT(B(12, 30, -32768, 0, 0), "beq", PPC_INST_BEQ, OPS(7, 0x81FF8000), "cr7,0x81ff8000");
        EXPECT(B(12, 2, 32764, 0, 0), "beq", PPC_INST_BEQ, OPS(0, 0x82007FFC), "0x82007ffc");
        EXPECT(B(4, 26, 16, 0, 0), "bne", PPC_INST_BNE, OPS(6, 0x82000010), "cr6,0x82000010");
        EXPECT(B(6, 2, 16, 0, 0), "bne-", PPC_INST_BNE, OPS(0, 0x82000010), "0x82000010");
        EXPECT(B(15, 0, 16, 0, 0), "blt+", PPC_INST_BLT, OPS(0, 0x82000010), "0x82000010");
        EXPECT(B(13, 1, 16, 0, 0), "bgt", PPC_INST_BGT, OPS(0, 0x82000010), "0x82000010"); // a = 0: no hint
        EXPECT(B(4, 0, 16, 0, 1), "bgel", PPC_INST_BGEL, OPS(0, 0x82000010), "0x82000010");
        EXPECT(B(4, 31, 16, 1, 0), "bnsa", PPC_INST_BNSA, OPS(7, 16), "cr7,0x10");
        EXPECT(B(16, 0, -4, 0, 0), "bdnz", PPC_INST_BDNZ, OPS(0x81FFFFFC), "0x81fffffc");
        EXPECT(B(25, 0, -4, 0, 0), "bdnz+", PPC_INST_BDNZ, OPS(0x81FFFFFC), "0x81fffffc");
        EXPECT(B(26, 0, 8, 0, 0), "bdz-", PPC_INST_BDZ, OPS(0x82000008), "0x82000008");
        EXPECT(B(0, 26, 8, 0, 0), "bdnzf", PPC_INST_BDNZF, OPS(26, 0x82000008), "4*cr6+eq,0x82000008");
        EXPECT(B(8, 2, 8, 0, 0), "bdnzt", PPC_INST_BDNZT, OPS(2, 0x82000008), "eq,0x82000008");
        EXPECT(B(20, 0, 16, 0, 0), "bc", PPC_INST_BC, OPS(20, 0, 0x82000010), "20,lt,0x82000010");
        EXPECT(B(16, 1, 16, 0, 0), "bc", PPC_INST_BC, OPS(16, 1, 0x82000010), "16,gt,0x82000010");
        EXPECT(B(25, 31, 16, 0, 0), "bc+", PPC_INST_BC, OPS(24, 31, 0x82000010), "24,4*cr7+so,0x82000010");
        ExpectUnknown(__LINE__, B(21, 0, 16, 0, 0), ".long 0x42a00010"); // BO 10101 is not defined

        // XL form: bclr, bcctr.
        EXPECT(Xl(20, 0, 16, 0), "blr", PPC_INST_BLR, OPS(), "");
        EXPECT(Xl(20, 0, 16, 1), "blrl", PPC_INST_BLRL, OPS(), "");
        EXPECT(Xl(20, 0, 528, 0), "bctr", PPC_INST_BCTR, OPS(), "");
        EXPECT(Xl(20, 0, 528, 1), "bctrl", PPC_INST_BCTRL, OPS(), "");
        EXPECT(Xl(12, 26, 16, 0), "beqlr", PPC_INST_BEQLR, OPS(6), "cr6");
        EXPECT(Xl(13, 2, 16, 0), "beqlr+", PPC_INST_BEQLR, OPS(0), "");
        EXPECT(Xl(14, 2, 16, 0), "beqlr-", PPC_INST_BEQLR, OPS(0), "");
        EXPECT(Xl(4, 2, 528, 0), "bnectr", PPC_INST_BNECTR, OPS(0), "");
        EXPECT(Xl(18, 0, 16, 0), "bdzlr", PPC_INST_BDZLR, OPS(), "");
        EXPECT(Xl(17, 0, 16, 0), "bdnzlr+", PPC_INST_BDNZLR, OPS(), "");
        EXPECT(Xl(1, 3, 16, 0), "bdnzflr+", PPC_INST_BDNZFLR, OPS(3), "so");
        EXPECT(Xl(17, 1, 16, 0), "bclr+", PPC_INST_BCLR, OPS(16, 1), "16,gt");
        EXPECT(Xl(16, 2, 528, 0), "bcctr-", PPC_INST_BCCTR, OPS(16, 2), "16,eq");
        EXPECT(Xl(20, 0, 16, 0) | 3 << 11, "bclr", PPC_INST_BCLR, OPS(20, 0, 3), "20,lt,3"); // BH = 3
        ExpectUnknown(__LINE__, Xl(1, 0, 528, 0), ".long 0x4c200420");

        // sc [Book I 2.4.3], CR logical [Book I 2.4.4] and their extended mnemonics.
        EXPECT(Primary(17) | 2, "sc", PPC_INST_SC, OPS(0), "");
        EXPECT(Primary(17) | 1 << 5 | 2, "sc", PPC_INST_SC, OPS(1), "1");
        EXPECT(X(19, 6, 6, 6, 289), "crset", PPC_INST_CRSET, OPS(6, 0, 0), "4*cr1+eq");
        EXPECT(X(19, 2, 2, 2, 193), "crclr", PPC_INST_CRCLR, OPS(2, 0, 0), "eq");
        EXPECT(X(19, 2, 3, 2, 33), "crnor", PPC_INST_CRNOR, OPS(2, 3, 2), "eq,so,eq");
        EXPECT(X(19, 31, 3, 3, 33), "crnot", PPC_INST_CRNOT, OPS(31, 3, 0), "4*cr7+so,so");
        EXPECT(X(19, 1 << 2, 3 << 2, 0, 0), "mcrf", PPC_INST_MCRF, OPS(1, 3), "cr1,cr3");
    }

    void Float()
    {
        // A form [Book I 4.6]: printed "fT,fA,fC,fB".
        EXPECT(A(63, 1, 2, 4, 3, 29), "fmadd", PPC_INST_FMADD, OPS(1, 2, 3, 4), "f1,f2,f3,f4");
        EXPECT(A(59, 31, 31, 31, 31, 31) | 1, "fnmadds.", PPC_INST_FNMADDS, OPS(31, 31, 31, 31), "f31,f31,f31,f31");
        EXPECT(A(63, 1, 31, 0, 30, 25), "fmul", PPC_INST_FMUL, OPS(1, 31, 30), "f1,f31,f30");
        EXPECT(A(63, 1, 31, 30, 0, 21), "fadd", PPC_INST_FADD, OPS(1, 31, 30), "f1,f31,f30");
        EXPECT(A(59, 3, 0, 4, 0, 22), "fsqrts", PPC_INST_FSQRTS, OPS(3, 4), "f3,f4");
        EXPECT(A(63, 1, 1, 0, 1, 23) | 0 << 11, "fsel", PPC_INST_FSEL, OPS(1, 1, 1, 0), "f1,f1,f1,f0");
        EXPECT(A(59, 29, 0, 18, 0, 24), "fres", PPC_INST_FRES, OPS(29, 18, 0), "f29,f18");
        EXPECT(A(59, 29, 1, 18, 0, 24), "fres", PPC_INST_FRES, OPS(29, 18, 1), "f29,f18,1");

        // X form.
        EXPECT(X(63, 7 << 2, 0, 31, 0), "fcmpu", PPC_INST_FCMPU, OPS(7, 0, 31), "cr7,f0,f31");
        EXPECT(X(63, 0, 0, 0, 0), "fcmpu", PPC_INST_FCMPU, OPS(0, 0, 0), "cr0,f0,f0");
        EXPECT(X(63, 13, 0, 0, 15), "fctiwz", PPC_INST_FCTIWZ, OPS(13, 0), "f13,f0");
        EXPECT(X(63, 31, 0, 1, 72, 1), "fmr.", PPC_INST_FMR, OPS(31, 1), "f31,f1");
        EXPECT(X(63, 0, 0, 0, 583), "mffs", PPC_INST_MFFS, OPS(0), "f0");
        EXPECT(X(63, 0, 0, 0, 70), "mtfsb0", PPC_INST_MTFSB0, OPS(0), "lt");
        // mtfsf: FLM, FRB, then L (bit 6) and W (bit 15), printed when either is set.
        EXPECT(Primary(63) | 0xFF << 17 | 711 << 1, "mtfsf", PPC_INST_MTFSF, OPS(255, 0, 0, 0), "255,f0");
        EXPECT(Primary(63) | 1 << 25 | 19 << 17 | 4 << 11 | 711 << 1, "mtfsf", PPC_INST_MTFSF, OPS(19, 4, 1, 0),
            "19,f4,1,0");
        EXPECT(Primary(63) | 93 << 17 | 1 << 16 | 11 << 11 | 711 << 1, "mtfsf", PPC_INST_MTFSF, OPS(93, 11, 0, 1),
            "93,f11,0,1");
    }

    void Vmx()
    {
        // VX, VA and VC forms [VMX 6].
        EXPECT(Vx(31, 0, 31, 10), "vaddfp", PPC_INST_VADDFP, OPS(31, 0, 31), "v31,v0,v31");
        EXPECT(Vx(0, 16, 0, 908), "vspltisw", PPC_INST_VSPLTISW, OPS(0, 0xFFFFFFF0), "v0,-16");
        EXPECT(Vx(0, 15, 0, 908), "vspltisw", PPC_INST_VSPLTISW, OPS(0, 15), "v0,15");
        EXPECT(Vx(12, 3, 1, 652), "vspltw", PPC_INST_VSPLTW, OPS(12, 1, 3), "v12,v1,3");
        EXPECT(Vx(0, 31, 13, 778), "vcfux", PPC_INST_VCFUX, OPS(0, 13, 31), "v0,v13,31");
        EXPECT(Vx(0, 0, 13, 266), "vrefp", PPC_INST_VREFP, OPS(0, 13), "v0,v13");
        EXPECT(Vx(3, 0, 0, 1540), "mfvscr", PPC_INST_MFVSCR, OPS(3), "v3");
        EXPECT(Vx(13, 9, 10, 6), "vcmpequb", PPC_INST_VCMPEQUB, OPS(13, 9, 10), "v13,v9,v10");
        EXPECT(Vx(13, 9, 10, 134 | 1 << 10), "vcmpequw.", PPC_INST_VCMPEQUW, OPS(13, 9, 10), "v13,v9,v10");
        EXPECT(Va(1, 2, 3, 4, 43), "vperm", PPC_INST_VPERM, OPS(1, 2, 3, 4), "v1,v2,v3,v4");
        EXPECT(Va(1, 2, 4, 3, 46), "vmaddfp", PPC_INST_VMADDFP, OPS(1, 2, 3, 4), "v1,v2,v3,v4");
        EXPECT(Va(0, 0, 0, 15, 44), "vsldoi", PPC_INST_VSLDOI, OPS(0, 0, 0, 15), "v0,v0,v0,15");

        // Vector loads and stores [VMX 6, CBE].
        EXPECT(X(31, 31, 0, 31, 103), "lvx", PPC_INST_LVX, OPS(31, 0, 31), "v31,r0,r31");
        EXPECT(X(31, 0, 0, 9, 519), "lvlx", PPC_INST_LVLX, OPS(0, 0, 9), "v0,0,r9");
        EXPECT(X(31, 0, 28, 10, 679), "stvrx", PPC_INST_STVRX, OPS(0, 28, 10), "v0,r28,r10");
        EXPECT(X(31, 2, 4, 6, 342), "dst", PPC_INST_DST, OPS(4, 6, 2), "r4,r6,2");
        EXPECT(X(31, 3, 0, 0, 822), "dss", PPC_INST_DSS, OPS(3), "3");
        EXPECT(X(31, 16, 0, 0, 822), "dssall", PPC_INST_DSSALL, OPS(), "");
    }

    void Vmx128()
    {
        // Loads and stores (opcode 4): VD128 at its limit, RA printed as a register.
        EXPECT(Primary(4) | Vd128(127) | 31 << 11 | 0x0C3, "lvx128", PPC_INST_LVX128, OPS(127, 0, 31), "v127,r0,r31");
        EXPECT(Primary(4) | Vd128(63) | 6 << 16 | 11 << 11 | 0x543, "stvrx128", PPC_INST_STVRX128, OPS(63, 6, 11),
            "v63,r6,r11");

        // Opcode 5.
        EXPECT(Primary(5) | Vd128(127) | Va128(127) | Vb128(127) | 0x010, "vaddfp128", PPC_INST_VADDFP128,
            OPS(127, 127, 127), "v127,v127,v127");
        EXPECT(Primary(5) | Vd128(64) | Va128(32) | Vb128(96) | 0x010, "vaddfp128", PPC_INST_VADDFP128, OPS(64, 32, 96),
            "v64,v32,v96");
        EXPECT(Primary(5) | Vd128(1) | Va128(2) | Vb128(3) | 7 << 6, "vperm128", PPC_INST_VPERM128, OPS(1, 2, 3, 7),
            "v1,v2,v3,v7");
        EXPECT(Primary(5) | Vd128(13) | Va128(12) | Vb128(125) | 0x0D0, "vmaddfp128", PPC_INST_VMADDFP128,
            OPS(13, 12, 125, 13), "v13,v12,v125,v13"); // vD = vA * vB + vD
        EXPECT(Primary(5) | Vd128(12) | Va128(126) | Vb128(127) | 0x110, "vmaddcfp128", PPC_INST_VMADDCFP128,
            OPS(12, 126, 12, 127), "v12,v126,v12,v127"); // vD = vA * vD + vB
        EXPECT(Primary(5) | Vd128(23) | Va128(21) | Vb128(22) | 0x350, "vsel128", PPC_INST_VSEL128, OPS(23, 21, 22, 23),
            "v23,v21,v22,v23");

        // Opcode 6.
        EXPECT(Primary(6) | Vd128(1) | 31 << 16 | Vb128(2) | 7 << 6 | 0x210, "vpermwi128", PPC_INST_VPERMWI128,
            OPS(1, 2, 255), "v1,v2,255");
        EXPECT(Primary(6) | Vd128(1) | 31 << 16 | Vb128(2) | 3 << 6 | 0x710, "vrlimi128", PPC_INST_VRLIMI128,
            OPS(1, 2, 31, 3), "v1,v2,31,3");
        EXPECT(Primary(6) | Vd128(38) | 7 << 18 | 2 << 16 | Vb128(39) | 3 << 6 | 0x610, "vpkd3d128", PPC_INST_VPKD3D128,
            OPS(38, 39, 7, 2, 3), "v38,v39,7,2,3");
        EXPECT(Primary(6) | Vd128(35) | 20 << 16 | Vb128(36) | 0x7F0, "vupkd3d128", PPC_INST_VUPKD3D128,
            OPS(35, 36, 20), "v35,v36,20");
        EXPECT(Primary(6) | Vd128(63) | 16 << 16 | 0x770, "vspltisw128", PPC_INST_VSPLTISW128, OPS(63, 0xFFFFFFF0),
            "v63,-16");
        EXPECT(Primary(6) | Vd128(0) | Vb128(104) | 0x630, "vrefp128", PPC_INST_VREFP128, OPS(0, 104), "v0,v104");
        EXPECT(Primary(6) | Vd128(43) | Va128(44) | Vb128(55) | 0x200 | 0x40, "vcmpequw128.", PPC_INST_VCMPEQUW128,
            OPS(43, 44, 55), "v43,v44,v55");
        EXPECT(Primary(6) | Vd128(62) | Va128(96) | Vb128(8) | 0x380, "vupkhsb128", PPC_INST_VUPKHSB128, OPS(62, 8, 96),
            "v62,v8,v96");
        // vsldoi128: opcode 4 with bit 27 set, the shift in bits 22-25.
        EXPECT(Primary(4) | Vd128(36) | Va128(37) | Vb128(37) | 8 << 6 | 0x10, "vsldoi128", PPC_INST_VSLDOI128,
            OPS(36, 37, 37, 8), "v36,v37,v37,8");
    }

    // Every word of opcode 4 with bit 27 set is VMX128's vsldoi128: the Xenon has none of the PPC405 multiply-accumulate
    // forms (machhw, macchw, nmachhw...) that share these encodings. 2^25 words.
    void Vsldoi128Space()
    {
        ++checks;
        uint32_t wrong = 0, first = 0;
        for (uint32_t rest = 0; rest < (1u << 25); rest++)
        {
            const uint32_t word = Primary(4) | 0x10 | ((rest << 1) & 0x03FFFFE0) | (rest & 0xF);
            ppc_insn insn;
            ppc::DecodeWord(word, Address, insn);
            if (insn.opcode == nullptr || insn.opcode->id != PPC_INST_VSLDOI128)
            {
                if (wrong++ == 0)
                    first = word;
            }
        }
        if (wrong != 0)
        {
            ++failures;
            fprintf(stderr, "FAIL: %u of the %u vsldoi128 words decode as something else (first %08X)\n", wrong,
                1u << 25, first);
        }
    }

    void Unknown()
    {
        ExpectUnknown(__LINE__, 0x00000000, ".long 0x0");
        ExpectUnknown(__LINE__, 0x44000001, ".long 0x44000001"); // sc without bit 30
    }
} // namespace

int main()
{
    Fixed();
    Branches();
    Float();
    Vmx();
    Vmx128();
    Vsldoi128Space();
    Unknown();
    printf("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
