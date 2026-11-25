#include "wii/gx_fifo.h"
#include "wii/memory_map.h"
#include <cstring>

namespace wii::gx
{
    namespace
    {
        uint32_t Get32(const uint8_t* p)
        {
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }
        uint16_t Get16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
        void Put16(uint8_t* p, uint32_t value)
        {
            p[0] = uint8_t(value >> 8);
            p[1] = uint8_t(value);
        }
        void Put32(uint8_t* p, uint32_t value)
        {
            Put16(p, value >> 16);
            Put16(p + 2, value);
        }

        // The CPU's and the GP's FIFOs take physical addresses; the host reads them at MEM1's or MEM2's cached one.
        uint32_t Cached(uint32_t physical) { return 0x80000000u | (physical & 0x1FFFFFFFu); }

        // A direct attribute's component, by its format: u8, s8, u16, s16, f32.
        constexpr uint32_t ComponentBytes[8] = { 1, 1, 2, 2, 4, 0, 0, 0 };
        // A direct colour, by its format: RGB565, RGB888, RGB888x, RGBA4444, RGBA6666, RGBA8888.
        constexpr uint32_t ColourBytes[8] = { 2, 3, 4, 2, 3, 4, 0, 0 };

        // An attribute's size when sent as an index (2: 8 bits, 3: 16), 0 when not sent (0), `direct` when direct (1).
        uint32_t AttributeBytes(uint32_t type, uint32_t direct)
        {
            return type == 1 ? direct : type == 2 ? 1 : type == 3 ? 2 : 0;
        }
    } // namespace

    uint32_t CommandProcessor::VertexSize(uint8_t format) const
    {
        const uint32_t low = cpRegisters[0x50], high = cpRegisters[0x60];
        const uint32_t a = cpRegisters[0x70 + (format & 7)], b = cpRegisters[0x80 + (format & 7)],
                       c = cpRegisters[0x90 + (format & 7)];
        uint32_t size = 0;
        for (uint32_t bit = 0; bit < 9; bit++) // the position's and the 8 texture coordinates' matrix indices
            size += (low >> bit) & 1;

        const uint32_t positionComponents = (a & 1) ? 3 : 2;
        size += AttributeBytes((low >> 9) & 3, positionComponents * ComponentBytes[(a >> 1) & 7]);

        const uint32_t normalType = (low >> 11) & 3;
        const bool nbt = ((a >> 9) & 1) != 0, threeIndices = ((a >> 31) & 1) != 0;
        if (normalType == 1)
            size += (nbt ? 9 : 3) * ComponentBytes[(a >> 10) & 7];
        else if (normalType >= 2)
            size += AttributeBytes(normalType, 0) * (nbt && threeIndices ? 3 : 1);

        size += AttributeBytes((low >> 13) & 3, ColourBytes[(a >> 14) & 7]);
        size += AttributeBytes((low >> 15) & 3, ColourBytes[(a >> 18) & 7]);

        // Each texture coordinate's count (s, or s and t) and format [VAT A for 0, B for 1-4, C for 4's shift on].
        const uint32_t texture[8][2] = { { (a >> 21) & 1, (a >> 22) & 7 }, { b & 1, (b >> 1) & 7 },
            { (b >> 9) & 1, (b >> 10) & 7 }, { (b >> 18) & 1, (b >> 19) & 7 }, { (b >> 27) & 1, (b >> 28) & 7 },
            { (c >> 5) & 1, (c >> 6) & 7 }, { (c >> 14) & 1, (c >> 15) & 7 }, { (c >> 23) & 1, (c >> 24) & 7 } };
        for (uint32_t i = 0; i < 8; i++)
            size += AttributeBytes((high >> (2 * i)) & 3, (texture[i][0] ? 2 : 1) * ComponentBytes[texture[i][1]]);
        return size;
    }

    size_t CommandProcessor::CommandSize(const uint8_t* bytes, size_t size) const
    {
        if (size == 0)
            return 0;
        const uint8_t opcode = bytes[0];
        auto need = [size](size_t length) -> size_t { return size >= length ? length : 0; };
        switch (opcode)
        {
        case Nop:
        case InvalidateVertexCache: return 1;
        case LoadCp: return need(6);
        case LoadXf: return size < 5 ? 0 : need(5 + 4 * (size_t(Get16(bytes + 1)) + 1));
        case LoadIndexA:
        case LoadIndexB:
        case LoadIndexC:
        case LoadIndexD:
        case LoadBp: return need(5);
        case CallList: return need(9);
        default:
            if (opcode >= Draw && opcode < 0xC0)
                return size < 3 ? 0 : need(3 + size_t(Get16(bytes + 1)) * VertexSize(opcode & 7));
            return ~size_t(0);
        }
    }

    void CommandProcessor::LoadIndexed(const uint8_t* base, uint8_t array, uint32_t word)
    {
        const uint32_t index = word >> 16, words = ((word >> 12) & 0xF) + 1, address = word & 0xFFF;
        const uint32_t source = Cached(cpRegisters[0xA0 + array] + index * cpRegisters[0xB0 + array]);
        if (!InMemory(source, words * 4))
            return;
        for (uint32_t i = 0; i < words && address + i < sizeof(xfMemory) / 4; i++)
            xfMemory[address + i] = Get32(base + source + 4 * i);
    }

