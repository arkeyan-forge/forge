// Checks the colour write-back's packing (d3d/resolve_writeback.h): read back the way the renderer uploads a guest
// texture (untile, endian swap, X in the first byte), each texel is its host block's average, red and blue swapped as
// the resolve did, for tiled and linear storage, at scale 1 and 2, with a wide frame's offset. usage:
// ArkeyanResolveWritebackTest
#include "d3d/resolve_writeback.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool ok, const char* what, uint32_t x, uint32_t y)
    {
        if (!ok && g_failures++ < 10)
            fprintf(stderr, "FAIL %s at (%u, %u)\n", what, x, y);
    }

    // A k_8_8_8_8 2D texture of width x height at physical 0x1000, tiled or linear, with this endian.
    d3d::xenos::TextureFetch Fetch(uint32_t width, uint32_t height, bool tiled, uint32_t endian)
    {
        d3d::xenos::TextureFetch fetch{};
        uint32_t pitch = (width + 31) / 32;
        fetch.dword[0] = 2 | pitch << 22 | (tiled ? 1u << 31 : 0);
        fetch.dword[1] = 6 | endian << 6 | 0x1000;
        fetch.dword[2] = (width - 1) | (height - 1) << 13;
        fetch.dword[5] = 1 << 9;
        return fetch;
    }

    uint32_t HostPixel(uint32_t x, uint32_t y) { return (x * 7 + y) & 0xFF | (y * 3) % 256 << 8 | (x ^ y) << 16 | 0xC0000000u; }

    void Run(uint32_t width, uint32_t height, bool tiled, uint32_t endian, uint32_t scale, uint32_t offsetX, bool swapRB)
    {
        const d3d::xenos::TextureFetch fetch = Fetch(width, height, tiled, endian);
        const d3d::xenos::TextureLevel level =
            d3d::xenos::TextureLevelLayout(fetch, d3d::xenos::TextureFormat(fetch.Format()), 0);
        const uint32_t hostWidth = width * scale + 2 * offsetX, hostHeight = height * scale;
        std::vector<uint32_t> host(size_t(hostWidth) * hostHeight);
        for (uint32_t y = 0; y < hostHeight; y++)
            for (uint32_t x = 0; x < hostWidth; x++)
                host[size_t(y) * hostWidth + x] = HostPixel(x, y);
        std::vector<uint8_t> guest(level.sliceBytes, 0xAB);
        d3d::PackResolvedColour(host.data(), hostWidth, hostHeight, scale, offsetX, fetch, level, swapRB, guest.data());
        for (uint32_t y = 0; y < height; y++)
            for (uint32_t x = 0; x < width; x++)
            {
                uint32_t offset = tiled ? d3d::xenos::Tiled2D(x, y, level.pitchBlocks, 2) : (y * level.pitchBlocks + x) * 4;
                uint8_t texel[4];
                memcpy(texel, &guest[offset], 4);
                d3d::xenos::SwapEndian(texel, 4, endian);
                for (uint32_t c = 0; c < 4; c++)
                {
                    uint32_t sum = 0;
                    for (uint32_t dy = 0; dy < scale; dy++)
                        for (uint32_t dx = 0; dx < scale; dx++)
                            sum += HostPixel(offsetX + x * scale + dx, y * scale + dy) >> (c * 8) & 0xFF;
                    uint32_t expected = (sum + scale * scale / 2) / (scale * scale);
                    uint32_t at = swapRB && c != 1 && c != 3 ? 2 - c : c;
                    Check(texel[at] == expected, swapRB ? "component (red and blue swapped)" : "component", x, y);
                }
            }
    }
}

int main()
{
    Run(64, 32, true, 2, 1, 0, false);  // tiled, 8-in-32, one host pixel per texel
    Run(64, 32, true, 2, 2, 0, true);   // scale 2, red and blue swapped (an ARGB destination)
    Run(160, 90, true, 2, 2, 16, true); // a frame copy wider than the guest's
    Run(48, 20, false, 0, 2, 0, false); // linear, no swap: rows of 256 bytes
    Run(40, 8, false, 1, 3, 0, false);  // 8-in-16
    if (g_failures == 0)
        printf("resolve write-back test: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
