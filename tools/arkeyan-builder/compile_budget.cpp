// See compile_budget.h.
#include "compile_budget.h"
#include <algorithm>

namespace compile_budget
{
    unsigned Units(uint64_t memoryBytes)
    {
        return unsigned(std::max<uint64_t>(1, memoryBytes > Reserve ? (memoryBytes - Reserve) / Unit : 0));
    }

    unsigned Jobs(unsigned threads, uint64_t memoryBytes)
    {
        return std::max(1u, std::min(threads, Units(memoryBytes)));
    }

    unsigned FileUnits(uintmax_t literalBytes, bool shaderCache, unsigned total)
    {
        if (!shaderCache)
            return 1;
        uintmax_t units = (literalBytes * 24 + Unit - 1) / Unit;
        return unsigned(std::clamp<uintmax_t>(units, 1, std::max(1u, total)));
    }

    Pool::Pool(unsigned units) : m_free(std::max(1u, units)) {}

    void Pool::Take(size_t index, unsigned units)
    {
        std::unique_lock lock(m_mutex);
        m_changed.wait(lock, [&]() { return m_next == index && m_free >= units; });
        m_free -= units;
        m_next++;
        m_changed.notify_all();
    }

    void Pool::Give(unsigned units)
    {
        {
            std::lock_guard lock(m_mutex);
            m_free += units;
        }
        m_changed.notify_all();
    }
}
