#pragma once

// A colour resolve written back to guest memory as the 360 stores it, for a game whose CPU reads what the GPU resolved
// (d3d/render.cpp WriteBackResolvedTexture). Resolves stay on the host GPU, so without it that memory holds whatever was
// there before (zeros): Imaginators' created-character portrait, a crop of a resolved frame copied and shrunk on the
// CPU, came out black (#47, docs/runtime/gpu/render_bugs.md).
#include "d3d/xenos.h"
#include <cstdint>

namespace d3d
{
    // The game's CPU is about to read `texture` (a guest D3D texture): when the GPU resolved into it, its host copy is read
    // back now (the GPU's work so far finished first) and packed into the texture's guest memory. Only a k_8_8_8_8 2D
    // texture; false when nothing was resolved there. A one-off stall: for a copy the game makes once, not every frame.
    bool WriteBackResolvedTexture(uint32_t texture);

    // Packs a host read-back (RGBA8, red in the low byte, `hostWidth` x `hostHeight`) into the guest storage of a
    // k_8_8_8_8 texture whose base level is `level` (TextureLevelLayout): one texel per guest pixel, the average of its
    // `scale` x `scale` host block starting `offsetX` host pixels in (a frame copy wider than the guest's); X to W are
    // the target's red, green, blue, alpha, or blue, green, red, alpha with `swapRB` (the resolve's copy_dest_swap, as
    // sampling reads it back); then the fetch's endian swap and its tiling. `guest`: the level's first byte.
    void PackResolvedColour(const uint32_t* rgba, uint32_t hostWidth, uint32_t hostHeight, uint32_t scale,
        uint32_t offsetX, const xenos::TextureFetch& fetch, const xenos::TextureLevel& level, bool swapRB,
        uint8_t* guest);
}
