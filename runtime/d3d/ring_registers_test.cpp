// Checks the ring scan's register file (d3d/ring_registers.h): a run of writes leaves the same registers, cached
// constant groups and recorded writes as the same writes one register at a time, across the file's edges, the float
// constant groups, recording and shader-owned writes. usage: ArkeyanRingRegistersTest
#include "d3d/ring_registers.h"
#include <cstdio>
#include <memory>
#include <random>

namespace
{
    using d3d::RingRegisters;

    // One register at a time, as the scan wrote before runs.
    struct Reference
    {
        std::array<uint32_t, RingRegisters::Count> values{}, epochOf{};
        std::array<bool, RingRegisters::Count> written{}, shaderOwned{};
        uint32_t epoch = 1;
        uint64_t constantsCached[2] = {};
        std::vector<d3d::RegisterWrite> recorded;

        void Write(uint32_t reg, uint32_t value, bool recording, uint32_t depth)
        {
            if (reg < RingRegisters::First || reg >= RingRegisters::First + RingRegisters::Count)
                return;
            if (recording)
            {
                recorded.push_back({ reg, value });
                return;
            }
            const uint32_t index = reg - RingRegisters::First;
            values[index] = value;
            written[index] = true;
            epochOf[index] = epoch;
            shaderOwned[index] = depth != 0;
            if (reg >= RingRegisters::AluConstants && reg < RingRegisters::FetchConstants)
            {
                const uint32_t group = (reg - RingRegisters::AluConstants) / 16;
                constantsCached[group / 64] &= ~(1ull << (63 - group % 64));
            }
        }
    };

    bool Same(const RingRegisters& file, const Reference& reference)
    {
        for (uint32_t i = 0; i < RingRegisters::Count; i++)
        {
            const RingRegisters::Register& r = file.registers[i];
            if (r.written != reference.written[i] ||
                (r.written && (r.value != reference.values[i] || r.epoch != reference.epochOf[i] ||
                                  r.shaderOwned != reference.shaderOwned[i])))
                return false;
        }
        if (file.recorded.size() != reference.recorded.size())
            return false;
        for (size_t i = 0; i < file.recorded.size(); i++)
            if (file.recorded[i].reg != reference.recorded[i].reg ||
                file.recorded[i].value != reference.recorded[i].value)
                return false;
        return file.constantsCached[0] == reference.constantsCached[0] &&
               file.constantsCached[1] == reference.constantsCached[1];
    }
}

int main()
{
    auto file = std::make_unique<RingRegisters>();
    auto reference = std::make_unique<Reference>();
    std::mt19937 random(42);
    // Starts: below the file, its first register, the float constants' start and end, the fetch constants, its end.
    const uint32_t starts[] = { 0x1FF0, RingRegisters::First, RingRegisters::AluConstants - 3,
        RingRegisters::AluConstants + 13, RingRegisters::FetchConstants - 20, RingRegisters::FetchConstants,
        RingRegisters::First + RingRegisters::Count - 5, 0xFFFFFFF0 };
    int failures = 0;
    for (uint32_t round = 0; round < 20000; round++)
    {
        if (round % 64 == 0)
        {
            // GpuFloatConstants caches groups; the next writes must clear their bits again.
            file->constantsCached[0] = reference->constantsCached[0] = random() | uint64_t(random()) << 32;
            file->constantsCached[1] = reference->constantsCached[1] = random() | uint64_t(random()) << 32;
        }
        if (round % 7 == 0)
        {
            file->epoch++;
            reference->epoch++;
        }
        const uint32_t reg = round % 3 == 0 ? starts[random() % std::size(starts)] + random() % 8
                                            : RingRegisters::First + random() % RingRegisters::Count;
        const uint32_t count = random() % 4 == 0 ? random() % 600 : random() % 40;
        const bool recording = random() % 10 == 0;
        const uint32_t depth = random() % 3;
        const uint32_t seed = random();
        auto value = [seed](uint32_t i) { return seed * 2654435761u + i; };
        file->WriteRun(reg, count, value, recording, depth);
        for (uint32_t i = 0; i < count && uint64_t(reg) + i <= UINT32_MAX; i++)
            reference->Write(reg + i, value(i), recording, depth);
        if (!Same(*file, *reference))
        {
            fprintf(stderr, "FAIL: round %u, a run of %u from 0x%X (recording %d, depth %u) differs\n", round, count,
                reg, recording, depth);
            failures++;
            break;
        }
    }
    file->Write(RingRegisters::AluConstants + 17, 5, false, 0);
    reference->Write(RingRegisters::AluConstants + 17, 5, false, 0);
    if (!Same(*file, *reference))
    {
        fprintf(stderr, "FAIL: a single write differs\n");
        failures++;
    }
    if (failures == 0)
        printf("ArkeyanRingRegistersTest: all passed\n");
    return failures == 0 ? 0 : 1;
}
