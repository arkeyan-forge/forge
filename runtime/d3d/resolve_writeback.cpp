#include "d3d/resolve_writeback.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

void d3d::PackResolvedColour(const uint32_t* rgba, uint32_t hostWidth, uint32_t hostHeight, float scale,
    uint32_t offsetX, const xenos::TextureFetch& fetch, const xenos::TextureLevel& level, bool swapRB, uint8_t* guest)
{
    // The host pixels [Start(i), Start(i + 1)) under guest pixel i, at least one.
    const auto start = [scale](uint32_t i) { return uint32_t(std::lround(float(i) * scale)); };
    for (uint32_t y = 0; y < level.height; y++)
        for (uint32_t x = 0; x < level.width; x++)
        {
            uint32_t sum[4] = {}, count = 0;
            const uint32_t top = start(y), left = start(x);
            const uint32_t rows = std::max(start(y + 1), top + 1) - top,
                           columns = std::max(start(x + 1), left + 1) - left;
            for (uint32_t dy = 0; dy < rows; dy++)
                for (uint32_t dx = 0; dx < columns; dx++)
                {
                    uint32_t hostX = offsetX + left + dx, hostY = top + dy;
                    if (hostX >= hostWidth || hostY >= hostHeight)
                        continue;
                    uint32_t pixel = rgba[size_t(hostY) * hostWidth + hostX];
                    for (uint32_t c = 0; c < 4; c++)
                        sum[c] += pixel >> (c * 8) & 0xFF;
                    count++;
                }
            if (count == 0)
                continue; // outside the read-back: the guest memory stays as it was
            uint8_t texel[4];
            for (uint32_t c = 0; c < 4; c++)
                texel[c] = uint8_t((sum[c] + count / 2) / count);
            if (swapRB)
                std::swap(texel[0], texel[2]);
            // The upload path reverses these steps (untile, endian swap, X in the first byte): sampling gives the target.
            xenos::SwapEndian(texel, 4, fetch.Endian());
            uint32_t offset = fetch.Tiled() ? xenos::Tiled2D(level.x + x, level.y + y, level.pitchBlocks, 2)
                                            : ((level.y + y) * level.pitchBlocks + level.x + x) * 4;
            memcpy(guest + offset, texel, 4);
        }
}
