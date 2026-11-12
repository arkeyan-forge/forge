// Checks which TLSF blocks count as allocated from their pool as it is now (engines/alchemy/live_blocks.h): a block
// freed once, a free of a block from before the pool was made again (SuperChargers' race start), the range edges.
// No game data. usage: ArkeyanLiveBlocksTest
#include "live_blocks.h"
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
}

int main()
{
    constexpr uint64_t End = 0x10000; // 64 KB of guest addresses: 16384 granules, 256 words
    std::vector<std::atomic<uint64_t>> bits(End / 4 / 64);
    alchemy::LiveBlocks blocks(bits.data(), End);

    // A pool's block: allocated, freed once; a second free (a double free) is refused.
    blocks.PoolMade(0x1000, 0x4000);
    blocks.Allocated(0x1010);
    CHECK(blocks.Freed(0x1010));
    CHECK(!blocks.Freed(0x1010));

    // The race start: a block of the menus' pool, the pool made again over the same memory, then its free comes.
    blocks.Allocated(0x2134);
    blocks.Allocated(0x2148);
    blocks.PoolMade(0x1000, 0x4000);
    CHECK(!blocks.Freed(0x2134));
    CHECK(!blocks.Freed(0x2148));
    // The new pool's own blocks, even at the same addresses, are freed normally.
    blocks.Allocated(0x2134);
    CHECK(blocks.Freed(0x2134));

    // Making a pool clears only its own range, at granule precision across word edges.
    blocks.Allocated(0x0FFC); // just before
    blocks.Allocated(0x5000); // just after (0x1000 + 0x4000)
    blocks.Allocated(0x10FC); // first word's edge inside
    blocks.Allocated(0x4FFC); // last granule inside
    blocks.PoolMade(0x1000, 0x4000);
    CHECK(!blocks.Freed(0x10FC));
    CHECK(!blocks.Freed(0x4FFC));
    CHECK(blocks.Freed(0x0FFC));
    CHECK(blocks.Freed(0x5000));
    // An unaligned pool (the game's pools start at control + 0xC74): neighbours in the same word stay.
    blocks.Allocated(0x3004);
    blocks.Allocated(0x3008);
    blocks.Allocated(0x300C);
    blocks.PoolMade(0x3008, 4);
    CHECK(blocks.Freed(0x3004));
    CHECK(!blocks.Freed(0x3008));
    CHECK(blocks.Freed(0x300C));

    // Null and addresses past the covered range pass through (true) and are never recorded.
    CHECK(blocks.Freed(0));
    blocks.Allocated(0x20000);
    CHECK(blocks.Freed(0x20000));
    CHECK(blocks.Freed(0x20000));
    // A pool that runs past the covered range is clipped.
    blocks.Allocated(0xFFFC);
    blocks.PoolMade(0xF000, 0x8000);
    CHECK(!blocks.Freed(0xFFFC));

    if (g_failures == 0)
        printf("ArkeyanLiveBlocksTest: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
