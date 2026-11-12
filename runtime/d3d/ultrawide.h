// Ultrawide: a window wider than 16:9 shows more of the world instead of bars or a stretched picture.
//
// The game draws the 3D scene into 16:9 targets (SSA 1120x704, Giants 1000x600) and composes it with the HUD into a
// 1280x720 frame. With a wider window the camera's aspect is widened (Hor+: the vertical field of view stays), the
// scene targets become `scene height x aspect` wide and the frame `720 x aspect`; the composite fills the frame and the HUD, drawn for 1280x720,
// stays centred (d3d/render.cpp).
// The scene's size is chosen once, at start: the games make their scene targets once (SSA and Giants before the title)
// and keep copies of them the runtime cannot follow. It is made for the widest shape the window can take here, the
// window's or its display's, so that maximising the window or going fullscreen later is as sharp as a start at that
// size. The window's shape is followed afterwards: once a new size has held for SettleMs, the camera's
// aspect and the frame (host targets only) change at the next swap, and the composite fits the scene to the new frame
// (correct shapes; narrower than the scene: more pixels than it shows; wider: stretched). `aspect=16:9` never follows.
// The game's own addresses are in engines/<engine>/ultrawide.cpp and games/<id>/adapter.cpp.
#pragma once

#include "game_hooks.h"
#include <cstdint>

namespace ultrawide
{
    // The game's 3D scene at 16:9 (SSA 1120x704, Giants 1000x600: games/<id>/addresses.h).
    constexpr uint32_t SceneWidth16x9 = game::UltrawideSceneWidth, SceneHeight16x9 = game::UltrawideSceneHeight;
    constexpr uint32_t FrameWidth16x9 = 1280, FrameHeight = 720;
    // How long a new window size must hold before the picture follows it (a drag sends many sizes).
    constexpr uint64_t SettleMs = 300;

    // The 16:9 scene the runtime asks the game for, when it is not the game's own (SceneWidth16x9 x SceneHeight16x9):
    // Swap Force at full resolution renders 1280x720 (game::Adapter::fullScene). `keepsArea` replaces
    // game::UltrawideSceneKeepsArea (a full-size scene has no EDRAM budget to keep). `maxWidth` (0: none) bounds a wide
    // scene: past it the scene is that wide (a multiple of 32) and as high as the window's aspect gives (a multiple of 8).
    // Called once, before Configure.
    void SetScene16x9(uint32_t width, uint32_t height, bool keepsArea, uint32_t maxWidth = 0);
    uint32_t Scene16x9Width();
    uint32_t Scene16x9Height();

    // What follows the window: whether it is wider than 16:9 (and the setting allows it), its aspect (16:9 when not),
    // the frame's width (1280 when not, else 720 x aspect).
    struct Layout
    {
        bool active = false;
        float aspect = float(FrameWidth16x9) / FrameHeight;
        uint32_t frameWidth = FrameWidth16x9;
        bool operator==(const Layout&) const = default;
    };
    Layout LayoutFor(bool automatic, uint32_t windowWidth, uint32_t windowHeight);

    // A window size that held for SettleMs. Sizes of 0 (a minimised window) are ignored.
    class Settle
    {
    public:
        explicit Settle(uint32_t width = 0, uint32_t height = 0) : m_width(width), m_height(height) {}
        // True once, when the size seen since `nowMs - SettleMs` is new: Width() and Height() are then that size.
        bool Update(uint32_t width, uint32_t height, uint64_t nowMs);
        uint32_t Width() const { return m_width; }
        uint32_t Height() const { return m_height; }

    private:
        uint32_t m_width, m_height; // the last settled size
        uint32_t m_seenWidth = 0, m_seenHeight = 0; // the size being watched, since m_since
        uint64_t m_since = 0;
    };

    // The shape the scene is made for: the window's, or the display's when that is wider (the window can become it by
    // being maximised). A scene that keeps its pixel count (game::UltrawideSceneKeepsArea: wider means less tall) takes
    // the window's: made for a wider display, a 16:9 window would show fewer lines than a 16:9 start.
    struct Shape
    {
        uint32_t width, height;
        bool operator==(const Shape&) const = default;
    };
    Shape SceneShape(Shape window, Shape display);

    // `automatic` is [video] aspect=auto; the window's size in pixels, its display's (0x0: unknown). Called once
    // before the title starts: the scene's size for the run (SceneShape), and the first layout (the window's).
    void Configure(bool automatic, uint32_t windowWidth, uint32_t windowHeight, uint32_t displayWidth = 0,
        uint32_t displayHeight = 0);

    // The main thread, every pass of its loop: the window's size now. A settled new shape becomes the next layout.
    void WindowSize(uint32_t width, uint32_t height, bool minimized, uint64_t nowMs);

    // Whether the frame can take `layout` while the game runs: not a wide one when the scene is the frame's own size
    // (1280x720, a full-resolution scene started at 16:9), which the renderer cannot tell from the frame. That one
    // waits for the next start, as FSR does (d3d/upscale.h).
    bool Follows(const Layout& layout);

    // The render thread, at the swap: takes the next layout. True when it changed the frame (the caller makes its frame
    // targets again and puts the camera's aspect where the game reads it).
    bool FrameEnd();

    // Whether the window is wider than 16:9 and the setting allows it (follows the window).
    bool Active();

    // The window's aspect when Active(), else 16:9 (the game's own).
    float Aspect();

    // Whether the scene was made wider than 16:9 at start (the scene hooks widen its targets): the window or the
    // display was.
    bool SceneWide();

    // The 3D scene's size: Scene16x9Width() x Scene16x9Height() unless SceneWide(). Wide (SSA): the height stays, the width is
    // height x aspect, a multiple of 32 (the sun tiles are 32x32 pixels). With game::UltrawideSceneKeepsArea (Giants)
    // the scene keeps its 16:9 pixel count and is at most 1280 wide, so that it still fits the console's 10 MB of
    // EDRAM (a bigger one makes the game split it into tiles) and the game's viewports (capped at the back buffer's
    // width): 2560x1080 gives 1184x496; width a multiple of 16, height of 8 (the passes halve it three times).
    // Fixed for the run.
    uint32_t SceneWidth();
    uint32_t SceneHeight();

    // The frame's width (its height stays 720): 1280 unless Active(), else 720 x aspect.
    uint32_t FrameWidth();

    // Movies keep 16:9 with black bars: while one is open (the Bink hooks count them, d3d/bink_display.cpp), the scene
    // the game may still draw behind it stays 16:9 too and the sides are cleared to black.
    void MovieOpened();
    void MovieClosed();
    bool MoviePlaying();
}
