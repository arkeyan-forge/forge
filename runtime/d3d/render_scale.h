#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace d3d
{
    // Colour tables keep their guest size: duplicating entries makes bilinear lookups form a staircase.
    // A scaled picture has both sides above 32, so this also works on host sizes.
    constexpr bool IsTableSize(uint32_t width, uint32_t height) { return std::min(width, height) <= 32; }

    constexpr float TargetScale(uint32_t width, uint32_t height, float resolutionScale)
    {
        return IsTableSize(width, height) ? 1.0f : resolutionScale;
    }

    // A guest size at a resolution scale, in host pixels: exact at a whole scale, rounded at a fractional one (the
    // upscaled scene's, d3d/upscale.h).
    inline uint32_t HostSize(uint32_t guest, float scale) { return uint32_t(std::lround(float(guest) * scale)); }

    // A target's scale on each axis: its host size over the guest size HostSize made it from. At a fractional scale the
    // two differ a little, and the guest's edges must land on the target's.
    struct AxisScale
    {
        float x, y;
    };
    inline AxisScale TargetAxisScale(uint32_t width, uint32_t height, float resolutionScale)
    {
        const float scale = TargetScale(width, height, resolutionScale);
        const auto axis = [scale](uint32_t host) {
            const float guest = std::max(std::round(float(host) / scale), 1.0f);
            return float(host) / guest;
        };
        return { axis(width), axis(height) };
    }

    // A guest pixel coordinate on one axis of a target at `scale`, in host pixels.
    inline int32_t ToHost(int32_t guest, float scale) { return int32_t(std::lround(float(guest) * scale)); }

    struct HostKernel
    {
        std::array<float, 8> constants;
        uint32_t scaledTextures;
    };

    // c0-c1 and fetch offsets must both step in host pixels for the edge filter.
    constexpr HostKernel ScaleHostKernel(HostKernel kernel, float resolutionScale)
    {
        if (resolutionScale > 1.0f)
        {
            for (float& value : kernel.constants)
                value /= float(resolutionScale);
            kernel.scaledTextures = 0;
        }
        return kernel;
    }

    struct Rect
    {
        int32_t left, top, right, bottom;
    };

    // A draw's scissor in host pixels: the device's rectangle (guest pixels, D3DDevice_SetScissorRect's as given) only
    // while D3DRS_SCISSORTESTENABLE is on, as the window scissor register takes it; otherwise the whole target. A
    // title may leave any rectangle there with the test off (an empty one, seen), which clips nothing.
    inline Rect DrawScissor(bool testEnabled, Rect guest, AxisScale scale, int32_t width, int32_t height)
    {
        if (!testEnabled)
            return { 0, 0, width, height };
        return { std::clamp(ToHost(guest.left, scale.x), 0, width), std::clamp(ToHost(guest.top, scale.y), 0, height),
            std::clamp(ToHost(guest.right, scale.x), 0, width), std::clamp(ToHost(guest.bottom, scale.y), 0, height) };
    }
}
