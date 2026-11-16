#pragma once

// Where the present pass draws the game's frame in the output (gpu::Present). Letterbox: the whole image, its aspect
// kept, black bars on what is left. Fill ([video] aspect=fill, docs/runtime/gpu/ultrawide.md): on an output narrower
// than the image, the image is scaled up until it covers the output's height and its sides go past the output's edges
// (the full-output scissor clips them). The cut stops at a 16:10 screen's (about 5 % of a 16:9 frame's width on each
// side, inside the margin the games keep their HUD out of); a narrower output gets that crop with bars above and
// below. An output at least as wide as the image (the ultrawide path, a 16:9 window) keeps the letterbox.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gpu::presentrect
{
    struct Rect
    {
        int32_t x, y; // the top left corner in the output; negative when the image goes past its left or top edge
        uint32_t width, height;
    };

    // The narrowest output a filled image still covers in height.
    constexpr float MinFillAspect = 16.0f / 10.0f;

    // Sizes rounded to whole pixels, the image centred, its offsets rounded down.
    inline Rect Fit(uint32_t imageWidth, uint32_t imageHeight, uint32_t outputWidth, uint32_t outputHeight, bool fill)
    {
        if (imageWidth == 0 || imageHeight == 0 || outputWidth == 0 || outputHeight == 0)
            return { 0, 0, outputWidth, outputHeight };
        const float letterbox = std::min(float(outputWidth) / imageWidth, float(outputHeight) / imageHeight);
        const float imageAspect = float(imageWidth) / imageHeight;
        const float outputAspect = float(outputWidth) / outputHeight;
        const float filled = float(outputWidth) / imageWidth * imageAspect / std::max(outputAspect, MinFillAspect);
        // 1 %: the ultrawide frame's width, rounded to whole pixels, can be a hair wider than the window.
        if (!fill || outputAspect * 1.01f >= imageAspect || filled <= letterbox)
        {
            const uint32_t width = std::clamp(uint32_t(std::lround(imageWidth * letterbox)), 1u, outputWidth);
            const uint32_t height = std::clamp(uint32_t(std::lround(imageHeight * letterbox)), 1u, outputHeight);
            return { int32_t((outputWidth - width) / 2), int32_t((outputHeight - height) / 2), width, height };
        }
        const uint32_t width = std::max(uint32_t(std::lround(imageWidth * filled)), 1u);
        const uint32_t height = std::clamp(uint32_t(std::lround(imageHeight * filled)), 1u, outputHeight);
        const auto offset = [](uint32_t output, uint32_t size) {
            return int32_t(std::floor((double(output) - double(size)) / 2.0));
        };
        return { offset(outputWidth, width), offset(outputHeight, height), width, height };
    }

    // With fill, the part of the 16:9 frame's width the output shows (1 when nothing is cut): the HUD is drawn that much
    // smaller, towards the frame's centre, so that none of it falls past the output's edges (d3d/render.cpp).
    inline float HudInset(uint32_t outputWidth, uint32_t outputHeight)
    {
        constexpr uint32_t FrameWidth = 1280, FrameHeight = 720;
        const Rect rect = Fit(FrameWidth, FrameHeight, outputWidth, outputHeight, true);
        if (rect.width <= outputWidth)
            return 1.0f;
        return float(outputWidth) / float(rect.width);
    }
}
