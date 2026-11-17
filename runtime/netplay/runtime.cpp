#include "netplay/clock.h"
#include "netplay/scheduler.h"
#include "netplay/events.h"
#include "netplay/inputs.h"
#include "netplay/state_hash.h"
#include "host/env.h"
#include "host/launcher.h"
#include "memory.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <ppc_config.h>

extern const GameInfo g_gameInfo;

namespace netplay
{
    namespace
    {
        struct Probe;
        std::atomic<Probe*> g_probe{ nullptr };
        std::atomic<uint32_t> g_bundle{ 0 };
        std::mutex g_inputMutex;
        InputLatch g_latch;

        [[noreturn]] void Refuse(const char* message)
        {
            // Not exit(): the guest's threads are running (as at the runtime's other exits).
            fprintf(stderr, "[netplay] refused: %s\n", message);
            fflush(stderr);
            std::_Exit(1);
        }

        FILE* Open(const char* path, const char* mode)
        {
            FILE* file = fopen(path, mode);
            if (!file)
                Refuse("cannot open probe file");
            return file;
        }

        void Write(FILE* file, const void* data, size_t size)
        {
            if (fwrite(data, 1, size, file) != size)
                Refuse("probe file write failed");
        }

        void WriteLE(FILE* file, uint64_t value, size_t size)
        {
            uint8_t bytes[8];
            for (size_t i = 0; i < size; ++i)
                bytes[i] = uint8_t(value >> (8 * i));
            Write(file, bytes, size);
        }

        void Seek(FILE* file, long offset, int origin)
        {
            if (fseek(file, offset, origin) != 0)
                Refuse("probe file seek failed");
        }

        void WriteBundle()
        {
            if (const uint32_t bundle = g_bundle.load(std::memory_order_acquire))
            {
                const uint64_t elapsed = g_clock.Elapsed();
                g_memory.Write<uint64_t>(bundle, elapsed);
                g_memory.Write<uint64_t>(bundle + 8, EpochFileTime + elapsed);
                g_memory.Write<uint32_t>(bundle + 16, uint32_t(elapsed / 10000));
            }
        }

        struct Probe
        {
            std::mutex mutex;
            Replay replay;
            bool replaying = false, ended = false, finished = false;
            FILE* record = nullptr;
            FILE* totals = nullptr;
            FILE* sidecar = nullptr;
            std::optional<EventLog> events;
            uint64_t frames = 0, hashed = 0, hashEvery = 1, blockBytes = 0x10000;
            double totalMs = 0, maxMs = 0;
            std::vector<Range> skips, imageRanges;
            std::vector<Block> blocks;

            Probe()
            {
                if (const char* path = skyenv::Get("SKY_NETPLAY_REPLAY"))
                {
                    FILE* file = Open(path, "rb");
                    Seek(file, 0, SEEK_END);
                    const long size = ftell(file);
                    if (size < 0 || uint64_t(size) > MaxReplayBytes)
                        Refuse("oversize or unreadable replay");
                    Seek(file, 0, SEEK_SET);
                    std::vector<uint8_t> bytes(static_cast<size_t>(size));
                    const size_t got = fread(bytes.data(), 1, bytes.size(), file);
                    fclose(file);
                    std::string error;
                    if (got != bytes.size())
                        Refuse("truncated replay read");
                    if (!ParseReplay(bytes, replay, error))
                        Refuse(error.c_str());
                    if (replay.gameId != g_gameInfo.id)
                        Refuse("replay game id does not match this runtime");
                    replaying = true;
                }
                if (const char* path = skyenv::Get("SKY_NETPLAY_RECORD"))
                {
                    if (replaying)
                        Refuse("record and replay are mutually exclusive");
                    record = Open(path, "wb");
                    const auto header = InputHeader(g_gameInfo.id, 0);
                    Write(record, header.data(), header.size());
                }
                if (const char* path = skyenv::Get("SKY_NETPLAY_HASH"))
                {
                    std::string error;
                    const char* skip = skyenv::Get("SKY_NETPLAY_HASH_SKIP");
                    if (!ParseSkips(skip ? skip : "", skips, error))
                        Refuse(error.c_str());
                    std::vector<uint8_t> headers(std::min(size_t(PPC_IMAGE_SIZE), size_t(1 << 20)));
                    for (size_t i = 0; i < headers.size(); ++i)
                        headers[i] = g_memory.Read<uint8_t>(PPC_IMAGE_BASE + uint32_t(i));
                    if (!WritableSections(headers, PPC_IMAGE_BASE, imageRanges, error, PPC_IMAGE_SIZE))
                        Refuse(error.c_str());
                    if (const char* every = skyenv::Get("SKY_NETPLAY_HASH_EVERY"))
                        if (!ParseCount(every, 1, 100000, false, hashEvery))
                            Refuse("SKY_NETPLAY_HASH_EVERY is not a count from 1 to 100000");
                    if (const char* size = skyenv::Get("SKY_NETPLAY_HASH_BLOCK"))
                        if (!ParseCount(size, 256, 0x10000, true, blockBytes))
                            Refuse("SKY_NETPLAY_HASH_BLOCK is not a power of two from 256 to 65536");
                    // The guest address space bounds the number of page fragments, even with one allocation per page.
                    blocks.reserve(1 << 20);
                    totals = Open(path, "w");
                    const char* blockMode = skyenv::Get("SKY_NETPLAY_HASH_BLOCKS");
                    if (blockMode && std::string_view(blockMode) == "1")
                    {
                        sidecar = Open((std::string(path) + ".blocks").c_str(), "wb");
                        Write(sidecar, "SKYBLK01", 8);
                        WriteLE(sidecar, 1, 4);
                    }
                }
                if (const char* path = skyenv::Get("SKY_NETPLAY_EVENTS"))
                {
                    events.emplace(Open(path, "w"));
                    g_events.store(&*events, std::memory_order_release);
                }
                g_probe.store(this, std::memory_order_release);
            }

