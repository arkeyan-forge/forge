#pragma once

#include <cstdint>
#include <iterator>
#include <map>

// In a heap's map of allocations (base -> size): the one containing `address`, else the first one after it; end() if
// none.
inline std::map<uint32_t, uint32_t>::const_iterator AllocationAtOrAfter(
    const std::map<uint32_t, uint32_t>& used, uint32_t address)
{
    auto it = used.upper_bound(address);
    if (it != used.begin())
    {
        const auto previous = std::prev(it);
        if (uint64_t(address) < uint64_t(previous->first) + previous->second)
            return previous;
    }
    return it;
}
