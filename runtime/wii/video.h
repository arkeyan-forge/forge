#pragma once

// The Wii host's window (docs/runtime/wii/gx.md): GX's EFB copies made into the external frame buffers in guest memory,
// and the one the video interface is given each frame shown in the window.
#include <cstdint>

namespace wii::video
{
    // On the main thread, before the game's: SDL's video, the window, the renderer, the command processor's copies.
    bool Start(const char* title, uint8_t* base);

    // On the main thread: the window's events; false once it is closed.
    bool Pump();

    // VISetNextFrameBuffer(address): that external frame buffer shown (its size the last display copy's to it).
    void Show(const uint8_t* base, uint32_t address);
}