            ~Probe() { Finish(); }

            void Finish()
            {
                if (finished)
                    return;
                finished = true;
                if (record)
                    fclose(record);
                if (totals)
                    fclose(totals);
                if (sidecar)
                    fclose(sidecar);
                if (events)
                {
                    g_events.store(nullptr, std::memory_order_release);
                    events->Close();
                }
                if (hashed)
                    fprintf(stderr, "[netplay] hashes: %llu frames, mean %.3f ms, max %.3f ms\n",
                        static_cast<unsigned long long>(hashed), totalMs / double(hashed), maxMs);
            }

            void Latch()
            {
                InputFrame frame{};
                if (replaying)
                {
                    frame = ReplayFrame(replay, frames);
                    if (frames >= replay.frames.size() && !ended)
                    {
                        ended = true;
                        fprintf(stderr, "[netplay] replay ended; neutral pads, recorded connections retained\n");
                    }
                }
                else
                    for (int user = 0; user < MaxUsers; ++user)
                    {
                        uint32_t ignored = 0;
                        frame[size_t(user)].state = GetGamepad(ignored, user);
                        frame[size_t(user)].connected = GamepadConnected(user);
                    }
                {
                    std::lock_guard lock(g_inputMutex);
                    g_latch.Latch(frame);
                }
                if (record)
                {
                    if (frames >= MaxReplayFrames)
                        Refuse("recording exceeds frame limit");
                    const auto bytes = EncodeFrame(frame);
                    Write(record, bytes.data(), bytes.size());
                    Seek(record, 16, SEEK_SET);
                    WriteLE(record, frames + 1, 8);
                    Seek(record, 0, SEEK_END);
                    if (fflush(record) != 0)
                        Refuse("recording flush failed");
                }
            }

            void Allocation(uint32_t address, uint32_t size, Heap heap)
            {
                const auto* bytes = static_cast<const uint8_t*>(g_memory.Translate(address));
                HashAllocation(std::span(bytes, size), address, heap, skips, blocks, uint32_t(blockBytes));
            }

            void Scan(const GuestHeap& heap, Heap kind)
            {
                // A debug probe: the copy ForEachAllocation makes costs less than a lookup per free page.
                heap.ForEachAllocation([&](uint32_t base, uint32_t size) {
                    const uint32_t address = kind == Heap::Physical ? PhysicalToGuest(base) : base;
                    if (address)
                        Allocation(address, size, kind);
                });
            }

            void Hash()
            {
                if (!totals || frames % hashEvery != 0)
                    return;
                const auto start = std::chrono::steady_clock::now();
                blocks.clear();
                Scan(g_virtualHeap4K, Heap::Virtual4K);
                Scan(g_virtualHeap64K, Heap::Virtual64K);
                Scan(g_physicalHeap, Heap::Physical);
                for (const auto range : imageRanges)
                    Allocation(uint32_t(range.begin), uint32_t(range.end - range.begin), Heap::Image);
                uint64_t total = HashSeed;
                for (const auto& block : blocks)
                {
                    total = Mix(total, (uint64_t(block.address) << 32) | uint32_t(block.heap));
                    total = Mix(total, block.hash);
                }
                if (fprintf(totals, "%llu %016llx\n", static_cast<unsigned long long>(frames),
                        static_cast<unsigned long long>(total)) < 0)
                    Refuse("hash totals write failed");
                if (sidecar)
                {
                    WriteLE(sidecar, frames, 8);
                    WriteLE(sidecar, blocks.size(), 4);
                    for (const auto& block : blocks)
                    {
                        WriteLE(sidecar, block.address, 4);
                        WriteLE(sidecar, uint32_t(block.heap), 4);
                        WriteLE(sidecar, block.hash, 8);
                    }
                }
                if (fflush(totals) != 0 || (sidecar && fflush(sidecar) != 0))
                    Refuse("hash output flush failed");
                const double ms =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                totalMs += ms;
                maxMs = std::max(maxMs, ms);
                ++hashed;
            }
        };
    }

    void SetTimeStampBundle(uint32_t address)
    {
        g_bundle.store(address, std::memory_order_release);
        if (Cooperative())
            g_clock.SetAdvanceHook(WriteBundle);
        WriteBundle();
    }

    void OnFrame(uint32_t presentInterval)
    {
        static Probe probe;
        std::lock_guard lock(probe.mutex);
        if (probe.finished)
            return;
        if (probe.events && !probe.events->EndFrame(probe.frames))
            Refuse("event log write failed");
        g_clock.Advance(presentInterval);
        WriteBundle();
        probe.Latch();
        ++probe.frames;
        probe.Hash();
    }

    void Finish()
    {
        if (Probe* probe = g_probe.load(std::memory_order_acquire))
        {
            std::lock_guard lock(probe->mutex);
            probe->Finish();
        }
    }

    bool LatchedConnected(int user)
    {
        std::lock_guard lock(g_inputMutex);
        uint32_t ignored;
        return g_latch.Get(user, ignored).connected;
    }

    bool ReadPad(int user, GamepadState& state, uint32_t& packet)
    {
        if (Enabled())
        {
            std::lock_guard lock(g_inputMutex);
            const Pad pad = g_latch.Get(user, packet);
            state = pad.state;
            return pad.connected;
        }
        if (!GamepadConnected(user))
            return false;
        state = GetGamepad(packet, user);
        return true;
    }
}
