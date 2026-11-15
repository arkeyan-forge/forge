// Checks guest index data in host order, 16-bit and 32-bit, with their range. usage: ArkeyanIndexConvertTest
#include "gpu/index_convert.h"
#include <cstdio>
#include <vector>

namespace
{
    int g_failures = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    uint32_t Host32(const std::vector<uint8_t>& data, size_t at)
    {
        return uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 | uint32_t(data[at + 2]) << 16 |
               uint32_t(data[at + 3]) << 24;
    }
    uint32_t Host16(const std::vector<uint8_t>& data, size_t at)
    {
        return uint32_t(data[at]) | uint32_t(data[at + 1]) << 8;
    }
}

int main()
{
    // 32-bit indices past 65535 (big-endian guest order): 0x00010002, 5, 0x00022C1A (142362).
    const std::vector<uint8_t> guest32 = { 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x02, 0x2C, 0x1A };
    std::vector<uint8_t> host(guest32.size(), 0xCC);
    gpu::IndexRange range = gpu::ConvertIndices(guest32.data(), host.data(), uint32_t(guest32.size()), true);
    CHECK(Host32(host, 0) == 0x00010002u);
    CHECK(Host32(host, 4) == 5u);
    CHECK(Host32(host, 8) == 0x00022C1Au);
    CHECK(range.minIndex == 5u && range.maxIndex == 0x00022C1Au);

    // The same bytes read as 16-bit indices: what the renderer drew before (every other index 0, 1 or 2: the mesh's
    // triangles fanned to its first vertices).
    range = gpu::ConvertIndices(guest32.data(), host.data(), uint32_t(guest32.size()), false);
    CHECK(Host16(host, 0) == 1u && Host16(host, 2) == 2u && Host16(host, 4) == 0u && Host16(host, 6) == 5u);
    CHECK(range.minIndex == 0u && range.maxIndex == 0x2C1Au);

    // 16-bit indices with an odd trailing byte, copied as it is.
    const std::vector<uint8_t> guest16 = { 0x12, 0x34, 0x00, 0x07, 0xAB };
    host.assign(guest16.size(), 0);
    range = gpu::ConvertIndices(guest16.data(), host.data(), uint32_t(guest16.size()), false);
    CHECK(Host16(host, 0) == 0x1234u && Host16(host, 2) == 7u && host[4] == 0xAB);
    CHECK(range.minIndex == 7u && range.maxIndex == 0x1234u);

    // Nothing to convert: an empty range.
    range = gpu::ConvertIndices(guest16.data(), host.data(), 0, true);
    CHECK(range.minIndex == 0u && range.maxIndex == 0u);

    if (g_failures == 0)
        printf("ArkeyanIndexConvertTest: all passed\n");
    return g_failures == 0 ? 0 : 1;
}
