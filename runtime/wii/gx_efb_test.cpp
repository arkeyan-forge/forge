// The embedded frame buffer's copy to the external frame buffer and back (docs/runtime/wii/gx.md). No game data.
#include "wii/gx_efb.h"
#include "wii/memory_map.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

namespace
{
    int checks = 0, failures = 0;
#define EXPECT(condition)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
        }                                                                                                              \
    } while (0)

    using namespace wii::gx;
    uint8_t* g_base = nullptr;

    bool Near(const uint8_t* pixel, uint32_t r, uint32_t g, uint32_t b)
    {
        return std::abs(int(pixel[0]) - int(r)) <= 2 && std::abs(int(pixel[1]) - int(g)) <= 2 &&
               std::abs(int(pixel[2]) - int(b)) <= 2 && pixel[3] == 255;
    }

    // A display copy of the EFB's top-left width x height to `address`, as GXCopyDisp sets it.
    void Copy(uint32_t bp[256], uint32_t address, uint32_t width, uint32_t height, uint32_t scale, bool clear)
    {
        bp[0x49] = 0;
        bp[0x4A] = (width - 1) | (height - 1) << 10;
        bp[0x4B] = address >> 5;
        bp[0x4D] = width * 2 / 32;
        bp[0x4E] = scale;
        bp[0x52] = copy::ToXfb | (clear ? copy::ClearAfter : 0) | (scale != 256 ? copy::ScaleY : 0);
    }

    void Colours(Efb& efb)
    {
        // Four colour bars, then back through YUYV.
        const uint32_t bars[4] = { 0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF };
        for (uint32_t y = 0; y < EfbHeight; y++)
            for (uint32_t x = 0; x < EfbWidth; x++)
                efb.colour[y * EfbWidth + x] = bars[x / 160];
        uint32_t bp[256] = {};
        constexpr uint32_t Xfb = 0x01000000; // physical, in MEM1
        Copy(bp, Xfb, 640, 528, 256, false);
        XfbCopy done;
        EXPECT(CopyToXfb(efb, g_base, bp, done));
        EXPECT(done.width == 640 && done.height == 528 && done.stride == 1280 && done.address == Xfb);
        // White: Y 235, chroma 128.
        const uint8_t* white = g_base + 0x80000000 + Xfb + 2 * 500;
        EXPECT(white[0] == 235 && white[1] == 128 && white[2] == 235 && white[3] == 128);
        std::vector<uint8_t> rgba(640 * 528 * 4);
        XfbToRgba(g_base + 0x80000000 + Xfb, 640, 528, 1280, rgba.data());
        EXPECT(Near(&rgba[4 * 10], 255, 0, 0));
        EXPECT(Near(&rgba[4 * (200 + 640 * 100)], 0, 255, 0));
        EXPECT(Near(&rgba[4 * (400 + 640 * 527)], 0, 0, 255));
        EXPECT(Near(&rgba[4 * 639], 255, 255, 255));

        // Scaled to 576 lines (PAL's 528 stretched), with the clear after: the EFB then holds the clear colour.
        bp[0x4F] = 0x80 << 8 | 0x10; // A, R
        bp[0x50] = 0x20 << 8 | 0x30; // G, B
        bp[0x51] = 0x123456;
        const uint32_t scale = 256 * 528 / 576; // 234: GXSetDispCopyYScale(576 / 528.0)
        Copy(bp, Xfb, 640, 528, scale, true);
        EXPECT(CopyToXfb(efb, g_base, bp, done));
        EXPECT(done.height == 577); // (527 << 8) / 234 + 1: every source line, stretched
        EXPECT(efb.colour[0] == 0x80302010u && efb.colour[EfbWidth * EfbHeight - 1] == 0x80302010u);
        EXPECT(efb.depth[1234] == 0x123456);
        rgba.resize(size_t(640) * done.height * 4);
        XfbToRgba(g_base + 0x80000000 + Xfb, 640, done.height, 1280, rgba.data());
        EXPECT(Near(&rgba[4 * (639 + 640 * 575)], 255, 255, 255)); // the copy was made before the clear

        // A destination outside memory: refused, nothing written.
        Copy(bp, 0x01FFFFE0, 640, 528, 256, false); // past MEM1's 24 MB
        EXPECT(!CopyToXfb(efb, g_base, bp, done));
    }

    void Black()
    {
        const uint8_t black[4] = { 16, 128, 16, 128 };
        uint8_t rgba[8];
        XfbToRgba(black, 2, 1, 4, rgba);
        EXPECT(Near(rgba, 0, 0, 0) && Near(rgba + 4, 0, 0, 0));
        Efb* efb = new Efb;
        efb->Clear(0xFF00FF00, 0xFFFFFFFF, 630, 520, 100, 100); // clipped to the EFB
        EXPECT(efb->colour[EfbWidth * EfbHeight - 1] == 0xFF00FF00 && efb->depth[EfbWidth * EfbHeight - 1] == 0xFFFFFF);
        delete efb;
    }
}

int main()
{
    std::string error;
    if (!wii::MapAddressSpace(g_base, {}, error))
    {
        fprintf(stderr, "cannot map the address space: %s\n", error.c_str());
        return 1;
    }
    auto efb = std::make_unique<Efb>();
    Colours(*efb);
    Black();
    printf("gx efb test: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
