#include "netplay/clock.h"
#include "netplay/scheduler.h"
#include "netplay/events.h"
#include "netplay/inputs.h"
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
        std::atomic<EndReason> g_endReason{ EndReason::None };
        std::atomic<Session*> g_activeSession{ nullptr };
        std::mutex g_sessionExitMutex, g_pendingMutex;
        EventBatch g_pending;
        bool g_leftLocally = false;

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
            if (!WriteEndFile(PortalSessionFolder(), failure.reason))
                fprintf(stderr, "[netplay] cannot write online-ended.txt\n");
            g_endReason.store(failure.reason, std::memory_order_release);
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
                    config.slotPlayers = PortalOnlinePlayers();
                    config.prepareEvent = PortalPrepareOnline;
                    config.applyEvents = PortalApplyOnline;
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
                    session = std::make_unique<Session>(std::move(config), std::move(transport));
                    g_activeSession.store(session.get(), std::memory_order_release);
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
                    if (!session->WaitReady())
                    {
                        SessionExit();
                        return;
                    }
                    EventBatch pending;
                    {
                        std::lock_guard lock(g_pendingMutex);
                        pending.count = g_pending.count;
                        std::copy_n(g_pending.events.begin(), pending.count, pending.events.begin());
                        g_pending.count = 0;
                    }
                    if (pending.count && !session->QueueEvents(std::span(pending.events).first(pending.count)))
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
                    {
                        SessionExit();
                        return;
                    }
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
            return { session->RoundTripMs(), session->LastGoodHash(), session->Delay() };
        return {};
    }
    EndReason SessionEndReason() { return g_endReason.load(std::memory_order_acquire); }
    const char* SessionEndMessage() { return g_leftLocally ? "You left the session" : EndMessage(SessionEndReason()); }
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
            if (SessionEndReason() == EndReason::None)
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
