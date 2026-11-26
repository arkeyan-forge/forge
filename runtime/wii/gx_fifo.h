#pragma once

// The Wii's graphics FIFO on the host (docs/runtime/wii/gx.md). The game writes GX's commands to the write-gather pipe
// (0xCC008000), which gathers them in 32-byte lines and writes each line to the CPU's FIFO in memory (the processor
// interface's base, end and write pointer). When that FIFO is also the graphics processor's (linked, GX's immediate
// mode), the host's command processor reads each line as it is written; otherwise (a display list being built) the
// lines stay in memory for a later call. The command processor keeps the state the commands load (its own registers,
// the transform unit's and the blitting processor's) and walks each draw's vertices.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace wii::gx
{
    constexpr uint32_t GatherPipe = 0xCC008000;
    constexpr uint32_t GatherLine = 32;

    // The registers' blocks the SDK reaches through its pointers (__piReg, __cpReg) [its GXSetCPUFifo, GXSetGPFifo].
    namespace pi
    {
        constexpr uint32_t FifoBase = 0xCC00300C, FifoEnd = 0xCC003010, FifoWrite = 0xCC003014;
        constexpr uint32_t WriteMask = 0x1FFFFFE0; // the write pointer's address bits; the bit above, its wrap
        constexpr uint32_t Wrapped = 0x20000000;
    }
    namespace cp
    {
        constexpr uint32_t Control = 0xCC000002; // 16 bits: 1 the GP reads its FIFO, 0x10 that FIFO is the CPU's
        constexpr uint16_t ReadEnable = 0x1, Linked = 0x10;
        constexpr uint32_t FifoBase = 0xCC000020, FifoEnd = 0xCC000024; // each two 16-bit halves, the low one first
        constexpr uint32_t FifoWrite = 0xCC000034, FifoRead = 0xCC000038;
        constexpr uint32_t Distance = 0xCC000030; // read-to-write bytes
    }

    // The FIFO's commands [their opcodes, the first byte].
    enum Opcode : uint8_t
    {
        Nop = 0x00,
        LoadCp = 0x08,     // a register (8 bits), its value (32)
        LoadXf = 0x10,     // count - 1 and an address (16 bits each), then that many words
        LoadIndexA = 0x20, // an index, size - 1 and an address (16, 4, 12 bits): XF words from an array (12-15)
        LoadIndexB = 0x28,
        LoadIndexC = 0x30,
        LoadIndexD = 0x38,
        CallList = 0x40,   // a display list: its address and size (32 bits each)
        InvalidateVertexCache = 0x48,
        LoadBp = 0x61,     // the register in the top byte, 24 bits of value
        Draw = 0x80,       // 0x80-0xBF: the primitive in bits 3-5, the vertex format in 0-2, then a count (16 bits)
    };

    enum class Primitive : uint8_t
    {
        Quads = 0,
        Quads2 = 1,
        Triangles = 2,
        TriangleStrip = 3,
        TriangleFan = 4,
        Lines = 5,
        LineStrip = 6,
        Points = 7,
    };

    struct DrawCall
    {
        Primitive primitive;
        uint8_t format;     // the vertex format (VAT) 0-7
        uint16_t count;     // vertices
        const uint8_t* vertices; // their bytes, `stride` each, in the FIFO or the display list (valid during the call)
        uint32_t stride;
    };

    struct Stats
    {
        uint32_t lines = 0; // gathered lines written
        uint32_t commands = 0; // read by the command processor
        uint32_t draws = 0;
        uint32_t vertices = 0;
        uint32_t lists = 0; // display lists called
        uint32_t copies = 0; // EFB copies (BP 0x52)
        uint32_t unknown = 0; // opcodes not known: the rest of that stream skipped
        uint32_t badLists = 0; // display lists out of memory or cut in a command
    };

    class CommandProcessor
    {
    public:
        // The bytes of a FIFO's commands, in order, in any pieces: a command cut by a piece's end waits for the next.
        void Feed(const uint8_t* base, const uint8_t* bytes, size_t size);

        // A display list's commands, whole (CallList, or a test).
        void RunList(const uint8_t* base, uint32_t address, uint32_t size);

        // The bytes one vertex of `format` takes, from the vertex descriptor (CP 0x50, 0x60) and the format's
        // attributes (CP 0x70, 0x80, 0x90 + format).
        uint32_t VertexSize(uint8_t format) const;

        uint32_t Cp(uint8_t index) const { return cpRegisters[index]; }
        uint32_t Bp(uint8_t index) const { return bpRegisters[index]; }
        uint32_t Xf(uint16_t address) const { return address < sizeof(xfMemory) / 4 ? xfMemory[address] : 0; }
        const Stats& Counters() const { return stats; }
        void ResetCounters() { stats = {}; }
        void CountLine() { stats.lines++; }

        std::function<void(const DrawCall&)> onDraw; // each draw, its vertices with it
        std::function<void(uint32_t bp52)> onCopy; // each EFB copy, with its BP 0x52 value
        std::function<void(uint8_t index, uint32_t value)> onBp; // each BP register written, after the write

    private:
        // The length of the command at `bytes` (`size` available), 0 if it is cut, ~0 if its opcode is not known.
        size_t CommandSize(const uint8_t* bytes, size_t size) const;
        void Run(const uint8_t* base, const uint8_t* command, int depth);
        void LoadIndexed(const uint8_t* base, uint8_t array, uint32_t word);

        uint32_t cpRegisters[256] = {};
        uint32_t bpRegisters[256] = {};
        uint32_t bpMask = 0xFFFFFF;
        uint32_t xfMemory[0x1058] = {}; // matrices (0x000-0x4FF), normals, lights (0x600-), registers (0x1000-)
        std::vector<uint8_t> pending; // the stream's bytes not run yet: a command cut by a piece's end
        bool skipping = false; // after an unknown opcode, until the stream starts over (Reset)
        Stats stats;

    public:
        void Reset()
        {
            pending.clear();
            skipping = false;
        }
    };

    // The host's graphics processor (one, as on the console).
    CommandProcessor& Processor();

    // A store to the write-gather pipe: `bytes` of `value` (big-endian, as the CPU writes them).
    void GatherWrite(uint8_t* base, uint64_t value, uint32_t bytes);
    // The pipe's address register (WPAR, the processor's SPR 921): its read has the "not empty" bit clear, each line
    // already gone at its 32nd byte; its write (GXRedirectWriteGatherPipe, GXRestoreWriteGatherPipe) empties the
    // line begun, the padding the SDK writes to push the last one out.
    constexpr uint32_t GatherAddress = 0x0C008000;
    void GatherReset();
}
