#pragma once

#include <cstdint>

namespace d3d
{
    // The colour targets a pixel shader writes (bit i: oC<i>), from its header's outputs dword (+0x1C: bits 0-3 oC0-oC3,
    // bit 4 the depth), which is what XenosRecomp declares as the translated shader's outputs.
    inline uint32_t PixelShaderColorOutputs(uint32_t outputs) { return outputs & 0xF; }

    // Render target `slot`'s write mask: its 4 bits of RB_COLOR_MASK, or none when the pixel shader does not write it
    // (`colorOutputs`: PixelShaderColorOutputs, 0 without a pixel shader). On Xenos such a target keeps its contents; the
    // host APIs leave it undefined: Apple and AMD keep it, NVIDIA writes 0 (Trap Team's title halo was black).
    inline uint8_t TargetWriteMask(uint32_t colorMask, uint32_t colorOutputs, uint32_t slot)
    {
        return ((colorOutputs >> slot) & 1) != 0 ? uint8_t((colorMask >> (slot * 4)) & 0xF) : 0;
    }

    // How a pixel shader's export to a target of Xenos colour format `colorFormat` (RB_COLOR_INFO bits 16-19) is stored:
    // the translated shader's exportClamp mode. Both formats here have half-float host targets, which keep the NaN and
    // the infinities the console never stores (a level once turned the whole picture green, and it stayed so).
    inline uint32_t ExportClamp(uint32_t colorFormat)
    {
        switch (colorFormat)
        {
        case 3:  // k_2_10_10_10_FLOAT: 7e3 colour (0 to 31.875), 2-bit alpha
        case 12: // k_2_10_10_10_FLOAT_AS_16_16_16_16
            return 1;
        case 4: // k_16_16: fixed point, -32 to 32
        case 5: // k_16_16_16_16
            return 2;
        default: return 0; // stored as the host stores it (unorm, IEEE floats)
        }
    }

    // The draw's g_TargetClamps: ExportClamp of target i in bits 2i and 2i+1.
    inline uint32_t TargetClamps(const uint32_t* colorFormats, uint32_t count)
    {
        uint32_t clamps = 0;
        for (uint32_t i = 0; i < count && i < 4; i++)
            clamps |= ExportClamp(colorFormats[i]) << (i * 2);
        return clamps;
    }
}
