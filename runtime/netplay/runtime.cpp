#include "netplay/clock.h"
#include "netplay/scheduler.h"
#include "netplay/events.h"
#include "netplay/inputs.h"
#include "netplay/net_sim.h"
#include "netplay/state_hash.h"
#ifdef ARKEYAN_NETPLAY_TRACE
#include "netplay/trace.h"
#endif
#include "netplay/session.h"
#include "netplay/session_end.h"
#include "netplay/session_runtime.h"
#include "netplay/session_settings.h"
#include "host/portal.h"
#include "kernel/video.h"
#include "vfs.h"
#include <image.h>
#include <random>
#include "host/env.h"
#include "host/launcher.h"
#include "host/profiles.h"
#include "memory.h"
#include <SDL3/SDL_events.h>
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
        std::atomic<EndReason> g_endReason{ EndReason::None };
        std::atomic<Session*> g_activeSession{ nullptr };
        std::mutex g_sessionExitMutex, g_pendingMutex;
        EventBatch g_pending;
        std::atomic<bool> g_leftLocally{ false };
        std::array<char, 256> g_endMessage{};

        void ExitSession(const SessionFailure& failure);
        void SessionRefuse(const char* message, EndReason reason = EndReason::Invalid)
        {
            fprintf(stderr, "[netplay] session refused: %s\n", message);
            ExitSession({ reason });
        }

        struct NetworkOptions
        {
            bool enabled = false, host = false, crossPlatform = false;
            uint16_t port = 0;
            uint32_t delay = 0;
            std::string address, password, dump, error;
            uint64_t testDesync = 0;
            uint32_t testSlowMs = 0;
            std::optional<NetSim> testNet;
            EndReason errorReason = EndReason::Invalid;

            NetworkOptions()
            {
                const char* hosting = skyenv::Get("SKY_NETPLAY_HOST");
                const char* joining = skyenv::Get("SKY_NETPLAY_JOIN");
                if (!hosting && !joining)
                    return;
                enabled = true;
                if (hosting && joining)
                {
                    error = "HOST and JOIN are mutually exclusive";
                    return;
                }
                const auto is = [](const char* name, std::string_view expected) {
                    const char* value = skyenv::Get(name);
                    return value && std::string_view(value) == expected;
                };
                if (!is("SKY_NETPLAY_DETERMINISTIC", "1") || !is("SKY_NETPLAY_DELIVERY", "frame") ||
                    !is("SKY_NETPLAY_SCHEDULE", "cooperative"))
                {
                    error = "HOST/JOIN require deterministic=1, delivery=frame and schedule=cooperative";
                    return;
                }
                if (skyenv::Get("SKY_NETPLAY_RECORD") || skyenv::Get("SKY_NETPLAY_REPLAY"))
                {
                    error = "HOST/JOIN cannot be used with record or replay";
                    return;
                }
                if (!SessionFrameRate(skyenv::Get("SKY_NETPLAY_FPS")))
                {
                    error = "SKY_NETPLAY_FPS is required and must be 30 or 60";
                    errorReason = EndReason::Fps;
                    return;
                }
                host = hosting != nullptr;
                uint64_t number = 0;
                if (host)
                {
                    if (!ParseCount(hosting, 1, 65535, false, number))
                    {
                        error = "HOST must be a UDP port from 1 to 65535";
                        return;
                    }
                    port = uint16_t(number);
                }
                else if (!ParseEndpoint(joining, address, port))
                {
                    error = "JOIN must be host:port or [IPv6]:port";
                    return;
                }
                if (const char* value = skyenv::Get("SKY_NETPLAY_DELAY"))
                {
                    if (!ParseCount(value, 1, 8, false, number))
                    {
                        error = "DELAY must be from 1 to 8 frames";
                        return;
                    }
                    delay = uint32_t(number);
                }
                if (const char* value = skyenv::Get("SKY_NETPLAY_PASSWORD"))
                    password = value;
                if (const char* value = skyenv::Get("SKY_NETPLAY_DESYNC_DUMP"))
                    dump = value;
                if (const char* value = skyenv::Get("SKY_NETPLAY_TEST_DESYNC"))
                    testDesync = std::strtoull(value, nullptr, 10);
                if (const char* value = skyenv::Get("SKY_NETPLAY_TEST_NET"))
                {
                    testNet.emplace();
                    if (!ParseNetSim(value, *testNet, error))
                    {
                        error = "SKY_NETPLAY_TEST_NET: " + error;
                        return;
                    }
                }
                if (const char* value = skyenv::Get("SKY_NETPLAY_TEST_SLOW"))
                {
                    if (!ParseCount(value, 0, UINT32_MAX, false, number))
                    {
                        error = "SKY_NETPLAY_TEST_SLOW must be milliseconds from 0 to 4294967295";
                        return;
                    }
                    testSlowMs = uint32_t(number);
                }
                crossPlatform = is("SKY_NETPLAY_CROSS_PLATFORM", "1");
            }
        };
        const NetworkOptions& Network()
        {
            static const NetworkOptions options;
            return options;
        }

        void ExitSession(const SessionFailure& failure)
        {
            std::lock_guard lock(g_sessionExitMutex);
            if (g_endReason.load(std::memory_order_relaxed) != EndReason::None)
                return;
            if (failure.reason == EndReason::Desync)
                fprintf(stderr, "[netplay] session ended: desync frame %llu local=%016llx remote=%016llx\n",
                    static_cast<unsigned long long>(failure.frame), static_cast<unsigned long long>(failure.localHash),
                    static_cast<unsigned long long>(failure.remoteHash));
            else
                fprintf(stderr, "[netplay] session %s: %s\n",
                    failure.reason >= EndReason::Protocol && failure.reason <= EndReason::Seats ? "refused" : "ended",
                    ReasonText(failure.reason));
            fflush(stderr);
            auto* session = g_activeSession.load(std::memory_order_acquire);
            const bool continues = session && session->ContinuingSolo() && !g_leftLocally.load();
            if (continues)
            {
                g_savePolicy.End(Hosting(), true, failure.reason, false);
                PortalPeerLeft();
                if (failure.reason == EndReason::Desync)
                    snprintf(g_endMessage.data(), g_endMessage.size(), "%s%s", EndMessage(failure.reason),
                        Hosting() ? "" : " Nothing will be saved.");
                else if (Hosting())
                    snprintf(g_endMessage.data(), g_endMessage.size(), "%s left the game",
                        profiles::Current()[1].name.c_str());
                else
                    snprintf(g_endMessage.data(), g_endMessage.size(),
                        "The host left. You can keep playing, but nothing will be saved.");
            }
            else
                snprintf(g_endMessage.data(), g_endMessage.size(), "%s",
                    g_leftLocally.load() ? "You left the session" : EndMessage(failure.reason));
            g_continuingSolo.store(continues, std::memory_order_release);
            const auto reason =
                continues && failure.reason == EndReason::Transport ? EndReason::Timeout : failure.reason;
            if (!WriteEndFile(PortalSessionFolder(), reason))
                fprintf(stderr, "[netplay] cannot write online-ended.txt\n");
            g_endReason.store(reason, std::memory_order_release);
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

        struct Probe : BlockSource
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
            std::vector<netplay::Allocation> allocations; // of the hash being made
            std::array<std::vector<netplay::Allocation>, 30> slices; // the network window's parts (PlanWindow)
            std::vector<Block> windowBlocks;
            bool windowFull = false;
            std::vector<uint64_t> ends;                   // each block's end, while locating blocks
            std::vector<Range> repairRanges;
            bool locatingBlocks = false;

            Probe()
            {
                PrepareSessionSetup();
                if (SessionEndReason() != EndReason::None)
                {
                    g_probe.store(this, std::memory_order_release);
                    return;
                }
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
                if (hashPath || Network().enabled)
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
                if (Network().enabled)
                {
                    const auto xex = g_disc.ReadAll("default.xex");
                    const auto image = Image::ParseImage(xex.data(), xex.size());
                    if (!image.data)
                    {
                        SessionRefuse("cannot read executable image for SHA-256");
                        g_probe.store(this, std::memory_order_release);
                        return;
                    }
                    SessionConfig config;
                    config.hello = MakeHello(g_gameInfo.id, Sha256Hex({ image.data.get(), size_t(image.size) }),
                        ARKEYAN_VERSION, PlatformName(), FrameRateLimit(), Network().crossPlatform, Network().host);
                    std::random_device random;
                    for (auto& byte : config.hello.nonce)
                        byte = uint8_t(random());
                    config.password = Network().password;
                    config.delay = Network().delay;
                    // A lost connection holds both games and resumes on its own; after a minute the player decides.
                    config.timeoutMs = 60000;
                    config.lostMs = 3000;
                    config.askBeforeEnding = true;
                    config.adaptiveDelay = true;
                    config.balanceAdvantage = true;
                    config.slotPlayers = PortalOnlinePlayers();
                    config.prepareEvent = PortalPrepareOnline;
                    config.applyEvents = PortalApplyOnline;
                    config.blocks = this;
                    config.ended = [](const SessionFailure& failure) {
                        // A desync is handled at its hash barrier, where the guest's block dump is still available.
                        if (failure.reason != EndReason::Desync)
                        {
                            ExitSession(failure);
                        }
                    };
                    std::string error;
                    auto transport = OpenUdp(Network().address, Network().port, Network().host, error);
                    if (!transport)
                    {
                        SessionRefuse(error.c_str(), EndReason::Transport);
                        g_probe.store(this, std::memory_order_release);
                        return;
                    }
                    if (Network().testNet)
                        transport = SimulateNetwork(std::move(transport), *Network().testNet, [] {
                            return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch())
                                    .count());
                        });
                    session = std::make_unique<Session>(std::move(config), std::move(transport));
                    g_activeSession.store(session.get(), std::memory_order_release);
                }
                g_probe.store(this, std::memory_order_release);
            }

            ~Probe() override { Finish(); }

            void Finish()
            {
                if (finished)
                    return;
                finished = true;
                if (session)
                    session->Stop();
                PortalFinishOnline();
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
                    if (!session->ContinuingSolo() && !session->WaitReady())
                    {
                        SessionExit();
                        if (!session->ContinuingSolo())
                            return;
                    }
                    EventBatch pending;
                    {
                        std::lock_guard lock(g_pendingMutex);
                        pending.count = g_pending.count;
                        std::copy_n(g_pending.events.begin(), pending.count, pending.events.begin());
                        g_pending.count = 0;
                    }
                    if (pending.count && !session->QueueEvents(std::span(pending.events).first(pending.count)) &&
                        !session->ContinuingSolo())
                    {
                        ExitSession({ EndReason::Invalid });
                        return;
                    }
                    uint32_t ignored = 0;
                    const Pad local{ GetGamepad(ignored, 0), GamepadConnected(0) };
                    if (!session->Exchange(frames, local, frame))
                    {
                        SessionExit();
                        return;
                    }
                    if (session->ContinuingSolo())
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
                allocations.push_back({ std::span(bytes, size), address, heap });
            }

            // SKY_NETPLAY_TEST_DESYNC=<frame> (a session test, SKY_NETPLAY_TEST=1): the guest flips one hashed byte at
            // that frame's barrier, which the repair must put back before any guest code runs again.
            void BreakOneBlock()
            {
                ListBlocks();
                std::vector<uint8_t> bytes;
                const size_t index = repairRanges.size() / 2;
                if (repairRanges.empty() || !ReadBlock(index, bytes) || bytes.empty())
                    return;
                bytes[0] ^= 0x5A;
                if (WriteBlock(index, bytes))
                    fprintf(stderr, "[netplay] test: changed one byte at %08llx\n",
                        static_cast<unsigned long long>(repairRanges[index].begin));
            }

            std::span<const Block> ListBlocks() override
            {
                // Extents come from the probe's allocations, never from a peer-supplied address or size.
                repairRanges.clear();
                locatingBlocks = true;
                TotalHash();
                locatingBlocks = false;
                return blocks;
            }

            bool ReadBlock(size_t index, std::vector<uint8_t>& bytes) override
            {
                if (index >= repairRanges.size())
                    return false;
                const auto range = repairRanges[index];
                const auto* memory = static_cast<const uint8_t*>(g_memory.Translate(uint32_t(range.begin)));
                PackBlock({ memory, size_t(range.end - range.begin) }, uint32_t(range.begin), skips, bytes);
                return true;
            }

            bool WriteBlock(size_t index, std::span<const uint8_t> bytes) override
            {
                if (index >= repairRanges.size())
                    return false;
                const auto range = repairRanges[index];
                auto* memory = static_cast<uint8_t*>(g_memory.Translate(uint32_t(range.begin)));
                return ApplyBlock({ memory, size_t(range.end - range.begin) }, uint32_t(range.begin), skips, bytes);
            }

            void Scan(const GuestHeap& heap, Heap kind, uint32_t begin)
            {
                // NextAllocation avoids the allocating map copy made by ForEachAllocation while the guest is held, and
                // goes from one allocation to the next instead of page by page through the free space.
                for (uint64_t at = begin; at <= UINT32_MAX && heap.Contains(uint32_t(at));)
                {
                    uint32_t size = 0;
                    const uint32_t base = heap.NextAllocation(uint32_t(at), &size);
                    if (!base || !heap.Contains(base))
                        break;
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

            void SessionExit()
            {
                if (ended)
                    return;
                ended = true;
                const auto failure = session->Failure();
                if (failure.reason == EndReason::Desync && !Network().dump.empty())
                {
                    FILE* file = fopen(Network().dump.c_str(), "wb");
                    bool saved = file != nullptr;
                    if (file)
                    {
                        const auto number = [file](uint64_t value, size_t size) {
                            std::array<uint8_t, 8> bytes{};
                            for (size_t index = 0; index < size; ++index)
                                bytes[index] = uint8_t(value >> (index * 8));
                            return fwrite(bytes.data(), 1, size, file) == size;
                        };
                        saved = fwrite("SKYBLK01", 1, 8, file) == 8 && number(1, 4) && number(frames, 8) &&
                                number(blocks.size(), 4);
                        for (const auto& block : blocks)
                            saved = saved && number(block.address, 4) && number(uint32_t(block.heap), 4) &&
                                    number(block.hash, 8);
                        saved = fclose(file) == 0 && saved;
                    }
                    if (!saved)
                        fprintf(stderr, "[netplay] cannot write desync block dump\n");
                }
                ExitSession(failure);
            }

            uint64_t TotalHash() override
            {
                blocks.clear();
                Scan(g_virtualHeap4K, Heap::Virtual4K, 0x00010000);
                Scan(g_virtualHeap64K, Heap::Virtual64K, 0x40000000);
                Scan(g_physicalHeap, Heap::Physical, 0x00010000);
                for (const auto range : imageRanges)
                    Allocation(uint32_t(range.begin), uint32_t(range.end - range.begin), Heap::Image);
                HashAllocations(allocations, skips, blocks, uint32_t(blockBytes), locatingBlocks ? &ends : nullptr);
                allocations.clear();
                if (locatingBlocks)
                    for (size_t i = 0; i < blocks.size(); ++i)
                        repairRanges.push_back({ blocks[i].address, ends[i] });
                return CombineBlocks(blocks);
            }

            // The checkpoint's memory cut into 30 parts of about the same size, on block boundaries: one is hashed at
            // each frame of the window (at 60 FPS a whole hash would take a frame and more at once). Both games are at
            // the same frame, so each block is hashed at the same moment on both sides.
            void PlanWindow()
            {
                allocations.clear();
                Scan(g_virtualHeap4K, Heap::Virtual4K, 0x00010000);
                Scan(g_virtualHeap64K, Heap::Virtual64K, 0x40000000);
                Scan(g_physicalHeap, Heap::Physical, 0x00010000);
                for (const auto range : imageRanges)
                    Allocation(uint32_t(range.begin), uint32_t(range.end - range.begin), Heap::Image);
                SliceAllocations(allocations, uint32_t(blockBytes), slices);
                allocations.clear();
            }

            void Hash()
            {
                const bool online = session && !session->ContinuingSolo();
                const bool fileHash = totals && frames % hashEvery == 0;
                if (!online && !fileHash)
                    return;
                const auto start = std::chrono::steady_clock::now();
                if (online)
                {
                    const uint64_t phase = frames % slices.size();
                    if (phase == 0)
                    {
                        if (!Hosting() && Network().testDesync != 0 && frames == Network().testDesync)
                            BreakOneBlock();
                        // The window that just ended, compared at this barrier.
                        if (windowFull)
                        {
                            NetworkWait waiting;
                            if (!session->CheckHash(frames, CombineBlocks(windowBlocks)))
                            {
                                SessionExit();
                                return;
                            }
                        }
                        PlanWindow();
                        windowBlocks.clear();
                        windowFull = false;
                    }
                    HashAllocations(slices[phase], skips, windowBlocks, uint32_t(blockBytes));
                    windowFull = phase + 1 == slices.size();
                }
                if (!fileHash)
                {
                    const double ms =
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                    totalMs += ms;
                    maxMs = std::max(maxMs, ms);
                    ++hashed;
                    return;
                }
                const uint64_t total = TotalHash();
                {
                    if (fprintf(totals, "%llu %016llx\n", static_cast<unsigned long long>(frames),
                            static_cast<unsigned long long>(total)) < 0)
                        Refuse("hash totals write failed");
                    if (sidecar)
                        WriteBlocks(sidecar);
                    if (fflush(totals) != 0 || (sidecar && fflush(sidecar) != 0))
                        Refuse("hash output flush failed");
                }
                const double ms =
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
                totalMs += ms;
                maxMs = std::max(maxMs, ms);
                ++hashed;
            }
        };
    }

    void PrepareSessionSetup()
    {
        static std::once_flag checked;
        std::call_once(checked, [] {
            if (!Network().enabled)
                return;
            std::error_code error;
            std::filesystem::remove(std::filesystem::path(PortalSessionFolder()) / "online-ended.txt", error);
            if (!Network().error.empty())
                SessionRefuse(Network().error.c_str(), Network().errorReason);
        });
    }

    OnlineStatus SessionStatus()
    {
        if (auto* session = g_activeSession.load(std::memory_order_acquire))
            return session->Stats();
        return {};
    }
    EndReason SessionEndReason() { return g_endReason.load(std::memory_order_acquire); }
    const char* SessionEndMessage() { return SessionEndReason() == EndReason::None ? "" : g_endMessage.data(); }
    void SetMenuOpen(bool open)
    {
        if (auto* session = g_activeSession.load(std::memory_order_acquire))
            session->SetMenuOpen(open);
    }
    void ContinueAlone()
    {
        if (auto* session = g_activeSession.load(std::memory_order_acquire))
            session->ContinueAlone();
    }
    void KeepWaiting()
    {
        if (auto* session = g_activeSession.load(std::memory_order_acquire))
            session->KeepWaiting();
    }
    void PlayTogetherAgain()
    {
        if (!WriteReplayFile(PortalSessionFolder()))
            fprintf(stderr, "[netplay] cannot write online-replay.txt\n");
        fprintf(stderr, "[netplay] the player chose to play together again: quitting to the launcher\n");
        LeaveSession();
        SDL_Event quit{};
        quit.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quit);
    }
    std::string OtherPlayerName() { return profiles::Current()[Hosting() ? 1 : 0].name; }
    void LeaveSession()
    {
        g_leftLocally = true;
        if (auto* session = g_activeSession.load(std::memory_order_acquire))
            session->Stop();
        ExitSession({ EndReason::Quit });
    }
    bool QueuePortalEvents(std::span<const PortalEvent> events)
    {
        std::lock_guard lock(g_pendingMutex);
        if (SessionEndReason() != EndReason::None || events.empty() || !events.back().last ||
            events.size() > MaxFrameEvents - g_pending.count)
            return false;
        std::copy(events.begin(), events.end(), g_pending.events.begin() + g_pending.count);
        g_pending.count += events.size();
        return true;
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
        std::unique_lock lock(probe.mutex);
        const auto park = [&] {
            if (SessionEndReason() == EndReason::None || SessionContinues())
                return;
            lock.unlock();
            NetworkWait waiting;
            static std::mutex mutex;
            static std::condition_variable stopped;
            std::unique_lock waitLock(mutex);
            stopped.wait(waitLock, [] { return false; });
        };
        park();
        if (probe.finished)
            return;
        if (Network().testSlowMs)
            std::this_thread::sleep_for(std::chrono::milliseconds(Network().testSlowMs));
        if (probe.events && !probe.events->EndFrame(probe.frames))
            Refuse("event log write failed");
#ifdef ARKEYAN_NETPLAY_TRACE
        if (g_trace)
            g_trace->EndFrame(ProbeFrame());
#endif
        g_clock.Advance(presentInterval);
        WriteBundle();
        probe.Latch();
        park();
        ++probe.frames;
#ifdef ARKEYAN_NETPLAY_TRACE
        g_frame.store(uint32_t(probe.frames), std::memory_order_relaxed);
#endif
        probe.Hash();
        park();
    }

    void Finish()
    {
        if (Online())
        {
            if (auto* session = g_activeSession.load(std::memory_order_acquire))
                session->Stop();
            ExitSession({ EndReason::Quit });
        }
        if (!g_probe.load(std::memory_order_acquire))
            PortalFinishOnline();
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
