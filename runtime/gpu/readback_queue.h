#pragma once

// Read-backs kept by the request that asked for them (gpu::RequestReadback numbers each one). The usual path takes the
// latest finished one; online play (docs/runtime/host/netplay.md, step 0d) takes one fixed request back and waits for
// it, so the guest reads the same frame's depth on both computers whatever the GPU's speed.
#include <chrono>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace gpu
{
    class ReadbackQueue
    {
    public:
        static constexpr size_t Kept = 8;

        // The GPU thread: `request` finished with `data`.
        void Finish(uint64_t request, std::vector<uint32_t> data) { Put(request, std::move(data)); }
        // The GPU thread could not record `request`: its taker gets nothing instead of waiting.
        void Drop(uint64_t request) { Put(request, std::nullopt); }

        // Waits up to `timeout` for `request`; false when it was dropped, forgotten or never came.
        bool Take(uint64_t request, std::vector<uint32_t>& data, std::chrono::milliseconds timeout)
        {
            std::unique_lock lock(m_mutex);
            if (!m_changed.wait_for(lock, timeout, [&] { return m_ready.count(request) || Forgotten(request); }))
                return false;
            const auto found = m_ready.find(request);
            if (found == m_ready.end())
                return false;
            std::optional<std::vector<uint32_t>> taken = std::move(found->second);
            m_ready.erase(found);
            if (!taken)
                return false;
            data.swap(*taken);
            return true;
        }

        size_t Size()
        {
            std::lock_guard lock(m_mutex);
            return m_ready.size();
        }

    private:
        void Put(uint64_t request, std::optional<std::vector<uint32_t>> data)
        {
            {
                std::lock_guard lock(m_mutex);
                m_ready[request] = std::move(data);
                m_newest = std::max(m_newest, request);
                while (m_ready.size() > Kept)
                    m_ready.erase(m_ready.begin());
            }
            m_changed.notify_all();
        }
        // Older than everything kept: it finished and was pushed out, it will not come back.
        bool Forgotten(uint64_t request) const { return m_newest >= Kept && request + Kept <= m_newest; }

        std::mutex m_mutex;
        std::condition_variable m_changed;
        std::map<uint64_t, std::optional<std::vector<uint32_t>>> m_ready;
        uint64_t m_newest = 0;
    };
}
