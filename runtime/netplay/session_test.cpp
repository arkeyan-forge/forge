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
        bool chaos = false, silent = false;
    };
    class Wire : public netplay::Transport
    {
    public:
        Wire(Link& link, size_t seat) : m_link(link), m_seat(seat) {}
        bool Send(std::span<const uint8_t> bytes) override
        {
            std::lock_guard lock(m_link.mutex);
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
        config.timeoutMs = 3000;
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
        netplay::Session first(config, std::move(host));
        netplay::Session second(Config(false), std::move(join));
        std::array<std::array<netplay::InputFrame, 600>, 2> results{};
        std::array<uint64_t, 2> stopped{};
        const auto play = [&](netplay::Session& session, size_t seat) {
            Check(session.WaitReady(), "session ready");
            Check(session.Delay() == delay, "host delay wins");
            for (uint64_t frame = 0; frame < 600; ++frame)
            {
                Check(session.Exchange(frame, Pad(frame, seat), results[seat][frame]), "exchange frame");
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
        if (desync)
            Check(stopped[0] == 300 && stopped[1] == 300, "both stop at hash frame 300");
        for (uint64_t frame = 0; frame < frames; ++frame)
        {
            Check(netplay::EncodeFrame(results[0][frame]) == netplay::EncodeFrame(results[1][frame]), "same pad pair");
            netplay::InputFrame expected{};
            if (frame >= delay)
                expected = { Pad(frame - delay, 0), Pad(frame - delay, 1) };
            Check(netplay::EncodeFrame(results[0][frame]) == netplay::EncodeFrame(expected), "pads arrive at N plus D");
        }
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
