// Guest index data in host order: 16-bit indices swapped 8-in-16, 32-bit ones (D3DFMT_INDEX32: a mesh of more than
// 65536 vertices) 8-in-32; also their smallest and largest index (gpu::CacheIndices).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace gpu
{
    struct IndexRange
    {
        uint32_t minIndex = 0, maxIndex = 0;
    };

    // `bytes` past the last whole index are copied as they are.
    inline IndexRange ConvertIndices(const uint8_t* source, uint8_t* destination, uint32_t bytes, bool indices32)
    {
        const uint32_t size = indices32 ? 4 : 2;
        uint32_t minIndex = UINT32_MAX, maxIndex = 0;
        uint32_t i = 0;
        for (; i + size <= bytes; i += size)
        {
            uint32_t value = 0;
            if (indices32)
                value = uint32_t(source[i]) << 24 | uint32_t(source[i + 1]) << 16 | uint32_t(source[i + 2]) << 8 |
                        source[i + 3];
            else
                value = uint32_t(source[i]) << 8 | source[i + 1];
            if (indices32)
                memcpy(destination + i, &value, 4);
            else
            {
                const uint16_t half = uint16_t(value);
                memcpy(destination + i, &half, 2);
            }
            minIndex = std::min(minIndex, value);
            maxIndex = std::max(maxIndex, value);
        }
        memcpy(destination + i, source + i, bytes - i);
        return { std::min(minIndex, maxIndex), maxIndex };
    }
}