    void CommandProcessor::Run(const uint8_t* base, const uint8_t* command, int depth)
    {
        stats.commands++;
        const uint8_t opcode = command[0];
        switch (opcode)
        {
        case LoadCp: cpRegisters[command[1]] = Get32(command + 2); break;
        case LoadXf:
        {
            const uint32_t count = uint32_t(Get16(command + 1)) + 1, address = Get16(command + 3);
            for (uint32_t i = 0; i < count && address + i < sizeof(xfMemory) / 4; i++)
                xfMemory[address + i] = Get32(command + 5 + 4 * i);
            break;
        }
        case LoadIndexA:
        case LoadIndexB:
        case LoadIndexC:
        case LoadIndexD: LoadIndexed(base, uint8_t(12 + (opcode - LoadIndexA) / 8), Get32(command + 1)); break;
        case CallList:
            // The GP runs a list from the FIFO only; a list calling one is not run (as the hardware does not).
            if (depth == 0)
                RunList(base, Get32(command + 1), Get32(command + 5));
            break;
        case LoadBp:
        {
            const uint8_t index = command[1];
            const uint32_t value = Get32(command + 1) & 0xFFFFFF;
            if (index == 0xFE) // the mask of the next write
            {
                bpMask = value;
                break;
            }
            bpRegisters[index] = (bpRegisters[index] & ~bpMask) | (value & bpMask);
            bpMask = 0xFFFFFF;
            if (index == 0x52)
            {
                stats.copies++;
                if (onCopy)
                    onCopy(bpRegisters[index]);
            }
            if (onBp)
                onBp(index, bpRegisters[index]);
            break;
        }
        default:
            if (opcode >= Draw && opcode < 0xC0)
            {
                const DrawCall draw{ Primitive((opcode >> 3) & 7), uint8_t(opcode & 7), Get16(command + 1), command + 3,
                    VertexSize(opcode & 7) };
                stats.draws++;
                stats.vertices += draw.count;
                if (onDraw)
                    onDraw(draw);
            }
            break;
        }
    }

    void CommandProcessor::RunList(const uint8_t* base, uint32_t address, uint32_t size)
    {
        stats.lists++;
        const uint32_t start = Cached(address);
        if (size == 0 || !InMemory(start, size))
        {
            stats.badLists++;
            return;
        }
        const uint8_t* bytes = base + start;
        for (size_t at = 0; at < size;)
        {
            const size_t length = CommandSize(bytes + at, size - at);
            if (length == 0 || length == ~size_t(0))
            {
                stats.badLists++;
                return;
            }
            Run(base, bytes + at, 1);
            at += length;
        }
    }

    void CommandProcessor::Feed(const uint8_t* base, const uint8_t* bytes, size_t size)
    {
        if (skipping)
            return;
        pending.insert(pending.end(), bytes, bytes + size);
        size_t at = 0;
        while (at < pending.size())
        {
            const size_t length = CommandSize(pending.data() + at, pending.size() - at);
            if (length == 0)
                break;
            if (length == ~size_t(0))
            {
                stats.unknown++;
                skipping = true;
                pending.clear();
                return;
            }
            Run(base, pending.data() + at, 0);
            at += length;
        }
        pending.erase(pending.begin(), pending.begin() + ptrdiff_t(at));
    }

    CommandProcessor& Processor()
    {
        static CommandProcessor& processor = *new CommandProcessor; // used by guest threads until the process ends
        return processor;
    }

    namespace
    {
        uint8_t g_line[GatherLine];
        uint32_t g_filled = 0;

        // A full line to the CPU's FIFO at the write pointer, which then moves on (to the base past the end, with
        // the wrap bit set); a FIFO the GP reads as it is written has the line run at once, nothing left to read.
        void Burst(uint8_t* base)
        {
            uint8_t* registers = base;
            const uint32_t pointer = Get32(registers + pi::FifoWrite);
            uint32_t write = pointer & pi::WriteMask;
            const uint32_t destination = Cached(write);
            if (InMemory(destination, GatherLine))
                memcpy(base + destination, g_line, GatherLine);
            // The pointer wraps on reaching the end's line only: a redirected pipe (GXRedirectWriteGatherPipe) writes
            // a buffer past an end it sets to 64 MB, in MEM2, and must go on there.
            const bool atEnd = write == (Get32(registers + pi::FifoEnd) & pi::WriteMask);
            write += GatherLine;
            uint32_t wrap = pointer & pi::Wrapped;
            if (atEnd)
            {
                write = Get32(registers + pi::FifoBase) & pi::WriteMask;
                wrap = pi::Wrapped;
            }
            Put32(registers + pi::FifoWrite, write | wrap);
            Processor().CountLine();

            CommandProcessor& processor = Processor();
            const uint16_t control = Get16(registers + cp::Control);
            if ((control & (cp::ReadEnable | cp::Linked)) == (cp::ReadEnable | cp::Linked))
            {
                processor.Feed(base, g_line, GatherLine);
                for (uint32_t pointerRegister : { cp::FifoWrite, cp::FifoRead })
                {
                    Put16(registers + pointerRegister, write & 0xFFFF);
                    Put16(registers + pointerRegister + 2, write >> 16);
                }
                Put16(registers + cp::Distance, 0);
                Put16(registers + cp::Distance + 2, 0);
            }
        }
    } // namespace

    void GatherWrite(uint8_t* base, uint64_t value, uint32_t bytes)
    {
        for (uint32_t i = bytes; i-- > 0;)
        {
            g_line[g_filled++] = uint8_t(value >> (8 * i));
            if (g_filled == GatherLine)
            {
                g_filled = 0;
                Burst(base);
            }
        }
    }

    void GatherReset() { g_filled = 0; }
}
