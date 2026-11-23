#include "netplay/session.h"
#include "netplay/net_sim.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <random>
#include <thread>

namespace
{
    void Check(bool value, const char* message)
    {
        if (!value)
        {
            fprintf(stderr, "FAIL: %s\n", message);
            std::_Exit(1);
        }
    }
    struct Link
    {
        std::mutex mutex;
        std::array<std::deque<netplay::Datagram>, 2> queues;
        std::mt19937 random{ 283 };
        bool chaos = false, silent = false, virtualTime = false, corrupt = false;
        bool stallRepair = false;
        uint64_t repairRequests = 0, repairChunks = 0;
        std::atomic<uint64_t> time{ 2000 };
        std::array<std::atomic<bool>, 2> healthy{ true, true };
    };
    class Wire : public netplay::Transport
    {
    public:
        Wire(Link& link, size_t seat) : m_link(link), m_seat(seat) {}
        bool Send(std::span<const uint8_t> bytes) override
        {
            std::lock_guard lock(m_link.mutex);
            if (m_link.virtualTime)
                ++m_link.time;
            if (bytes.size() > 10)
            {
                const auto type = netplay::PacketType(bytes[10]);
                m_link.repairRequests += type == netplay::PacketType::BlockRequest;
                if (type == netplay::PacketType::BlockList || type == netplay::PacketType::BlockData)
                {
                    ++m_link.repairChunks;
                    if (m_link.stallRepair)
                        return true;
                }
            }
            if (m_link.silent || (m_link.chaos && m_link.random() % 5 == 0))
                return true;
            netplay::Datagram packet{};
            packet.size = uint16_t(bytes.size());
            std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
            if (m_link.corrupt && m_link.random() % 7 == 0)
                packet.bytes[packet.size - 1] ^= 1;
            auto& queue = m_link.queues[1 - m_seat];
            if (m_link.chaos && m_link.random() % 3 == 0)
                queue.push_front(packet);
            else
                queue.push_back(packet);
            if (m_link.chaos && m_link.random() % 5 == 0)
                queue.push_back(packet);
            return true;
        }
        bool Healthy() const override { return m_link.healthy[m_seat].load(); }
        size_t Receive(std::span<uint8_t> bytes) override
        {
            std::lock_guard lock(m_link.mutex);
            auto& queue = m_link.queues[m_seat];
            if (queue.empty())
                return NoDatagram;
            const auto packet = queue.front();
            queue.pop_front();
            std::copy(packet.View().begin(), packet.View().end(), bytes.begin());
            return packet.size;
        }

