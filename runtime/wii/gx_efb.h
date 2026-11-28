#pragma once

// The embedded frame buffer on the host (docs/runtime/wii/gx.md): the picture GX draws into, 640 x 528 colours and
// depths, and its copy to the external frame buffer in guest memory (YUYV, what the video interface scans out), with
// the clear the copy may do.
#include <cstdint>

namespace wii::gx
{
    constexpr uint32_t EfbWidth = 640, EfbHeight = 528;

    struct Efb
    {
        uint32_t colour[EfbWidth * EfbHeight]; // R G B A in memory order
        uint32_t depth[EfbWidth * EfbHeight];  // 24 bits

        void Clear(uint32_t rgba, uint32_t z, uint32_t left, uint32_t top, uint32_t width, uint32_t height);
    };

    // A copy to the external frame buffer, as BP 0x49-0x4E describe it (BP 0x52's bit 0x4000).
    struct XfbCopy
    {
        uint32_t address = 0; // physical
        uint32_t width = 0, height = 0; // pixels, the height after the vertical scale
        uint32_t stride = 0;  // bytes between rows
    };

    // The BP registers' copy fields [GXSetDispCopySrc, GXSetDispCopyDst, GXSetDispCopyYScale, GXSetCopyClear,
    // GXCopyDisp]: 0x49 the source's left (bits 0-9) and top (10-19), 0x4A its width and height minus one, 0x4B the
    // destination >> 5, 0x4D the stride in 32 bytes, 0x4E 256 / the vertical scale, 0x4F and 0x50 the clear colour
    // (A R, G B), 0x51 the clear depth; 0x52 the copy: 0x4000 to the external frame buffer, 0x800 clear after, 0x400
    // scaled vertically.
    namespace copy
    {
        constexpr uint32_t ToXfb = 0x4000, ClearAfter = 0x800, ScaleY = 0x400;
    }
    XfbCopy DisplayCopyOf(const uint32_t bp[256]);

    // The copy itself: the EFB's rectangle to `base` + the copy's cached address, YUYV (each pair of pixels Y0 U Y1
    // V, BT.601's video range, the pair's chroma their mean), rows repeated or dropped by the vertical scale; then the
    // clear when BP 0x52 asks. False when the destination is outside memory (nothing written then).
    bool CopyToXfb(Efb& efb, uint8_t* base, const uint32_t bp[256], XfbCopy& done);

    // An external frame buffer back to RGBA for the host's window.
    void XfbToRgba(const uint8_t* yuyv, uint32_t width, uint32_t height, uint32_t stride, uint8_t* rgba);
}
