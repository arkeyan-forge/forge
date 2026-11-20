#include "netplay/clock.h"
#include "netplay/scheduler.h"
#include "netplay/events.h"
#include "netplay/inputs.h"
#include "netplay/state_hash.h"
#ifdef ARKEYAN_NETPLAY_TRACE
#include "netplay/trace.h"
#endif
#include "netplay/session.h"
#include "kernel/video.h"
#include "vfs.h"
#include <image.h>
#include <random>
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
        [[noreturn]] void SessionRefuse(const char* message)
        {
            fprintf(stderr, "[netplay] session refused: %s\n", message);
            fflush(stderr);
            std::_Exit(1);
        }

        struct NetworkOptions
        {
            bool enabled = false, host = false, crossPlatform = false;
            uint16_t port = 0;
            uint32_t delay = 0;
            std::string address, password, dump;

            NetworkOptions()
            {
                const char* hosting = skyenv::Get("SKY_NETPLAY_HOST");
                const char* joining = skyenv::Get("SKY_NETPLAY_JOIN");
                if (!hosting && !joining)
                    return;
                enabled = true;
                if (hosting && joining)
                    SessionRefuse("HOST and JOIN are mutually exclusive");
                const auto is = [](const char* name, std::string_view expected) {
                    const char* value = skyenv::Get(name);
                    return value && std::string_view(value) == expected;
                };
                if (!is("SKY_NETPLAY_DETERMINISTIC", "1") || !is("SKY_NETPLAY_DELIVERY", "frame") ||
                    !is("SKY_NETPLAY_SCHEDULE", "cooperative"))
                    SessionRefuse("HOST/JOIN require deterministic=1, delivery=frame and schedule=cooperative");
                if (skyenv::Get("SKY_NETPLAY_RECORD") || skyenv::Get("SKY_NETPLAY_REPLAY"))
                    SessionRefuse("HOST/JOIN cannot be used with record or replay");
                host = hosting != nullptr;
                uint64_t number = 0;
                if (host)
                {
                    if (!ParseCount(hosting, 1, 65535, false, number))
                        SessionRefuse("HOST must be a UDP port from 1 to 65535");
                    port = uint16_t(number);
                }
                else if (!ParseEndpoint(joining, address, port))
                    SessionRefuse("JOIN must be host:port or [IPv6]:port");
                if (const char* value = skyenv::Get("SKY_NETPLAY_DELAY"))
                {
                    if (!ParseCount(value, 1, 8, false, number))
                        SessionRefuse("DELAY must be from 1 to 8 frames");
                    delay = uint32_t(number);
                }
                if (const char* value = skyenv::Get("SKY_NETPLAY_PASSWORD"))
                    password = value;
                if (const char* value = skyenv::Get("SKY_NETPLAY_DESYNC_DUMP"))
                    dump = value;
                crossPlatform = is("SKY_NETPLAY_CROSS_PLATFORM", "1");
            }
        };
        const NetworkOptions g_network;
        std::mutex g_sessionExitMutex;

        [[noreturn]] void ExitSession(const SessionFailure& failure)
        {
            if (failure.reason == EndReason::Desync)
                fprintf(stderr, "[netplay] session ended: desync frame %llu local=%016llx remote=%016llx\n",
                    static_cast<unsigned long long>(failure.frame), static_cast<unsigned long long>(failure.localHash),
                    static_cast<unsigned long long>(failure.remoteHash));
            else
                fprintf(stderr, "[netplay] session %s: %s\n",
                    failure.reason >= EndReason::Protocol && failure.reason <= EndReason::Seats ? "refused" : "ended",
                    ReasonText(failure.reason));
            fflush(stderr);
            std::_Exit(1);
        }

        const char* PlatformName()
        {
#if defined(_WIN32)
            constexpr const char* Os = "windows";
#elif defined(__APPLE__)
            constexpr const char* Os = "macos";
#else
            constexpr const char* Os = "linux";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
            constexpr const char* Cpu = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
            constexpr const char* Cpu = "x64";
#else
            constexpr const char* Cpu = "other";
#endif
            static const std::string name = std::string(Os) + "/" + Cpu;
            return name.c_str();
        }

        struct NetworkWait
        {
            NetworkWait() { GuestScheduler().NetworkWait(true); }
            ~NetworkWait() { GuestScheduler().NetworkWait(false); }
        };

        struct Probe;
        std::atomic<Probe*> g_probe{ nullptr };
        std::atomic<uint32_t> g_bundle{ 0 };
        std::mutex g_inputMutex;
        InputLatch g_latch;
