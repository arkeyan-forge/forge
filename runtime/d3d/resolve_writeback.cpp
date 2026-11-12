#include "d3d/resolve_writeback.h"
#include <cstring>
#include <utility>

void d3d::PackResolvedColour(const uint32_t* rgba, uint32_t hostWidth, uint32_t hostHeight, uint32_t scale,
    uint32_t offsetX, const xenos::TextureFetch& fetch, const xenos::TextureLevel& level, bool swapRB, uint8_t* guest)
{
    for (uint32_t y = 0; y < level.height; y++)
        for (uint32_t x = 0; x < level.width; x++)
        {
            uint32_t sum[4] = {}, count = 0;
            for (uint32_t dy = 0; dy < scale; dy++)
                for (uint32_t dx = 0; dx < scale; dx++)
                {
                    uint32_t hostX = offsetX + x * scale + dx, hostY = y * scale + dy;
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
