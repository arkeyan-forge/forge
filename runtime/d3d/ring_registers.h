#pragma once

#include "d3d/ring.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace d3d
{
    // The GPU registers as the ring scan last wrote them (d3d/ring.cpp). A register's fields sit together: the scan
    // writes every register of every draw's packets, and separate arrays cost a cache line each per write.
    struct RingRegisters
    {
        static constexpr uint32_t First = 0x2000;
        static constexpr uint32_t Count = 0x4928 - First;
        static constexpr uint32_t AluConstants = 0x4000; // vertex shader c0.x .. then pixel shader from 0x4400
        static constexpr uint32_t FetchConstants = 0x4800;

        struct Register
        {
            uint32_t value;
            uint32_t epoch; // `epoch` when written
            bool written;
            // Written from an indirect buffer (a shader's own packet) or loaded from memory (LOAD_ALU_CONSTANT): a
            // shader's literal constants, in effect while the shader is used even if the device's shadow of that group
            // is pending.
            bool shaderOwned;
        };
        std::array<Register, Count> registers{};
        uint32_t epoch = 1;                  // advanced after each draw (DrawConsumed)
        std::vector<RegisterWrite> recorded; // writes into the command buffer being recorded
        // Per group of 16 float constant registers: GpuFloatConstants' copy holds the stream's values of a clean group
        // whose registers were all written, and no write came since. As the device's dirty masks: group g is bit
        // 63 - g % 64 of word g / 64.
        uint64_t constantsCached[2] = {};

        void Write(uint32_t reg, uint32_t value, bool recording, uint32_t depth = 0)
        {
            WriteRun(reg, 1, [value](uint32_t) { return value; }, recording, depth);
        }

        // Registers reg .. reg + count - 1 take read(0) .. read(count - 1); those outside the file are dropped.
        template <typename Read>
        void WriteRun(uint32_t reg, uint32_t count, Read&& read, bool recording, uint32_t depth = 0)
        {
            const uint32_t from = std::max(reg, First);
            const uint32_t to = uint32_t(std::min(uint64_t(reg) + count, uint64_t(First + Count)));
            if (from >= to)
                return;
            if (recording)
            {
                for (uint32_t r = from; r < to; r++)
                    recorded.push_back({ r, read(r - reg) });
                return;
            }
            const bool owned = depth != 0;
            for (uint32_t r = from; r < to; r++)
                registers[r - First] = { read(r - reg), epoch, true, owned };
            const uint32_t constantsFrom = std::max(from, AluConstants), constantsTo = std::min(to, FetchConstants);
            if (constantsFrom < constantsTo)
                for (uint32_t group = (constantsFrom - AluConstants) / 16;
                    group <= (constantsTo - 1 - AluConstants) / 16; group++)
                    constantsCached[group / 64] &= ~(1ull << (63 - group % 64));
        }
    };
}
