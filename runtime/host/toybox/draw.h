#pragma once

// The Toy Box's look: everything drawn, nothing decided (the logic is model.h). The design's tokens and pure logic are
// look.h (docs/runtime/host/ingame_ui.md). Drawn with Dear ImGui into a 1280x720 RGBA image on the CPU
// (host/imgui_software.h), or larger on a large screen, letterboxed over the game like its frames.
#include "host/toybox/model.h"

namespace toybox
{
    constexpr int CanvasWidth = 1280, CanvasHeight = 720;

    // Fonts, the backdrop. False if the fonts did not load.
    bool DrawInit();

    // The current screen into `pixels` (look::Scaled(CanvasWidth x CanvasHeight, scale), RGBA; the layout stays in
    // canvas pixels). Over the game (the wheel, its list, a toast with the Toy Box closed) the image is transparent around what
    // is drawn.
    void Draw(Model& model, std::vector<uint32_t>& pixels, float scale = 1);

    // Mouse, in canvas pixels, on what the last Draw showed: button 1 left, 3 right; `twice` for a double click.
    void Click(Model& model, float x, float y, int button, bool twice);
    // A left press at a canvas point, after its Click: a shelf card or a seat's virtual figure under it may be dragged.
    void Press(float x, float y);
    // The pointer moved with the left button held. True while a drag runs (draw again).
    bool Drag(float x, float y);
    // The left button released (`inside` false: off the canvas): a drag drops its card there, on a seat or the shelf.
    // True when a drag ended.
    bool Release(Model& model, float x, float y, bool inside);
    // Escape: the drag ends with nothing done. True when one ran.
    bool CancelDrag();
    // The pointer in canvas pixels (`inside` false: off the canvas): what is under it gets a hover outline. True when that
    // changed since the last Draw (draw again).
    bool Pointer(float x, float y, bool inside);
    // The hoverable hit under the pointer, not the keyboard's selected row/card; "none" for a miss or backdrop.
    std::string HoverTarget();
    // The wheel's place under a canvas point (-1 the centre, -2 outside it).
    int WheelPlaceAt(float x, float y);
}
