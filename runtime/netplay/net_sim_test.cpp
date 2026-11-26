#include "netplay/net_sim.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <deque>

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

    netplay::NetSim Parse(std::string_view text)
    {
        netplay::NetSim sim;
        std::string error;
        Check(netplay::ParseNetSim(text, sim, error) && error.empty(), "valid simulation spec");
        return sim;
    }

    void Parsing()
    {
        auto sim = Parse("delay=150,jitter=20,loss=2,dup=1,cut=30000-35000,cut=60000-120000,seed=7");
        Check(sim.delayMs == 150 && sim.jitterMs == 20 && sim.lossPercent == 2 && sim.duplicatePercent == 1 &&
                  sim.cutsMs == std::vector<std::pair<uint64_t, uint64_t>>{ { 30000, 35000 }, { 60000, 120000 } } &&
                  sim.seed == 7,
            "all simulation fields parsed");
        sim = Parse("");
        Check(!sim.delayMs && !sim.jitterMs && !sim.lossPercent && !sim.duplicatePercent && sim.cutsMs.empty() &&
                  !sim.seed,
            "empty subset has zero defaults");
        for (const char* subset : { "delay=1", "jitter=2", "loss=2.5", "dup=0.25", "cut=0-1", "seed=7" })
            Parse(subset);
        sim = Parse("cut=20-30,cut=0-10,cut=10-20,delay=4294967295,jitter=4294967295,"
                    "seed=18446744073709551615,loss=100,dup=100");
        Check(sim.cutsMs.front().first == 0 && sim.cutsMs.back().second == 30 && sim.seed == UINT64_MAX,
            "adjacent cuts sorted and integer maxima accepted");
        for (const char* invalid : { "unknown=1", "delay=-1", "jitter=-1", "loss=-1", "dup=-1", "seed=-1",
                 "loss=100.01", "dup=101", "loss=nan", "dup=inf", "loss=1e999", "delay=4294967296", "jitter=4294967296",
                 "seed=18446744073709551616", "delay=1.5", "jitter=1ms", "seed=+1", "delay=", "loss=", "seed=", "=1",
                 "delay", ",delay=1", "delay=1,", "delay=1,,loss=2", "delay=1=2", " delay=1", "delay= 1", "delay=1 ",
                 "loss=2x", "delay=1,delay=2", "jitter=1,jitter=2", "loss=1,loss=2", "dup=1,dup=2", "seed=1,seed=2",
                 "cut=20-10", "cut=1-1", "cut=-1-10", "cut=0--1", "cut=1", "cut=1-", "cut=-10", "cut=1-2-3", "cut=1-2x",
                 "cut=0-18446744073709551616", "cut=0-10,cut=9-20", "cut=5-8,cut=0-10", "cut=0-10,cut=0-10" })
        {
            netplay::NetSim unchanged;
            unchanged.seed = 123;
            std::string error;
            Check(!netplay::ParseNetSim(invalid, unchanged, error) && !error.empty(), invalid);
            Check(unchanged.seed == 123, "failed parsing leaves the output intact");
        }
    }

    struct Sent
    {
        uint64_t time;
        std::vector<uint8_t> bytes;
        bool operator==(const Sent&) const = default;
    };

    class Wire final : public netplay::Transport
    {
    public:
        explicit Wire(uint64_t& now) : time(now) {}
        bool Send(std::span<const uint8_t> bytes) override
        {
            sent.push_back({ time, { bytes.begin(), bytes.end() } });
            return true;
        }
        size_t Receive(std::span<uint8_t> bytes) override
        {
            if (incoming.empty())
                return NoDatagram;
            const auto packet = std::move(incoming.front());
            incoming.pop_front();
            std::copy_n(packet.begin(), std::min(bytes.size(), packet.size()), bytes.begin());
            return packet.size();
        }
        bool AcceptPeer() override
        {
            ++accepted;
            return false;
        }
        bool Healthy() const override { return healthy; }
        uint16_t Port() const override { return 4321; }

        uint64_t& time;
        bool healthy = true;
        unsigned accepted = 0;
        std::vector<Sent> sent;
        std::deque<std::vector<uint8_t>> incoming;
    };

    struct Link
    {
        explicit Link(std::string_view spec, uint64_t start = 1000) : now(start)
        {
            auto inner = std::make_unique<Wire>(now);
            wire = inner.get();
            transport = netplay::SimulateNetwork(std::move(inner), Parse(spec), [&] { return now; });
        }

        void Send(uint32_t id)
        {
            const std::array<uint8_t, 4> bytes{ uint8_t(id), uint8_t(id >> 8), uint8_t(id >> 16), uint8_t(id >> 24) };
            Check(transport->Send(bytes), "simulated send accepted");
        }
        size_t Receive() { return transport->Receive(buffer); }

        uint64_t now;
        Wire* wire;
        std::unique_ptr<netplay::Transport> transport;
        std::array<uint8_t, 64> buffer{};
    };

    uint32_t Id(const Sent& sent)
    {
        Check(sent.bytes.size() == 4, "datagram payload length preserved");
        return uint32_t(sent.bytes[0]) | (uint32_t(sent.bytes[1]) << 8) | (uint32_t(sent.bytes[2]) << 16) |
               (uint32_t(sent.bytes[3]) << 24);
    }

    void DelayAndForwarding()
    {
        Link link("delay=150");
        Check(link.transport->Port() == 4321 && link.transport->Healthy(), "port and health forwarded");
        link.transport->AcceptPeer();
        Check(link.wire->accepted == 1, "peer acceptance forwarded");
        link.wire->healthy = false;
        Check(!link.transport->Healthy(), "inner failure forwarded");
        link.wire->healthy = true;
        link.Send(42);
        link.Send(43);
        Check(link.wire->sent.empty(), "send queues a copy without sending immediately");
        link.wire->incoming.push_back({ 9, 8, 7 });
        Check(link.Receive() == 3 && link.buffer[0] == 9 && link.buffer[2] == 7,
            "incoming datagrams have no added delay");
        link.now = 1149;
        Check(link.Receive() == netplay::Transport::NoDatagram && link.wire->sent.empty(), "delay holds until due");
        link.now = 1150;
        link.wire->incoming.push_back({});
        Check(link.Receive() == 0 && link.wire->sent.size() == 2, "due sends flush before a zero-byte receive");
        Check(Id(link.wire->sent[0]) == 42 && Id(link.wire->sent[1]) == 43,
            "equal deadlines retain order and copied payloads");
        link.Receive();
        Check(link.wire->sent.size() == 2, "queued datagrams sent only once");

        Link overflow("delay=150", UINT64_MAX - 2);
        overflow.Send(1);
        overflow.Receive();
        Check(overflow.wire->sent.empty(), "deadline does not wrap at the clock limit");
        overflow.now = UINT64_MAX;
        overflow.Receive();
        Check(overflow.wire->sent.size() == 1, "deadline saturates at the clock limit");
    }

    void Jitter()
    {
        Link link("delay=100,jitter=20,seed=7");
        for (uint32_t id = 0; id < 512; ++id)
            link.Send(id);
        for (; link.now <= 1120; ++link.now)
            link.Receive();
        Check(link.wire->sent.size() == 512, "jitter delivers every datagram");
        bool reordered = false;
        std::array<unsigned, 41> histogram{};
        std::array<bool, 512> seen{};
        uint32_t previous = 0;
        for (const auto& sent : link.wire->sent)
        {
            Check(sent.time >= 1080 && sent.time <= 1120, "uniform jitter stays within bounds");
            ++histogram[size_t(sent.time - 1080)];
            const auto id = Id(sent);
            Check(id < seen.size() && !seen[id], "jitter preserves each payload exactly once");
            seen[id] = true;
            reordered |= id < previous;
            previous = id;
        }
        Check(reordered, "jitter reorders datagrams by due time");
        Check(std::all_of(histogram.begin(), histogram.end(), [](unsigned count) { return count > 0; }),
            "jitter samples the full interval including both endpoints");

        Link clamped("jitter=20,seed=7");
        for (uint32_t id = 0; id < 128; ++id)
            clamped.Send(id);
        clamped.Receive();
        Check(!clamped.wire->sent.empty(), "negative jitter clamps to immediate delivery");
        for (; clamped.now <= 1020; ++clamped.now)
            clamped.Receive();
        Check(clamped.wire->sent.size() == 128, "clamped jitter has no unsigned underflow");
    }

    void LossAndDuplicates()
    {
        for (const char* loss : { "loss=0", "loss=100", "loss=10,seed=7" })
        {
            Link link(loss);
            for (uint32_t id = 0; id < 10000; ++id)
                link.Send(id);
            link.Receive();
            const auto delivered = link.wire->sent.size();
            if (std::string_view(loss) == "loss=0")
                Check(delivered == 10000, "zero loss delivers every datagram");
            else if (std::string_view(loss) == "loss=100")
                Check(delivered == 0, "complete loss sends no datagrams");
            else
                Check(delivered >= 8800 && delivered <= 9200, "ten percent loss is within eight to twelve percent");
        }
        Link duplicate("dup=100,delay=10");
        duplicate.Send(123);
        duplicate.Receive();
        Check(duplicate.wire->sent.empty(), "duplicates respect delay");
        duplicate.now += 10;
        duplicate.Receive();
        Check(duplicate.wire->sent.size() == 2 && Id(duplicate.wire->sent[0]) == 123 &&
                  Id(duplicate.wire->sent[1]) == 123,
            "complete duplication sends two identical datagrams");
        Link lost("loss=100,dup=100");
        lost.Send(1);
        lost.Receive();
        Check(lost.wire->sent.empty(), "loss precedes duplication");
    }

    void Cuts()
    {
        Link link("cut=10-20,cut=30-40");
        for (uint64_t elapsed : { 0, 9, 10, 19, 20, 29, 30, 39, 40 })
        {
            link.now = 1000 + elapsed;
            const bool cut = (elapsed >= 10 && elapsed < 20) || (elapsed >= 30 && elapsed < 40);
            const auto previous = link.wire->sent.size();
            link.Send(uint32_t(elapsed));
            link.wire->incoming.push_back({ 1 });
            link.wire->incoming.push_back({ 2 });
            Check(link.Receive() == (cut ? netplay::Transport::NoDatagram : 1),
                "cuts drop inbound datagrams from their start until their exact end");
            Check(link.wire->sent.size() == previous + (cut ? 0 : 1), "cuts drop outbound datagrams");
            if (!cut)
                Check(link.Receive() == 1 && link.buffer[0] == 2, "non-cut incoming order preserved");
            Check(link.Receive() == netplay::Transport::NoDatagram, "cut traffic drained without later delivery");
        }
        Link queued("delay=10,cut=10-20");
        queued.Send(1);
        queued.now = 1010;
        queued.Receive();
        queued.now = 1020;
        queued.Receive();
        Check(queued.wire->sent.empty(), "a queued datagram due during a cut is discarded");
        queued.Send(2);
        queued.now = 1030;
        queued.Receive();
        Check(queued.wire->sent.size() == 1 && Id(queued.wire->sent[0]) == 2,
            "sending resumes after a cut without replaying dropped traffic");
    }

    std::vector<Sent> Outcome(uint64_t seed)
    {
        Link link("delay=20,jitter=10,loss=10,dup=25,cut=40-60,cut=100-110,seed=" + std::to_string(seed));
        for (uint32_t tick = 0; tick < 200; ++tick)
        {
            link.now = 1000 + tick;
            link.Send(tick);
            link.Receive();
        }
        for (; link.now <= 1230; ++link.now)
            link.Receive();
        return std::move(link.wire->sent);
    }
}

int main()
{
    Parsing();
    DelayAndForwarding();
    Jitter();
    LossAndDuplicates();
    Cuts();
    const auto first = Outcome(7);
    Check(!first.empty() && first == Outcome(7), "same seed and fake clock reproduce payloads, drops and send times");
    Check(first != Outcome(8), "different seeds change the outcome");
    puts("network simulation: parsing, delay, jitter, loss, duplicates, cuts and seeded replay passed");
}
