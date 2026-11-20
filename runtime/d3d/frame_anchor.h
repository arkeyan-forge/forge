// Where a HUD draw goes in the wide frame (d3d/render.cpp, BuildDraw). The HUD is laid out for 1280x720 and drawn
// centred; a piece the game puts in a corner of that frame (Trap Team's menu vignette darkens the four corners) would end
// at the 16:9 edge with a hard step, so it moves to the same corner of the window instead.
#pragma once

#include <algorithm>
#include <cstdint>

namespace d3d
{
    enum class FramePlace
    {
        Centre, // the HUD's place: the 16:9 frame in the middle of the wide one
        Left, // flush with the window's left edge
        Right, // flush with the window's right edge
        Whole, // over the whole frame (a fade, a flash, a full-screen page): over the whole wide frame too (WholeScaleX),
        // and never drawn smaller where aspect=fill cuts the frame (d3d/render.cpp)
    };

    // `positions`: `count` vertices of `components` floats (x, y[, z]; the rest 0). `rows`: the vertex shader's position
    // matrix, four rows of four (clip = x rows[0] + y rows[1] + z rows[2] + rows[3]). A draw that reaches one corner of
    // the 16:9 frame (within half a guest pixel, or past it) and is partly on screen goes to that side; one that reaches
    // all four edges is Whole; anything else, a band across the whole width included, stays centred. `reach`, for Whole:
    // how far it goes past the nearer side, in clip units (1: the frame's own width; a turning sheet bigger than the
    // screen goes further).
    inline FramePlace PlaceFrameDraw(
        const float* positions, uint32_t count, uint32_t components, const float* rows, float* reach = nullptr)
    {
        if (count < 3 || components < 2)
            return FramePlace::Centre;
        float minX = 1e30f, maxX = -1e30f, minY = 1e30f, maxY = -1e30f;
        for (uint32_t v = 0; v < count; v++)
        {
            const float* p = positions + size_t(v) * components;
            const float z = components >= 3 ? p[2] : 0.0f;
            float clip[4];
            for (uint32_t i = 0; i < 4; i++)
                clip[i] = p[0] * rows[i] + p[1] * rows[4 + i] + z * rows[8 + i] + rows[12 + i];
            if (!(clip[3] > 0.0f))
                return FramePlace::Centre;
            const float x = clip[0] / clip[3], y = clip[1] / clip[3];
            minX = std::min(minX, x), maxX = std::max(maxX, x);
            minY = std::min(minY, y), maxY = std::max(maxY, y);
        }
        const float halfX = 1.0f / 1280.0f, halfY = 1.0f / 720.0f; // half a guest pixel in clip units
        if (maxX <= -1.0f || minX >= 1.0f || maxY <= -1.0f || minY >= 1.0f)
            return FramePlace::Centre; // off screen
        const bool left = minX <= -1.0f + halfX, right = maxX >= 1.0f - halfX;
        const bool top = minY <= -1.0f + halfY, bottom = maxY >= 1.0f - halfY;
        if (left && right && top && bottom)
        {
            if (reach != nullptr)
                *reach = std::min(-minX, maxX);
            return FramePlace::Whole;
        }
        const bool corner = top || bottom;
        if (!corner || left == right)
            return FramePlace::Centre;
        return left ? FramePlace::Left : FramePlace::Right;
    }

    // The factor on a Whole draw's clip x once its viewport covers a frame `wideRatio` times the 16:9 one's width: the
    // least stretch that still covers that frame. A draw of the frame's own size (a fade) is stretched with it (1); one
    // reaching at least `wideRatio` past the sides (Trap Team's capture flash, sheets several screens wide) keeps its
    // shape (1 / wideRatio).
    inline float WholeScaleX(float reach, float wideRatio)
    {
        if (!(reach > 0.0f) || !(wideRatio > 1.0f))
            return 1.0f;
        return std::clamp(1.0f / reach, 1.0f / wideRatio, 1.0f);
    }
}