    private:
        Link& m_link;
        size_t m_seat;
    };
    netplay::SessionConfig Config(bool host)
    {
        netplay::SessionConfig config;
        config.hello = netplay::MakeHello("test", std::string(64, 'b'), "test-runtime", "test-cpu", 30, false, host);
        config.hello.nonce[0] = host ? 12 : 27;
        config.password = "room";
        config.delay = host ? 4 : 8;
        config.timeoutMs = 5000;
        config.slotPlayers[0] = 1;
        config.slotPlayers[1] = 2;
        return config;
    }
    netplay::Pad Pad(uint64_t frame, size_t seat)
    {
        return { { uint16_t(1 + frame + seat * 1000), uint8_t(frame), uint8_t(frame >> 1), int16_t(-int64_t(frame)),
                     int16_t(seat), 123, -456 },
            true };
    }
    void Run(std::unique_ptr<netplay::Transport> host, std::unique_ptr<netplay::Transport> join, bool desync,
        uint32_t delay = 4)
    {
        auto config = Config(true);
        config.delay = delay;
        std::array<std::atomic<uint64_t>, 2> applied{};
        const auto apply = [&](size_t peer, const netplay::FrameEvents& events) {
            for (size_t player = 0; player < 2; ++player)
                for (size_t index = 0; index < events.players[player].count; ++index)
                {
                    const auto& event = events.players[player].events[index];
                    Check(event.frame >= delay && (event.frame - delay) % 7 == 0 && event.slot == player,
                        "event application runs at its frame barrier");
                    ++applied[peer];
                }
            return true;
        };
        config.applyEvents = [&](const netplay::FrameEvents& events) { return apply(0, events); };
        auto joinConfig = Config(false);
        joinConfig.applyEvents = [&](const netplay::FrameEvents& events) { return apply(1, events); };
        netplay::Session first(config, std::move(host));
        netplay::Session second(joinConfig, std::move(join));
        std::array<std::array<netplay::InputFrame, 600>, 2> results{};
        std::array<uint64_t, 2> stopped{};
        auto eventResults = std::make_unique<std::array<std::array<netplay::FrameEvents, 600>, 2>>();
        const auto play = [&](netplay::Session& session, size_t seat) {
            Check(session.WaitReady(), "session ready");
            Check(session.Delay() == delay, "host delay wins");
            for (uint64_t frame = 0; frame < 600; ++frame)
            {
                netplay::PortalEvent event;
                event.slot = uint8_t(seat);
                event.figure = frame + 1;
                event.hasDump = true;
                event.last = true;
                event.dump.fill(uint8_t(frame + seat));
                if (frame % 7 == 0)
                    Check(session.QueueEvents(std::span(&event, 1)), "queue event");
                if (frame == 0 && seat == 1)
                {
                    event.slot = 0;
                    Check(!session.QueueEvents(std::span(&event, 1)), "guest cannot change player one seat");
                }
                Check(session.Exchange(frame, Pad(frame, seat), results[seat][frame], &(*eventResults)[seat][frame]),
                    "exchange frame and reliable events");
                if ((frame + 1) % 30 == 0 &&
                    !session.CheckHash(frame + 1, desync && seat == 1 && frame == 299 ? 999 : frame + 1))
                {
                    stopped[seat] = frame + 1;
                    const auto failure = session.Failure();
                    fprintf(stderr, "session stopped: seat=%zu frame=%llu reason=%s\n", seat,
                        static_cast<unsigned long long>(frame) + 1, ReasonText(failure.reason));
                    Check(failure.reason == netplay::EndReason::Desync, "desync reason");
                    Check(failure.frame == 300 && failure.localHash == (seat == 0 ? 300u : 999u) &&
                              failure.remoteHash == (seat == 0 ? 999u : 300u),
                        "desync frame and both hashes");
                    return;
                }
            }
        };
        std::thread a([&] { play(first, 0); });
        std::thread b([&] { play(second, 1); });
        a.join();
        b.join();
        const uint64_t frames = desync ? 300 : 600;
        Check(applied[0] == 2 * ((frames - delay + 6) / 7) && applied[1] == applied[0],
            "both event sinks apply each event exactly once");
        Check(first.Stats().goodChecks == (desync ? 9u : 20u) && second.Stats().goodChecks == first.Stats().goodChecks,
            "successful hash checks counted exactly");
        if (desync)
            Check(stopped[0] == 300 && stopped[1] == 300, "both stop at hash frame 300");
        for (uint64_t frame = 0; frame < frames; ++frame)
        {
            Check(netplay::EncodeFrame(results[0][frame]) == netplay::EncodeFrame(results[1][frame]), "same pad pair");
            for (size_t player = 0; player < 2; ++player)
            {
                const bool due = frame >= delay && (frame - delay) % 7 == 0;
                const auto& aEvents = (*eventResults)[0][frame].players[player];
                const auto& bEvents = (*eventResults)[1][frame].players[player];
                Check(aEvents.count == (due ? 1 : 0) && bEvents.count == aEvents.count,
                    "events delivered exactly once at N plus D on both peers");
                if (due)
                {
                    const auto& aEvent = aEvents.events[0];
                    const auto& bEvent = bEvents.events[0];
                    Check(aEvent.figure == frame - delay + 1 && aEvent.frame == frame &&
                              aEvent.sequence == (frame - delay) / 7 + 1 && aEvent.slot == player &&
                              aEvent.dump == bEvent.dump && aEvent.dump[0] == uint8_t(frame - delay + player),
                        "ordered event identity, frame and dump");
                }
            }
            netplay::InputFrame expected{};
            if (frame >= delay)
                expected = { Pad(frame - delay, 0), Pad(frame - delay, 1) };
            Check(netplay::EncodeFrame(results[0][frame]) == netplay::EncodeFrame(expected), "pads arrive at N plus D");
        }
    }
    void RoundTrips()
    {
        using namespace netplay;
        Link link;
        link.virtualTime = true;
        auto host = Config(true), join = Config(false);
        host.now = join.now = [&] { return link.time.load(); };
        host.timeoutMs = join.timeoutMs = 100000;
        Session first(host, std::make_unique<Wire>(link, 0));
        Session second(join, std::make_unique<Wire>(link, 1));
        Check(first.WaitReady() && second.WaitReady(), "ready before RTT refresh");
        link.time += 1001;
        for (unsigned wait = 0; wait < 1000 && !second.RoundTripMs(); ++wait)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Check(first.RoundTripMs() > 0 && second.RoundTripMs() > 0, "both peers measure the round trip for F3");
    }

