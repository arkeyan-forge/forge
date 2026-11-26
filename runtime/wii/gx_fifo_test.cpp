// GX's FIFO on the host (docs/runtime/wii/gx.md): the write-gather pipe's lines in the CPU's FIFO, and the command
// processor on hand-made commands. No game data.
#include "wii/gx_fifo.h"
#include "wii/memory_map.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    using namespace wii::gx;
    uint8_t* g_base = nullptr;

    void Put16(uint32_t address, uint32_t value)
    {
        g_base[address] = uint8_t(value >> 8);
        g_base[address + 1] = uint8_t(value);
    }
    void Put32(uint32_t address, uint32_t value)
    {
        Put16(address, value >> 16);
        Put16(address + 2, value);
    }
    uint32_t Get32(uint32_t address)
    {
        return uint32_t(g_base[address]) << 24 | uint32_t(g_base[address + 1]) << 16 |
               uint32_t(g_base[address + 2]) << 8 | g_base[address + 3];
    }

    // A command stream built as the SDK's GX writes it.
    struct Stream
    {
        std::vector<uint8_t> bytes;
        void U8(uint32_t v) { bytes.push_back(uint8_t(v)); }
        void U16(uint32_t v)
        {
            U8(v >> 8);
            U8(v);
        }
        void U32(uint32_t v)
        {
            U16(v >> 16);
            U16(v);
        }
        void Float(float f)
        {
            uint32_t bits;
            memcpy(&bits, &f, 4);
            U32(bits);
        }
        void Cp(uint8_t index, uint32_t value)
        {
            U8(LoadCp);
            U8(index);
            U32(value);
        }
        void Bp(uint8_t index, uint32_t value)
        {
            U8(LoadBp);
            U32(uint32_t(index) << 24 | (value & 0xFFFFFF));
        }
        void Xf(uint16_t address, std::initializer_list<uint32_t> values)
        {
            U8(LoadXf);
            U16(uint32_t(values.size() - 1));
            U16(address);
            for (uint32_t v : values)
                U32(v);
        }
    };

    // The pipe's stores, as the game's code makes them (bytes, halves, words).
    void Gather(const std::vector<uint8_t>& bytes)
    {
        size_t at = 0;
        for (; at + 4 <= bytes.size(); at += 4)
            GatherWrite(g_base,
                uint32_t(bytes[at]) << 24 | uint32_t(bytes[at + 1]) << 16 | uint32_t(bytes[at + 2]) << 8 |
                    bytes[at + 3],
                4);
        for (; at < bytes.size(); at++)
            GatherWrite(g_base, bytes[at], 1);
    }
    void Flush() // GXFlush: 32 zero bytes, which push the last line out
    {
        for (int i = 0; i < 8; i++)
            GatherWrite(g_base, 0, 4);
    }

    constexpr uint32_t Fifo = 0x00100000, FifoSize = 0x100; // physical, in MEM1

    void SetFifo(bool linked)
    {
        Put32(pi::FifoBase, Fifo);
        Put32(pi::FifoEnd, Fifo + FifoSize - 4);
        Put32(pi::FifoWrite, Fifo);
        Put16(cp::Control, linked ? (cp::ReadEnable | cp::Linked) : 0);
    }

    void UnlinkedLines()
    {
        SetFifo(false);
        Processor().ResetCounters();
        std::vector<uint8_t> line(32);
        for (uint32_t i = 0; i < 32; i++)
            line[i] = uint8_t(0xA0 + i);
        Gather(line);
        EXPECT(memcmp(g_base + 0x80000000 + Fifo, line.data(), 32) == 0);
        EXPECT(Get32(pi::FifoWrite) == Fifo + 32);
        EXPECT(Processor().Counters().commands == 0); // a display list being built: nothing run
        // Up to the end, then the base again with the wrap bit.
        for (uint32_t i = 1; i < FifoSize / 32; i++)
            Gather(line);
        EXPECT(Get32(pi::FifoWrite) == (Fifo | pi::Wrapped));
        Gather(line);
        EXPECT(Get32(pi::FifoWrite) == ((Fifo + 32) | pi::Wrapped));
        // A byte store and a word store sharing a line keep their order.
        SetFifo(false);
        GatherWrite(g_base, 0x11, 1);
        GatherWrite(g_base, 0x22334455, 4);
        for (int i = 0; i < 27; i++)
            GatherWrite(g_base, 0, 1);
        EXPECT(g_base[0x80000000 + Fifo] == 0x11 && Get32(0x80000000 + Fifo + 1) == 0x22334455);

        // Redirected (GXRedirectWriteGatherPipe): a buffer in MEM2, past the 64 MB end it sets, is written in place.
        constexpr uint32_t Redirect = 0x10200000;
        Put32(pi::FifoBase, 0);
        Put32(pi::FifoEnd, 0x04000000);
        Put32(pi::FifoWrite, Redirect);
        Gather(line);
        Gather(line);
        EXPECT(Get32(pi::FifoWrite) == Redirect + 64);
        EXPECT(memcmp(g_base + 0x80000000 + Redirect + 32, line.data(), 32) == 0);
        EXPECT(g_base[0x80000000] == 0); // nothing over low memory
        // The line begun is emptied by a write of the pipe's address register (the SDK's padding after the last).
        SetFifo(false);
        GatherWrite(g_base, 0x77, 1);
        GatherReset();
        Gather(line);
        EXPECT(Get32(pi::FifoWrite) == Fifo + 32 && g_base[0x80000000 + Fifo] == line[0]);
    }

    void LinkedCommands()
    {
        SetFifo(true);
        CommandProcessor& gp = Processor();
        gp.Reset();
        gp.ResetCounters();
        std::vector<uint32_t> copies;
        std::vector<DrawCall> draws;
        std::vector<float> firstVertex;
        gp.onCopy = [&](uint32_t value) { copies.push_back(value); };
        gp.onDraw = [&](const DrawCall& draw) {
            draws.push_back(draw);
            for (int i = 0; i < 3; i++)
            {
                const uint8_t* p = draw.vertices + 4 * i;
                const uint32_t bits = uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
                float f;
                memcpy(&f, &bits, 4);
                firstVertex.push_back(f);
            }
        };

        Stream s;
        s.Cp(0x50, 1 << 9); // the position direct
        s.Cp(0x70, 1 | 4 << 1); // format 0: xyz, f32
        s.Xf(0x1000, { 0x12345678, 0x9ABCDEF0 });
        s.U8(Draw | uint8_t(Primitive::Triangles) << 3 | 0);
        s.U16(3);
        for (int v = 0; v < 3; v++)
            for (int c = 0; c < 3; c++)
                s.Float(float(v * 10 + c));
        s.Bp(0xFE, 0x00FF00); // the next BP write: only its middle byte
        s.Bp(0x41, 0xABCDEF);
        s.Bp(0x52, 0x004000); // an EFB copy to the external frame buffer
        Gather(s.bytes);
        Flush();
        EXPECT(gp.Cp(0x50) == 1 << 9 && gp.Xf(0x1000) == 0x12345678 && gp.Xf(0x1001) == 0x9ABCDEF0);
        EXPECT(draws.size() == 1 && draws[0].primitive == Primitive::Triangles && draws[0].count == 3 &&
               draws[0].stride == 12);
        EXPECT(firstVertex.size() == 3 && firstVertex[0] == 0 && firstVertex[1] == 1 && firstVertex[2] == 2);
        EXPECT(gp.Bp(0x41) == 0x00CD00); // masked
        EXPECT(copies.size() == 1 && copies[0] == 0x004000 && gp.Counters().copies == 1);
        EXPECT(gp.Counters().draws == 1 && gp.Counters().vertices == 3 && gp.Counters().unknown == 0);
        // Nothing left for the GP to read: its read and write pointers at the CPU's.
        const uint32_t write = Get32(pi::FifoWrite) & pi::WriteMask;
        EXPECT((uint32_t(g_base[cp::FifoRead + 2]) << 24 | uint32_t(g_base[cp::FifoRead + 3]) << 16 |
                   uint32_t(g_base[cp::FifoRead]) << 8 | g_base[cp::FifoRead + 1]) == write);
        gp.onCopy = nullptr;
        gp.onDraw = nullptr;
    }

    void DisplayLists()
    {
        SetFifo(true);
        CommandProcessor& gp = Processor();
        gp.Reset();
        gp.ResetCounters();
        // A list in memory: a CP write and a draw of 2 points (8-bit indexed positions).
        Stream list;
        list.Cp(0x50, 2 << 9);
        list.U8(Draw | uint8_t(Primitive::Points) << 3 | 1);
        list.U16(2);
        list.U8(4);
        list.U8(5);
        while (list.bytes.size() % 32 != 0)
            list.U8(Nop);
        constexpr uint32_t List = 0x00200000;
        memcpy(g_base + 0x80000000 + List, list.bytes.data(), list.bytes.size());
        Stream call;
        call.U8(CallList);
        call.U32(List);
        call.U32(uint32_t(list.bytes.size()));
        call.U8(CallList); // a list cut in its draw
        call.U32(List);
        call.U32(8);
        Gather(call.bytes);
        Flush();
        EXPECT(gp.Counters().lists == 2 && gp.Counters().badLists == 1);
        EXPECT(gp.Cp(0x50) == 2 << 9 && gp.Counters().draws == 1 && gp.Counters().vertices == 2);

        // An indexed load: XF words from array 12 (base and stride in CP 0xAC, 0xBC).
        constexpr uint32_t Array = 0x00300000;
        Put32(0x80000000 + Array + 2 * 16, 0x01020304);
        Put32(0x80000000 + Array + 2 * 16 + 4, 0x05060708);
        Stream indexed;
        indexed.Cp(0xAC, Array);
        indexed.Cp(0xBC, 16);
        indexed.U8(LoadIndexA);
        indexed.U32(2u << 16 | 1u << 12 | 0x030); // index 2, 2 words, to XF 0x030
        Gather(indexed.bytes);
        Flush();
        EXPECT(gp.Xf(0x030) == 0x01020304 && gp.Xf(0x031) == 0x05060708);
    }

    void VertexSizes()
    {
        CommandProcessor gp;
        Stream s;
        // Everything direct: matrix index, xyz s16, normal s8, colour 0 RGBA8888, colour 1 RGB565, texture 0 st u16.
        s.Cp(0x50, 1 | 1 << 9 | 1 << 11 | 1 << 13 | 1 << 15);
        s.Cp(0x60, 1);
        s.Cp(0x70, 1 | 3 << 1 | 1 << 10 | 5 << 14 | 0 << 18 | 1 << 21 | 2 << 22);
        gp.Feed(nullptr, s.bytes.data(), s.bytes.size());
        EXPECT(gp.VertexSize(0) == 1 + 6 + 3 + 4 + 2 + 4);
        // Format 1: positions by 16-bit index, an NBT normal by three 8-bit ones, texture 7's s in f32.
        Stream t;
        t.Cp(0x50, 3 << 9 | 2 << 11);
        t.Cp(0x60, 1 << 14);
        t.Cp(0x71, 0 | 1 << 9 | 1u << 31);
        t.Cp(0x91, 0 << 23 | 4 << 24);
        gp.Feed(nullptr, t.bytes.data(), t.bytes.size());
        EXPECT(gp.VertexSize(1) == 2 + 3 + 4);
        // A command cut between two pieces runs once whole.
        Stream u;
        u.Cp(0x60, 0);
        gp.Feed(nullptr, u.bytes.data(), 3);
        EXPECT(gp.Cp(0x60) == 1 << 14);
        gp.Feed(nullptr, u.bytes.data() + 3, u.bytes.size() - 3);
        EXPECT(gp.Cp(0x60) == 0);
        // An opcode not known: the stream stops there until it starts over.
        const uint8_t unknown[] = { 0x02, LoadCp, 0x60, 0, 0, 0, 1 };
        gp.Feed(nullptr, unknown, sizeof(unknown));
        EXPECT(gp.Counters().unknown == 1 && gp.Cp(0x60) == 0);
        gp.Reset();
        gp.Feed(nullptr, unknown + 1, sizeof(unknown) - 1);
        EXPECT(gp.Cp(0x60) == 1);
    }
}

int main()
{
    std::string error;
    if (!wii::MapAddressSpace(g_base, {}, error))
    {
        fprintf(stderr, "cannot map the address space: %s\n", error.c_str());
        return 1;
    }
    UnlinkedLines();
    LinkedCommands();
    DisplayLists();
    VertexSizes();
    printf("gx fifo test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
