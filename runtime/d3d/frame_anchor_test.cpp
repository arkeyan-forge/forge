// Checks where a HUD draw goes in the wide frame: a piece in a corner of the 16:9 frame (Trap Team's menu vignette)
// moves to that corner of the window, everything else stays centred (d3d/frame_anchor.h). usage: ArkeyanFrameAnchorTest
#include "d3d/frame_anchor.h"
#include <cstdio>

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

    // A unit quad (x and z from 0 to 1, as the game's UI quads) through `rows` (c0-c3, clip = x c0 + y c1 + z c2 + c3).
    d3d::FramePlace PlaceQuad(const float rows[16])
    {
        const float quad[12] = { 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1 };
        return d3d::PlaceFrameDraw(quad, 4, 3, rows);
    }
}

int main()
{
    using d3d::FramePlace;
    const float w = 1108.51f;

    // The vignette's four corner pieces as Trap Team draws them (vertex constants of the main menu at 2560x1080): 200x320
    // pixels from each corner of the 1280x720 frame, rotated by quarter turns.
    const float topLeft[16] = { 346.41f, 0, 0, 0, 0, 1.34596e-05f, 100.005f, 100, 0, -615.84f, 8.74272e-06f,
        8.74228e-06f, -w, w, 1107.57f, w };
    const float bottomRight[16] = { -346.41f, -0.00827619f, 1.17492e-10f, 1.17486e-10f, 0, 1.34596e-05f, 100.005f, 100,
        -0.00465536f, 615.84f, -8.74272e-06f, -8.74228e-06f, w, -w, 1107.57f, w };
    const float topRight[16] = { 4.1309e-06f, -615.84f, 8.74272e-06f, 8.74228e-06f, 0, 1.34596e-05f, 100.005f, 100,
        -346.41f, -7.34382e-06f, 1.04256e-13f, 1.04251e-13f, w, w, 1107.57f, w };
    const float bottomLeft[16] = { -1.51421e-05f, 615.84f, -8.74272e-06f, -8.74228e-06f, 0, 1.34596e-05f, 100.005f, 100,
        346.41f, 0, 0, 0, -w, -w, 1107.57f, w };
    CHECK(PlaceQuad(topLeft) == FramePlace::Left);
    CHECK(PlaceQuad(bottomLeft) == FramePlace::Left);
    CHECK(PlaceQuad(topRight) == FramePlace::Right);
    CHECK(PlaceQuad(bottomRight) == FramePlace::Right);

    // The notice's crystal icon (60 pixels, in the middle): centred.
    const float icon[16] = { 59.1117f, 0, 0, 0, 0, 2.01894e-05f, 150.008f, 150, 0, -105.087f, 1.49186e-06f,
        1.49179e-06f, -156.744f, -626.419f, 1107.57f, w };
    CHECK(PlaceQuad(icon) == FramePlace::Centre);

    // On the left edge but in neither corner (a bar along the side, a notice sliding in): centred.
    const float leftBar[16] = { 100, 0, 0, 0, 0, 0, 0, 0, 0, -400, 0, 0, -w, 200, 0, w };
    CHECK(PlaceQuad(leftBar) == FramePlace::Centre);

    // Over the whole frame (a fade, a full-screen page): Whole, over the whole wide frame (render.cpp), and never drawn
    // smaller with aspect=fill. Across the whole width but not the height (a band): centred.
    const float full[16] = { 2 * w, 0, 0, 0, 0, 0, 0, 0, 0, -2 * w, 0, 0, -w, w, 0, w };
    CHECK(PlaceQuad(full) == FramePlace::Whole);
    const float beyond[16] = { 2.2f * w, 0, 0, 0, 0, 0, 0, 0, 0, -2.2f * w, 0, 0, -1.1f * w, 1.1f * w, 0, w };
    CHECK(PlaceQuad(beyond) == FramePlace::Whole);
    const float band[16] = { 2 * w, 0, 0, 0, 0, 0, 0, 0, 0, -0.5f * w, 0, 0, -w, w, 0, w };
    CHECK(PlaceQuad(band) == FramePlace::Centre);

    // How far a Whole draw reaches past the frame's nearer side, and how much it is stretched over a wide frame.
    const float quad[12] = { 0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 1 };
    float reach = 0;
    CHECK(d3d::PlaceFrameDraw(quad, 4, 3, full, &reach) == FramePlace::Whole);
    CHECK(reach > 0.999f && reach < 1.001f);
    CHECK(d3d::PlaceFrameDraw(quad, 4, 3, beyond, &reach) == FramePlace::Whole);
    CHECK(reach > 1.099f && reach < 1.101f);
    // Off centre (from -1.1 to 3): the nearer side counts.
    const float lopsided[16] = { 4.1f * w, 0, 0, 0, 0, 0, 0, 0, 0, -2 * w, 0, 0, -1.1f * w, w, 0, w };
    CHECK(d3d::PlaceFrameDraw(quad, 4, 3, lopsided, &reach) == FramePlace::Whole);
    CHECK(reach > 1.099f && reach < 1.101f);
    // A fade of the frame's size: stretched with the wide frame. A sheet twice the screen's width (the capture flash):
    // its shape kept at 21:9 (ratio 1.36), stretched just enough at 32:9 (ratio 2). Between: the least stretch that
    // covers. No wide frame: unchanged.
    const auto near = [](float a, float b) { return a > b - 0.001f && a < b + 0.001f; };
    CHECK(near(d3d::WholeScaleX(1.0f, 1.36f), 1.0f));
    CHECK(near(d3d::WholeScaleX(2.0f, 1.36f), 1.0f / 1.36f));
    CHECK(near(d3d::WholeScaleX(2.0f, 2.0f), 0.5f));
    CHECK(near(d3d::WholeScaleX(1.25f, 2.0f), 0.8f));
    CHECK(near(d3d::WholeScaleX(3.0f, 1.0f), 1.0f));
    CHECK(near(d3d::WholeScaleX(0.0f, 2.0f), 1.0f));
    // A scene veil made for 16:9 (Trap Team's end-of-fight swirl): stretched by the scene's ratio to 16:9 (2.8 wide:
    // 1.575), so that what covered 16:9 covers the wide scene. 16:9, a narrower scene or none: unchanged.
    CHECK(near(d3d::SceneVeilScaleX(2.8f), 2.8f * 9.0f / 16.0f));
    CHECK(near(d3d::SceneVeilScaleX(32.0f / 9.0f), 2.0f));
    CHECK(near(d3d::SceneVeilScaleX(16.0f / 9.0f), 1.0f));
    CHECK(near(d3d::SceneVeilScaleX(4.0f / 3.0f), 1.0f));
    CHECK(near(d3d::SceneVeilScaleX(0.0f), 1.0f));

    // One pixel short of the corner (x from 1 to 201): centred.
    const float inset[16] = { 346.41f, 0, 0, 0, 0, 0, 0, 0, 0, -615.84f, 0, 0, -w + 2 * w / 1280, w, 0, w };
    CHECK(PlaceQuad(inset) == FramePlace::Centre);

    // Partly off screen past the corner (a piece wider than the frame's edge): still that corner.
    const float past[16] = { 400, 0, 0, 0, 0, 0, 0, 0, 0, -615.84f, 0, 0, -w - 50, w + 30, 0, w };
    CHECK(PlaceQuad(past) == FramePlace::Left);

    // Wholly off screen (a notice parked left of the frame): centred, so it stays out of view.
    const float parked[16] = { 300, 0, 0, 0, 0, 0, 0, 0, 0, -615.84f, 0, 0, -w - 400, w, 0, w };
    CHECK(PlaceQuad(parked) == FramePlace::Centre);

    // A 2-float position (x, y; z is 0): the top-left piece drawn in x and y instead of x and z is still that corner.
    const float flatRows[16] = { 346.41f, 0, 0, 0, 0, -615.84f, 0, 0, 0, 0, 0, 0, -w, w, 0, w };
    const float flat[8] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    CHECK(d3d::PlaceFrameDraw(flat, 4, 2, flatRows) == FramePlace::Left);

    // A vertex behind the camera (w <= 0), too few vertices, a 1-float position: centred, no division by zero.
    const float behind[16] = { 346.41f, 0, 0, 0, 0, 0, 0, 0, 0, -615.84f, 0, 0, -w, w, 0, 0 };
    CHECK(PlaceQuad(behind) == FramePlace::Centre);
    const float two[6] = { 0, 0, 1, 0, 0, 1 };
    CHECK(d3d::PlaceFrameDraw(two, 2, 3, topLeft) == FramePlace::Centre);
    CHECK(d3d::PlaceFrameDraw(flat, 4, 1, flatRows) == FramePlace::Centre);

    if (g_failures == 0)
        printf("frame anchor: all checks passed\n");
    return g_failures == 0 ? 0 : 1;
}