    struct Blocks : netplay::BlockSource
    {
        std::vector<netplay::Block> blocks;
        std::vector<std::vector<uint8_t>> bytes;
        bool refuseWrite = false;
        explicit Blocks(size_t count = 3)
        {
            for (size_t i = 0; i < count; ++i)
            {
                blocks.push_back({ uint32_t(0x10000 + i * 0x10000), netplay::Heap::Virtual4K, 0 });
                bytes.emplace_back(4097, uint8_t(i));
            }
            TotalHash();
        }
        std::span<const netplay::Block> ListBlocks() override { return blocks; }
        bool ReadBlock(size_t index, std::vector<uint8_t>& result) override
        {
            result = bytes.at(index);
            return true;
        }
        bool WriteBlock(size_t index, std::span<const uint8_t> value) override
        {
            if (value.size() != bytes.at(index).size())
                return false;
            if (!refuseWrite)
                std::copy(value.begin(), value.end(), bytes[index].begin());
            return true;
        }
        uint64_t TotalHash() override
        {
            uint64_t total = netplay::HashSeed;
            for (size_t i = 0; i < blocks.size(); ++i)
            {
                blocks[i].hash = netplay::HashBytes(bytes[i], blocks[i].address, {});
                total = netplay::Mix(total, (uint64_t(blocks[i].address) << 32) | uint32_t(blocks[i].heap));
                total = netplay::Mix(total, blocks[i].hash);
            }
            return total;
        }
    };

