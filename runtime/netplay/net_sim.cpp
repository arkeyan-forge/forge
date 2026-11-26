#include "netplay/net_sim.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <queue>
#include <random>

namespace netplay
{
    namespace
    {
        template <typename T>
        bool Number(std::string_view text, T& value)
        {
            if (text.empty() || text.front() == '-')
                return false;
            const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
            return error == std::errc{} && end == text.data() + text.size();
        }

        class Simulated final : public Transport
        {
        public:
            Simulated(std::unique_ptr<Transport> inner, NetSim sim, std::function<uint64_t()> nowMs)
                : m_inner(std::move(inner)),
                  m_sim(std::move(sim)),
                  m_now(std::move(nowMs)),
                  m_random(m_sim.seed),
                  m_opened(m_now())
            {}

            bool Send(std::span<const uint8_t> bytes) override
            {
                const uint64_t now = m_now();
                if (Cut(now) || Chance(m_sim.lossPercent))
                    return true;
                Queue(bytes, now);
                if (Chance(m_sim.duplicatePercent))
                    Queue(bytes, now);
                return true;
            }

            size_t Receive(std::span<uint8_t> bytes) override
            {
                const uint64_t now = m_now();
                const bool cut = Cut(now);
                while (!m_pending.empty() && m_pending.top().due <= now)
                {
                    if (!cut)
                        m_inner->Send(m_pending.top().bytes);
                    m_pending.pop();
                }
                size_t received;
                do
                    received = m_inner->Receive(bytes);
                while (cut && received != NoDatagram);
                return received;
            }

            bool AcceptPeer() override { return m_inner->AcceptPeer(); }
            bool Healthy() const override { return m_inner->Healthy(); }
            uint16_t Port() const override { return m_inner->Port(); }

        private:
            struct Pending
            {
                uint64_t due, order;
                std::vector<uint8_t> bytes;
                bool operator<(const Pending& other) const
                {
                    return due > other.due || (due == other.due && order > other.order);
                }
            };

            bool Cut(uint64_t now) const
            {
                const uint64_t elapsed = now - m_opened;
                return std::any_of(m_sim.cutsMs.begin(), m_sim.cutsMs.end(),
                    [&](const auto& cut) { return elapsed >= cut.first && elapsed < cut.second; });
            }

            bool Chance(double percent)
            {
                return percent > 0 &&
                       (percent >= 100 || std::uniform_real_distribution<double>(0, 100)(m_random) < percent);
            }

            void Queue(std::span<const uint8_t> bytes, uint64_t now)
            {
                const int64_t jitter =
                    std::uniform_int_distribution<int64_t>(-int64_t(m_sim.jitterMs), int64_t(m_sim.jitterMs))(m_random);
                const uint64_t delay = uint64_t(std::max(int64_t(0), int64_t(m_sim.delayMs) + jitter));
                const uint64_t due = now + std::min(delay, UINT64_MAX - now);
                m_pending.push({ due, m_order++, { bytes.begin(), bytes.end() } });
            }

            std::unique_ptr<Transport> m_inner;
            NetSim m_sim;
            std::function<uint64_t()> m_now;
            std::mt19937_64 m_random;
            uint64_t m_opened, m_order = 0;
            std::priority_queue<Pending> m_pending;
        };
    }

    bool ParseNetSim(std::string_view text, NetSim& sim, std::string& error)
    {
        NetSim parsed;
        uint32_t seen = 0;
        error.clear();
        while (!text.empty())
        {
            const size_t comma = text.find(',');
            const auto item = text.substr(0, comma);
            const size_t equals = item.find('=');
            if (equals == std::string_view::npos)
            {
                error = "expected key=value";
                return false;
            }
            const auto key = item.substr(0, equals), value = item.substr(equals + 1);
            uint32_t field = 0;
            bool valid = false;
            if (key == "delay" || key == "jitter")
            {
                field = key == "delay" ? 1 : 2;
                valid = Number(value, key == "delay" ? parsed.delayMs : parsed.jitterMs);
            }
            else if (key == "loss" || key == "dup")
            {
                field = key == "loss" ? 4 : 8;
                double& percent = key == "loss" ? parsed.lossPercent : parsed.duplicatePercent;
                valid = Number(value, percent) && std::isfinite(percent) && percent >= 0 && percent <= 100;
            }
            else if (key == "seed")
            {
                field = 16;
                valid = Number(value, parsed.seed);
            }
            else if (key == "cut")
            {
                const size_t dash = value.find('-');
                uint64_t start = 0, end = 0;
                valid = dash != std::string_view::npos && Number(value.substr(0, dash), start) &&
                        Number(value.substr(dash + 1), end) && start < end;
                if (valid)
                    parsed.cutsMs.emplace_back(start, end);
            }
            else
            {
                error = "unknown network simulation key: " + std::string(key);
                return false;
            }
            if (!valid || (seen & field))
            {
                error = "invalid or repeated network simulation value: " + std::string(key);
                return false;
            }
            seen |= field;
            if (comma == std::string_view::npos)
                break;
            text.remove_prefix(comma + 1);
            if (text.empty())
            {
                error = "expected key=value after comma";
                return false;
            }
        }
        std::sort(parsed.cutsMs.begin(), parsed.cutsMs.end());
        for (size_t i = 1; i < parsed.cutsMs.size(); ++i)
            if (parsed.cutsMs[i].first < parsed.cutsMs[i - 1].second)
            {
                error = "network simulation cuts overlap";
                return false;
            }
        sim = std::move(parsed);
        return true;
    }

    std::unique_ptr<Transport> SimulateNetwork(
        std::unique_ptr<Transport> inner, NetSim sim, std::function<uint64_t()> nowMs)
    {
        return std::make_unique<Simulated>(std::move(inner), std::move(sim), std::move(nowMs));
    }
}
