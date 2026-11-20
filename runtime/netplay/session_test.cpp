#include "netplay/session.h"
#include <algorithm>
#include <atomic>
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
        bool chaos = false, silent = false, virtualTime = false;
        std::atomic<uint64_t> time{ 2000 };
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
            if (m_link.silent || (m_link.chaos && m_link.random() % 5 == 0))
                return true;
            netplay::Datagram packet{};
            packet.size = uint16_t(bytes.size());
            std::copy(bytes.begin(), bytes.end(), packet.bytes.begin());
            auto& queue = m_link.queues[1 - m_seat];
            if (m_link.chaos && m_link.random() % 3 == 0)
                queue.push_front(packet);
            else
                queue.push_back(packet);
            if (m_link.chaos && m_link.random() % 5 == 0)
                queue.push_back(packet);
            return true;
        }
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
                        static_cast<unsigned long long>(frame + 1), ReasonText(failure.reason));
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
            std::jthread advance([&](std::stop_token stop) {
                while (!stop.stop_requested())
                {
                    time.fetch_add(1000);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            });
            InputFrame pads;
            for (uint64_t frame = 0;; ++frame)
                if (!first.Exchange(frame, Pad(frame, 0), pads))
                    break;
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
    std::string endpointHost;
    uint16_t port = 0;
    for (const auto text : { "127.0.0.1:47640", "[::1]:47640", "localhost:47640" })
        Check(ParseEndpoint(text, endpointHost, port) && port == 47640, "IPv4, bracketed IPv6 and DNS endpoints");
    for (const auto text : { "", ":123", "::1:123", "[::1]", "host:0", "host:65536", "host:+2", "host:12x" })
        Check(!ParseEndpoint(text, endpointHost, port), "invalid endpoint refused");
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
    std::puts("session: 600 frames, loss, duplicates, reordering, desync and timeout passed");
}