#ifdef ARKEYAN_NETPLAY_TRACE
        std::atomic<uint32_t> g_frame{ 0 };
#endif

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
            std::unique_ptr<Session> session;
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
                const char* hashPath = skyenv::Get("SKY_NETPLAY_HASH");
                if (hashPath || g_network.enabled)
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
                    blocks.reserve(size_t((1ull << 32) / std::min(blockBytes, uint64_t(0x1000))));
                    if (hashPath)
                        totals = Open(hashPath, "w");
                    const char* blockMode = skyenv::Get("SKY_NETPLAY_HASH_BLOCKS");
                    if (hashPath && blockMode && std::string_view(blockMode) == "1")
                    {
                        sidecar = Open((std::string(hashPath) + ".blocks").c_str(), "wb");
                        Write(sidecar, "SKYBLK01", 8);
                        WriteLE(sidecar, 1, 4);
                    }
                }
                if (const char* path = skyenv::Get("SKY_NETPLAY_EVENTS"))
                {
                    events.emplace(Open(path, "w"));
                    g_events.store(&*events, std::memory_order_release);
                }
                if (g_network.enabled)
                {
                    const auto xex = g_disc.ReadAll("default.xex");
                    const auto image = Image::ParseImage(xex.data(), xex.size());
                    if (!image.data)
                        SessionRefuse("cannot read executable image for SHA-256");
                    SessionConfig config;
                    config.hello = MakeHello(g_gameInfo.id, Sha256Hex({ image.data.get(), size_t(image.size) }),
                        ARKEYAN_VERSION, PlatformName(), FrameRateLimit(), g_network.crossPlatform, g_network.host);
                    std::random_device random;
                    for (auto& byte : config.hello.nonce)
                        byte = uint8_t(random());
                    config.password = g_network.password;
                    config.delay = g_network.delay;
                    config.ended = [](const SessionFailure& failure) {
                        // A desync is handled at its hash barrier, where the guest's block dump is still available.
                        if (failure.reason != EndReason::Desync)
                        {
                            std::lock_guard lock(g_sessionExitMutex);
                            ExitSession(failure);
                        }
                    };
                    std::string error;
                    auto transport = OpenUdp(g_network.address, g_network.port, g_network.host, error);
                    if (!transport)
                        SessionRefuse(error.c_str());
                    session = std::make_unique<Session>(std::move(config), std::move(transport));
                }
                g_probe.store(this, std::memory_order_release);
            }

            ~Probe() { Finish(); }

            void Finish()
            {
                if (finished)
                    return;
                finished = true;
                if (session)
                    session->Stop();
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
                if (session)
                {
                    NetworkWait waiting;
                    if (!session->WaitReady())
                        SessionExit();
                    uint32_t ignored = 0;
                    const Pad local{ GetGamepad(ignored, 0), GamepadConnected(0) };
                    if (!session->Exchange(frames, local, frame))
                        SessionExit();
                }
                else if (replaying)
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

            void Scan(const GuestHeap& heap, Heap kind, uint32_t begin)
            {
                // FindAllocation avoids the allocating map copy made by ForEachAllocation while the guest is held.
                for (uint64_t at = begin; at <= UINT32_MAX && heap.Contains(uint32_t(at));)
                {
                    uint32_t size = 0;
                    const uint32_t base = heap.FindAllocation(uint32_t(at), &size);
                    if (!base)
                    {
                        at += heap.PageSize();
                        continue;
                    }
                    const uint32_t address = kind == Heap::Physical ? PhysicalToGuest(base) : base;
                    if (address)
                        Allocation(address, size, kind);
                    at = uint64_t(base) + size;
                }
            }

            void WriteBlocks(FILE* file)
            {
                WriteLE(file, frames, 8);
                WriteLE(file, blocks.size(), 4);
                for (const auto& block : blocks)
                {
                    WriteLE(file, block.address, 4);
                    WriteLE(file, uint32_t(block.heap), 4);
                    WriteLE(file, block.hash, 8);
                }
            }

            [[noreturn]] void SessionExit()
            {
                std::lock_guard lock(g_sessionExitMutex);
                const auto failure = session->Failure();
                if (failure.reason == EndReason::Desync && !g_network.dump.empty())
                {
                    FILE* file = Open(g_network.dump.c_str(), "wb");
                    Write(file, "SKYBLK01", 8);
                    WriteLE(file, 1, 4);
                    WriteBlocks(file);
                    if (fclose(file) != 0)
                        Refuse("desync dump flush failed");
                }
                ExitSession(failure);
            }

            uint64_t TotalHash()
            {
                blocks.clear();
                Scan(g_virtualHeap4K, Heap::Virtual4K, 0x00010000);
                Scan(g_virtualHeap64K, Heap::Virtual64K, 0x40000000);
                Scan(g_physicalHeap, Heap::Physical, 0x00010000);
                for (const auto range : imageRanges)
                    Allocation(uint32_t(range.begin), uint32_t(range.end - range.begin), Heap::Image);
                uint64_t total = HashSeed;
                for (const auto& block : blocks)
                {
                    total = Mix(total, (uint64_t(block.address) << 32) | uint32_t(block.heap));
                    total = Mix(total, block.hash);
                }
                return total;
            }

            void Hash()
            {
                const bool networkHash = session && frames % 30 == 0;
                const bool fileHash = totals && frames % hashEvery == 0;
                if (!networkHash && !fileHash)
                    return;
                const auto start = std::chrono::steady_clock::now();
                const uint64_t total = TotalHash();
                if (fileHash)
                {
                    if (fprintf(totals, "%llu %016llx\n", static_cast<unsigned long long>(frames),
                            static_cast<unsigned long long>(total)) < 0)
                        Refuse("hash totals write failed");
                    if (sidecar)
                        WriteBlocks(sidecar);
                    if (fflush(totals) != 0 || (sidecar && fflush(sidecar) != 0))
                        Refuse("hash output flush failed");
                }
                if (networkHash)
                {
                    NetworkWait waiting;
                    if (!session->CheckHash(frames, total))
                        SessionExit();
                }
                const double ms =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                totalMs += ms;
                maxMs = std::max(maxMs, ms);
                ++hashed;
            }
        };
    }

#ifdef ARKEYAN_NETPLAY_TRACE
    uint32_t ProbeFrame() { return g_frame.load(std::memory_order_relaxed); }
#endif

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
#ifdef ARKEYAN_NETPLAY_TRACE
        if (g_trace)
            g_trace->EndFrame(ProbeFrame());
#endif
        g_clock.Advance(presentInterval);
        WriteBundle();
        probe.Latch();
        ++probe.frames;
#ifdef ARKEYAN_NETPLAY_TRACE
        g_frame.store(uint32_t(probe.frames), std::memory_order_relaxed);
#endif
        probe.Hash();
    }

    void Finish()
    {
        if (Probe* probe = g_probe.load(std::memory_order_acquire))
        {
            std::lock_guard lock(probe->mutex);
            probe->Finish();
#ifdef ARKEYAN_NETPLAY_TRACE
            FinishTrace(ProbeFrame());
#endif
        }
#ifdef ARKEYAN_NETPLAY_TRACE
        else
            FinishTrace(ProbeFrame());
#endif
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