    void Repair(bool chaos, size_t differences = 1, bool layout = false, bool badTotal = false, unsigned attempts = 1,
        bool expire = false, bool stall = false)
    {
        using namespace netplay;
        Link link;
        link.chaos = chaos;
        link.stallRepair = stall;
        Blocks hostBlocks(std::max(size_t(80), differences)), guestBlocks = hostBlocks;
        if (differences == 64)
            for (size_t i = 0; i < differences; ++i)
            {
                hostBlocks.bytes[i].resize(65536, uint8_t(i));
                guestBlocks.bytes[i] = hostBlocks.bytes[i];
            }
        guestBlocks.refuseWrite = badTotal;
        if (layout)
            guestBlocks.blocks.back().heap = Heap::Physical;
        auto host = Config(true), guest = Config(false);
        std::atomic<uint64_t> time{ 2000 };
        host.now = guest.now = [&] { return time.load(); };
        host.timeoutMs = guest.timeoutMs = 100000;
        host.blocks = &hostBlocks;
        guest.blocks = &guestBlocks;
        Session first(host, std::make_unique<Wire>(link, 0));
        Session second(guest, std::make_unique<Wire>(link, 1));
        Check(first.WaitReady() && second.WaitReady(), "repair session ready");
        uint64_t frame = 0;
        for (unsigned attempt = 0; attempt < attempts; ++attempt)
        {
            for (size_t i = 0; i < differences; ++i)
                guestBlocks.bytes[i][0] ^= 0x80;
            const auto hostHash = hostBlocks.TotalHash(), guestHash = guestBlocks.TotalHash();
            auto advance = [&](Session& session, size_t seat) {
                InputFrame pads;
                for (uint64_t f = frame; f < frame + 30; ++f)
                    Check(session.Exchange(f, Pad(f, seat), pads), "frames advance before repair");
            };
            std::thread a([&] { advance(first, 0); });
            std::thread b([&] { advance(second, 1); });
            a.join();
            b.join();
            frame += 30;
            std::array<bool, 2> repaired{};
            std::thread c([&] { repaired[0] = first.CheckHash(frame, hostHash); });
            std::thread d([&] { repaired[1] = second.CheckHash(frame, guestHash); });
            if (stall)
            {
                bool requested = false;
                for (unsigned wait = 0; wait < 1000; ++wait)
                {
                    {
                        std::lock_guard lock(link.mutex);
                        requested = link.repairRequests > 0 && link.repairChunks > 0;
                    }
                    if (requested && first.Stats().repairing && second.Stats().repairing)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                Check(requested && first.Stats().repairing && second.Stats().repairing,
                    "both games remain held during repair");
                time += 5001;
            }
            c.join();
            d.join();
            const bool failed = layout || differences > 64 || badTotal || (attempt == 2 && !expire) || stall;
            Check(repaired[0] == !failed && repaired[1] == !failed, "both peers agree on repair outcome");
            if (failed)
            {
                for (auto* session : { &first, &second })
                {
                    Check(session->Failure().reason == EndReason::Desync && session->ContinuingSolo(),
                        "failed repair continues solo with desync reason");
                    InputFrame pads;
                    Check(session->Exchange(frame, Pad(frame, 0), pads), "failed repair releases frame barrier");
                }
                break;
            }
            Check(first.Stats().resyncs == attempt + 1 && second.Stats().resyncs == attempt + 1,
                "successful repairs counted on both peers");
            Check(hostBlocks.bytes == guestBlocks.bytes && first.Stats().backInSync && second.Stats().backInSync,
                "repair copies complete blocks and shows back in sync");
            time += 2000;
            Check(!first.Stats().backInSync && !second.Stats().backInSync, "back in sync expires after two seconds");
            if (expire)
                time += 60000;
            std::thread e([&] { advance(first, 0); });
            std::thread f([&] { advance(second, 1); });
            e.join();
            f.join();
            frame += 30;
            std::thread g(
                [&] { Check(first.CheckHash(frame, hostBlocks.TotalHash()), "host stays in sync after repair"); });
            std::thread h(
                [&] { Check(second.CheckHash(frame, guestBlocks.TotalHash()), "guest stays in sync after repair"); });
            g.join();
            h.join();
            Check(first.Stats().resyncs == attempt + 1 && second.Stats().resyncs == attempt + 1,
                "ordinary checkpoints do not count as repairs");
        }
        if (!layout && differences <= 64 && attempts == 1)
        {
            std::lock_guard lock(link.mutex);
            Check(link.repairRequests > 0 && link.repairChunks > 0, "repair uses requested chunks");
        }
    }

    void OnlineMenuAndStats()
    {
        using namespace netplay;
        Link link;
        link.chaos = link.virtualTime = link.corrupt = true;
        auto host = Config(true), join = Config(false);
        host.now = join.now = [&] { return link.time.load(); };
        host.timeoutMs = join.timeoutMs = 100000;
        Session first(host, std::make_unique<Wire>(link, 0));
        Session second(join, std::make_unique<Wire>(link, 1));
        Check(first.WaitReady() && second.WaitReady(), "statistics session ready");
        for (bool open : { true, false })
        {
            first.SetMenuOpen(open);
            for (unsigned i = 0; i < 1000 && second.Stats().peerMenuOpen != open; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Check(second.Stats().peerMenuOpen == open, "peer sees menu opening and closing through loss");
        }
        auto play = [&](Session& session, size_t seat) {
            InputFrame pads;
            for (uint64_t frame = 0; frame < 180; ++frame)
                Check(session.Exchange(frame, Pad(frame, seat), pads), "lossy statistics exchange");
        };
        std::thread a([&] { play(first, 0); });
        std::thread b([&] { play(second, 1); });
        a.join();
        b.join();
        const auto stats = first.Stats();
        Check(stats.lost > 0 && stats.rejected > 0 && stats.waitedFrames > 0 && stats.waitMs > 0,
            "loss, bad tags and input waits counted independently");
        Check(stats.sentPerSecond > 0 && stats.receivedPerSecond > 0 && stats.sentKB > 0 && stats.receivedKB > 0,
            "packet and byte rates measured");
        Check(stats.pingCount > 0 && stats.minPingMs <= stats.roundTripMs && stats.maxPingMs >= stats.minPingMs,
            "ping history and bounds available");
        link.time += 61000;
        Check(first.Stats().waitedFrames == 0 && first.Stats().waitMs == 0, "input waits expire after sixty seconds");
    }
    // A dropped connection pauses both games and resumes on its own; after the wait the player is asked, and a peer
    // that answers but does not advance is waited for.
    void Reconnection()
    {
        using namespace netplay;
        Link link;
        std::atomic<uint64_t> time{ 0 };
        const auto configure = [&](bool host) {
            auto config = Config(host);
            config.now = [&] { return time.load(); };
            config.delay = 2;
            config.timeoutMs = 60000;
            config.lostMs = 3000;
            config.askBeforeEnding = true;
            return config;
        };
        Session host(configure(true), std::make_unique<Wire>(link, 0));
        Session guest(configure(false), std::make_unique<Wire>(link, 1));
        Check(host.WaitReady() && guest.WaitReady(), "ready before the drop");
        std::atomic<uint64_t> hostFrame{ 0 }, guestFrame{ 0 };
        std::atomic<bool> stop{ false };
        const auto play = [&](Session& session, size_t seat, std::atomic<uint64_t>& frame) {
            InputFrame pads;
            while (!stop && session.Failure().reason == EndReason::None)
            {
                if (!session.Exchange(frame, Pad(frame, seat), pads))
                    break;
                ++frame;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        };
        std::jthread a([&] { play(host, 0, hostFrame); });
        std::jthread b([&] { play(guest, 1, guestFrame); });
        const auto until = [&](const std::function<bool()>& done) {
            for (unsigned i = 0; i < 20000 && !done(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return done();
        };
        Check(until([&] { return hostFrame > 100 && guestFrame > 100; }), "frames advance before the drop");
        {
            std::lock_guard lock(link.mutex);
            link.silent = true;
        }
        // Ten seconds without a packet: both hold, report a lost connection, and nothing ends.
        for (unsigned i = 0; i < 100; ++i)
        {
            time.fetch_add(100);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Check(until([&] { return host.Stats().connectionLost && guest.Stats().connectionLost; }),
            "a silence of lostMs is a lost connection on both sides");
        const uint64_t held = hostFrame;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        Check(hostFrame <= held + 3 && host.Failure().reason == EndReason::None && !host.Stats().askContinue,
            "a lost connection holds the game without ending it");
        {
            std::lock_guard lock(link.mutex);
            link.silent = false;
        }
        Check(until([&] {
            return !host.Stats().connectionLost && !guest.Stats().connectionLost && hostFrame > held + 100;
        }),
            "the connection comes back on its own and frames advance again");
        Check(host.Stats().reconnects == 1 && guest.Stats().reconnects == 1, "each side counts the reconnection");
        // A minute and more: the player is asked; waiting more clears the question, continuing alone ends.
        {
            std::lock_guard lock(link.mutex);
            link.silent = true;
        }
        for (unsigned i = 0; i < 700; ++i)
        {
            time.fetch_add(100);
            std::this_thread::sleep_for(std::chrono::microseconds(300));
        }
        Check(until([&] { return host.Stats().askContinue; }) && host.Failure().reason == EndReason::None,
            "after timeoutMs the player is asked, the session still open");
        host.KeepWaiting();
        Check(!host.Stats().askContinue && host.Stats().connectionLost, "waiting more clears the question");
        host.ContinueAlone();
        Check(host.Failure().reason == EndReason::Timeout, "continuing alone ends the session for the solo path");
        stop = true;
        guest.Stop();
    }

    // Each side moves its own input delay during play: the frames it skips repeat its last input and the input of a
    // frame that falls on one already sent is dropped, so both games still read the same pads for every frame.
    void AdaptiveDelay()
    {
        using namespace netplay;
        const auto ms = [] {
            return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        };
        for (const bool network : { true, false })
        {
            Link link;
            const auto configure = [&](bool host) {
                auto config = Config(host);
                config.delay = 1;
                config.timeoutMs = 100000;
                config.adaptiveDelay = true;
                return config;
            };
            // 60 ms each way with 20 ms of jitter (reordering): a round trip near 140 ms asks for four frames at 30.
            NetSim sim;
            sim.delayMs = network ? 60 : 0;
            sim.jitterMs = network ? 20 : 0;
            Session host(configure(true), SimulateNetwork(std::make_unique<Wire>(link, 0), sim, ms));
            Session guest(configure(false), SimulateNetwork(std::make_unique<Wire>(link, 1), sim, ms));
            Check(host.WaitReady() && guest.WaitReady(), "ready before the adaptive delay");
            const uint64_t frames = network ? 300 : 600;
            std::vector<std::pair<uint16_t, uint16_t>> seen[2];
            uint32_t highest[2] = {};
            const auto play = [&](Session& session, size_t seat, bool slow) {
                seen[seat].resize(frames);
                InputFrame pads;
                for (uint64_t frame = 0; frame < frames; ++frame)
                {
                    if (network)
                        std::this_thread::sleep_for(std::chrono::milliseconds(8));
                    // Without network delay, the guest is a slow PC between frames 100 and 400.
                    if (slow && frame > 100 && frame < 400)
                        std::this_thread::sleep_for(std::chrono::milliseconds(4));
                    Check(session.Exchange(frame, Pad(frame, seat), pads), "adaptive exchange");
                    seen[seat][frame] = { pads[0].state.buttons, pads[1].state.buttons };
                    highest[seat] = std::max(highest[seat], session.Delay());
                }
            };
            std::jthread a([&] { play(host, 0, false); });
            play(guest, 1, !network);
            a.join();
            Check(seen[0] == seen[1], "both games read the same pads for every frame while the delay moves");
            if (network)
                Check(highest[0] >= 3 && highest[1] >= 3, "the delay rises with the round trip");
            else
                Check(highest[0] <= 3, "a slower PC's waits add one frame at most");
            host.Stop();
            guest.Stop();
        }
    }

    // The side that runs ahead slows a little every 30 frames; the pads both games read stay the same.
    void BalancedAdvantage()
    {
        using namespace netplay;
        Link link;
        const auto configure = [&](bool host) {
            auto config = Config(host);
            config.delay = 4;
            config.timeoutMs = 100000;
            config.balanceAdvantage = true;
            return config;
        };
        Session host(configure(true), std::make_unique<Wire>(link, 0));
        Session guest(configure(false), std::make_unique<Wire>(link, 1));
        Check(host.WaitReady() && guest.WaitReady(), "ready before the balance");
        constexpr uint64_t Frames = 240;
        std::vector<std::pair<uint16_t, uint16_t>> seen[2];
        const auto play = [&](Session& session, size_t seat, int frameMs) {
            seen[seat].resize(Frames);
            InputFrame pads;
            for (uint64_t frame = 0; frame < Frames; ++frame)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(frameMs));
                Check(session.Exchange(frame, Pad(frame, seat), pads), "balanced exchange");
                seen[seat][frame] = { pads[0].state.buttons, pads[1].state.buttons };
            }
        };
        // The host's frames are quicker: it keeps running into the delay's edge, ahead, until the balance slows it.
        std::jthread a([&] { play(host, 0, 2); });
        play(guest, 1, 9);
        a.join();
        Check(seen[0] == seen[1], "both games read the same pads while one slows");
        Check(host.Stats().balanceSlows > guest.Stats().balanceSlows, "the side ahead is the one that slows");
        host.Stop();
        guest.Stop();
    }

    // A peer that answers but does not advance (a long load on a slower PC) is not a lost connection and ends nothing.
    void SlowPeer()
    {
        using namespace netplay;
        Link link;
        std::atomic<uint64_t> time{ 0 };
        const auto configure = [&](bool host) {
            auto config = Config(host);
            config.now = [&] { return time.load(); };
            config.delay = 2;
            config.timeoutMs = 5000;
            config.askBeforeEnding = true;
            return config;
        };
        Session host(configure(true), std::make_unique<Wire>(link, 0));
        Session guest(configure(false), std::make_unique<Wire>(link, 1));
        Check(host.WaitReady() && guest.WaitReady(), "ready before the slow peer");
        std::jthread clock([&](const std::stop_token& stop) {
            while (!stop.stop_requested())
            {
                time.fetch_add(50);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        InputFrame pads;
        // The guest never sends frame 2 and on: the host waits through many timeouts, with pings answered meanwhile.
        std::jthread waiting([&] {
            for (uint64_t frame = 0; frame < 10 && host.Failure().reason == EndReason::None; ++frame)
                host.Exchange(frame, Pad(frame, 0), pads);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        const auto stats = host.Stats();
        Check(host.Failure().reason == EndReason::None && stats.waiting && !stats.connectionLost,
            "a peer that answers but does not advance is waited for, not lost");
        host.ContinueAlone();
        Check(host.Failure().reason == EndReason::Timeout, "the waiting player may continue alone");
        clock.request_stop();
        guest.Stop();
    }

    void RemoteDeparture()
    {
        using namespace netplay;
        for (bool hosting : { true, false })
            for (auto reason : { EndReason::Quit, EndReason::Transport })
            {
                Link link;
                Session survivor(Config(hosting), std::make_unique<Wire>(link, 0));
                Session leaver(Config(!hosting), std::make_unique<Wire>(link, 1));
                Check(survivor.WaitReady() && leaver.WaitReady(), "ready before remote departure");
                if (reason == EndReason::Quit)
                    leaver.Stop();
                else
                    link.healthy[0] = false;
                for (unsigned wait = 0; wait < 1000 && survivor.Failure().reason == EndReason::None; ++wait)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                Check(!survivor.WaitReady() && survivor.Failure().reason == reason, "network session ended");
                for (uint64_t frame = 0; frame < 90; ++frame)
                {
                    InputFrame pads;
                    FrameEvents events;
                    events.players[0].count = 1;
                    Check(survivor.Exchange(frame, Pad(frame, 0), pads, &events), "survivor keeps advancing frames");
                    const auto seat = hosting ? 0u : 1u;
                    Check(pads[seat].connected && pads[seat].state.buttons == Pad(frame, 0).state.buttons &&
                              !pads[1 - seat].connected && EncodeFrame({ pads[1 - seat] }) == EncodeFrame({}),
                        "survivor retains seat; departed pad neutral and disconnected");
                    Check(
                        events.players[0].count == 0 && events.players[1].count == 0, "pending peer events discarded");
                }
            }
    }

    void Bursts()
    {
        using namespace netplay;
        Link link;
        link.chaos = true;
        auto host = Config(true), join = Config(false);
        host.delay = 8;
        host.timeoutMs = join.timeoutMs = 10000;
        std::array<std::array<bool, 4>, 2> known{};
        const auto prepare = [&](size_t receiver, const PortalEvent& event) {
            if (!event.figure)
                return true;
            const size_t id = size_t(event.figure - 1);
            Check(
                id < 4 && event.hasDump == !known[receiver][id], "first reference carries the stand-in dump only once");
            known[receiver][id] = true;
            return true;
        };
        host.prepareEvent = [&](bool, const PortalEvent& event) { return prepare(0, event); };
        join.prepareEvent = [&](bool, const PortalEvent& event) { return prepare(1, event); };
        auto first = std::make_unique<Session>(host, std::make_unique<Wire>(link, 0));
        auto second = std::make_unique<Session>(join, std::make_unique<Wire>(link, 1));
        const auto play = [&](Session& session, size_t player) {
            Check(session.WaitReady(), "burst session ready");
            std::array<PortalEvent, MaxFrameEvents + 1> pending{};
            pending.back().last = true;
            Check(!session.QueueEvents(pending), "oversized pending batch refused");
            for (uint64_t frame = 0; frame < 88; ++frame)
            {
                for (size_t index = 0; index < MaxFrameEvents; ++index)
                {
                    auto& event = pending[index];
                    event.slot = uint8_t(player);
                    event.figure = index % 2 ? 0 : 1 + frame % 4;
                    event.hasDump = frame < 4 && index == 0;
                    event.dump.fill(uint8_t(frame % 4));
                    event.last = index % 4 == 3;
                }
                if (frame < 80)
                    Check(session.QueueEvents(std::span(pending).first(MaxFrameEvents)),
                        "queue compound replacements and removals");
                InputFrame pads;
                FrameEvents events;
                Check(session.Exchange(frame, Pad(frame, player), pads, &events),
                    "exchange event burst across ring wrap");
                for (size_t owner = 0; owner < 2; ++owner)
                {
                    const auto& received = events.players[owner];
                    Check(received.count == (frame < 8 ? 0 : MaxFrameEvents), "complete burst waits for its frame");
                    for (size_t index = 0; index < received.count; ++index)
                    {
                        const auto& event = received.events[index];
                        Check(event.sequence == (frame - 8) * MaxFrameEvents + index + 1 && event.slot == owner &&
                                  event.frame == frame && event.last == (index % 4 == 3) &&
                                  event.figure == (index % 2 ? 0 : 1 + (frame - 8) % 4),
                            "burst order, transaction boundary, removals and references preserved");
                    }
                }
            }
        };
        std::thread a([&] { play(*first, 0); });
        std::thread b([&] { play(*second, 1); });
        a.join();
        b.join();
    }

    void Refusals()
    {
        using namespace netplay;
        const auto refuse = [](const auto& change, EndReason reason) {
            Link link;
            auto config = Config(false);
            change(config.hello);
            Session first(Config(true), std::make_unique<Wire>(link, 0));
            Session second(config, std::make_unique<Wire>(link, 1));
            Check(!first.WaitReady() && !second.WaitReady(), "both peers refuse a mismatch");
            Check(first.Failure().reason == reason && second.Failure().reason == reason, "same refusal reason");
        };
        refuse([](Hello& h) { ++h.protocol; }, EndReason::Protocol);
        refuse([](Hello& h) { h.game[0] ^= 1; }, EndReason::Game);
        refuse([](Hello& h) { h.image[0] = 'c'; }, EndReason::Image);
        refuse([](Hello& h) { h.runtime[0] ^= 1; }, EndReason::Runtime);
        refuse([](Hello& h) { h.platform[0] ^= 1; }, EndReason::Platform);
        refuse([](Hello& h) { h.fps = 60; }, EndReason::Fps);
    }
    void TimeoutAndQuit()
    {
        using namespace netplay;
        {
            Link link;
            auto config = Config(true);
            std::atomic<uint64_t> time{ 0 };
            config.now = [&] { return time.load(); };
            config.timeoutMs = 15000;
            Session first(config, std::make_unique<Wire>(link, 0));
            Session second(Config(false), std::make_unique<Wire>(link, 1));
            Check(first.WaitReady() && second.WaitReady(), "ready before silence");
            {
                std::lock_guard lock(link.mutex);
                link.silent = true;
                link.queues = {};
            }
            std::jthread advance([&](const std::stop_token& stop) {
                while (!stop.stop_requested())
                {
                    time.fetch_add(1000);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
            InputFrame pads;
            uint64_t frame = 0;
            while (first.Failure().reason == EndReason::None && frame < 1000)
                Check(first.Exchange(frame++, Pad(0, 0), pads), "timeout keeps exchanging neutral remote input");
            Check(first.Exchange(frame, Pad(123, 0), pads) && pads[0].state.buttons == Pad(123, 0).state.buttons &&
                      !pads[1].connected && pads[1].state.buttons == 0,
                "timeout retains local pad and disconnects peer");
            Check(first.Failure().reason == EndReason::Timeout, "silent established peer ends after virtual timeout");
        }
        {
            Link link;
            link.queues[0].push_back({});
            netplay::Datagram malformed{};
            malformed.size = 3;
            link.queues[0].push_back(malformed);
            std::atomic<bool> ended{ false };
            Session* observer = nullptr;
            auto config = Config(false);
            config.ended = [&](const SessionFailure& failure) {
                Check(observer && failure.reason == EndReason::Quit && observer->Failure().reason == EndReason::Quit,
                    "end notification runs outside the session lock");
                ended = true;
            };
            Session first(Config(true), std::make_unique<Wire>(link, 0));
            Session second(config, std::make_unique<Wire>(link, 1));
            observer = &second;
            Check(first.WaitReady() && second.WaitReady(), "ready before quit");
            Check(first.Dropped() == 2, "zero-length and malformed transport datagrams counted");
            first.Stop();
            Check(!second.WaitReady() && second.Failure().reason == EndReason::Quit, "quit reaches the peer");
            for (unsigned i = 0; i < 1000 && !ended; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            Check(ended, "end notification reaches the host without another frame");
        }
    }

}
int main()
{
    using namespace netplay;
    Repair(false);
    Repair(true);
    Repair(true, 64);
    Repair(false, 1, true);
    Repair(false, 65);
    Repair(false, 1, false, true);
    Repair(false, 1, false, false, 3);
    Repair(false, 1, false, false, 3, true);
    Repair(false, 64, false, false, 1, false, true);
    std::string endpointHost;
    uint16_t port = 0;
    for (const auto text : { "127.0.0.1:47640", "[::1]:47640", "localhost:47640" })
        Check(ParseEndpoint(text, endpointHost, port) && port == 47640, "IPv4, bracketed IPv6 and DNS endpoints");
    for (const auto text : { "", ":123", "::1:123", "[::1]", "host:0", "host:65536", "host:+2", "host:12x" })
        Check(!ParseEndpoint(text, endpointHost, port), "invalid endpoint refused");
    OnlineMenuAndStats();
    Reconnection();
    AdaptiveDelay();
    BalancedAdvantage();
    SlowPeer();
    RemoteDeparture();
    RoundTrips();
    Bursts();
    Refusals();
    TimeoutAndQuit();
    for (bool chaos : { false, true })
        for (bool desync : { false, true })
        {
            Link link;
            link.chaos = chaos;
            Run(std::make_unique<Wire>(link, 0), std::make_unique<Wire>(link, 1), desync);
        }
    for (uint32_t delay = 1; delay <= 8; ++delay)
    {
        Link link;
        link.chaos = true;
        Run(std::make_unique<Wire>(link, 0), std::make_unique<Wire>(link, 1), false, delay);
    }
    {
        Link link;
        auto config = Config(true);
        std::atomic<uint64_t> time{ 0 };
        config.now = [&] { return time.load(); };
        Session session(config, std::make_unique<Wire>(link, 0));
        time = 16000;
        Check(!session.WaitReady() && session.Failure().reason == EndReason::Timeout, "silent peer timeout");
    }
    for (bool desync : { false, true })
    {
        std::string error;
        auto host = OpenUdp("", 0, true, error);
        if (!host)
        {
            fprintf(stderr, "SKIP real UDP: %s\n", error.c_str());
            break;
        }
        auto join = OpenUdp("127.0.0.1", host->Port(), false, error);
        Check(bool(join), "join UDP socket");
        Run(std::move(host), std::move(join), desync);
    }
    {
        // The launcher's punched path: a host knocks at the joiner's address until the joiner's first packet, and a
        // joiner binds the port its launcher used.
        std::string error;
        auto receiver = OpenUdp("", 0, true, error);
        if (receiver)
        {
            UdpOptions knocking;
            knocking.knockHost = "127.0.0.1";
            knocking.knockPort = receiver->Port();
            auto host = OpenUdp("", 0, true, error, knocking);
            Check(bool(host), "a host with a knock target opens");
            host->Knock();
            std::array<uint8_t, 64> bytes{};
            size_t got = Transport::NoDatagram;
            for (unsigned i = 0; i < 200 && got == Transport::NoDatagram; ++i)
            {
                got = receiver->Receive(bytes);
                if (got == Transport::NoDatagram)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            Check(got == 1, "the host's knock reaches the joiner's address");
            uint16_t free = 0;
            {
                auto probe = OpenUdp("", 0, true, error);
                Check(bool(probe), "a port to reuse");
                free = probe->Port();
            }
            UdpOptions local;
            local.localPort = free;
            auto join = OpenUdp("127.0.0.1", host->Port(), false, error, local);
            Check(join && join->Port() == free, "a joiner binds the port its launcher punched through");
        }
        else
            fprintf(stderr, "SKIP real UDP knock: %s\n", error.c_str());
    }
    std::puts("session: 600 frames, loss, duplicates, reordering, desync and timeout passed");
}
