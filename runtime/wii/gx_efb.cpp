#include "wii/gx_efb.h"
#include "wii/memory_map.h"
#include <algorithm>

namespace wii::gx
{
    namespace
    {
        uint8_t Clamp8(int32_t value) { return uint8_t(std::clamp(value, 0, 255)); }

        // BT.601, video range (Y 16-235, chroma 16-240), in 8.8 fixed point.
        void ToYuv(uint32_t rgba, int32_t& y, int32_t& u, int32_t& v)
        {
            const int32_t r = int32_t(rgba & 0xFF), g = int32_t((rgba >> 8) & 0xFF), b = int32_t((rgba >> 16) & 0xFF);
            y = 16 + ((66 * r + 129 * g + 25 * b + 128) >> 8);
            u = 128 + ((-38 * r - 74 * g + 112 * b + 128) >> 8);
            v = 128 + ((112 * r - 94 * g - 18 * b + 128) >> 8);
        }
    } // namespace

    void Efb::Clear(uint32_t rgba, uint32_t z, uint32_t left, uint32_t top, uint32_t width, uint32_t height)
    {
        const uint32_t right = std::min(left + width, EfbWidth), bottom = std::min(top + height, EfbHeight);
        for (uint32_t y = top; y < bottom; y++)
            for (uint32_t x = left; x < right; x++)
            {
                colour[y * EfbWidth + x] = rgba;
                depth[y * EfbWidth + x] = z & 0xFFFFFF;
            }
    }

    XfbCopy DisplayCopyOf(const uint32_t bp[256])
    {
        XfbCopy copy;
        copy.address = (bp[0x4B] & 0xFFFFFF) << 5;
        copy.width = (bp[0x4A] & 0x3FF) + 1;
        const uint32_t height = ((bp[0x4A] >> 10) & 0x3FF) + 1;
        const uint32_t scale = bp[0x4E] & 0x1FF;
        // The SDK's own count [GXSetDispCopyYScale]: (height - 1) * 256 / scale + 1 lines.
        copy.height = (bp[0x52] & copy::ScaleY) != 0 && scale != 0 ? ((height - 1) << 8) / scale + 1 : height;
        copy.stride = (bp[0x4D] & 0x3FF) * 32;
        return copy;
    }

    bool CopyToXfb(Efb& efb, uint8_t* base, const uint32_t bp[256], XfbCopy& done)
    {
        done = DisplayCopyOf(bp);
        const uint32_t left = bp[0x49] & 0x3FF, top = (bp[0x49] >> 10) & 0x3FF;
        const uint32_t sourceHeight = ((bp[0x4A] >> 10) & 0x3FF) + 1;
        const uint32_t destination = 0x80000000u | (done.address & 0x1FFFFFFFu);
        if (done.width < 2 || done.stride < done.width * 2 || !InMemory(destination, done.stride * done.height))
            return false;
        for (uint32_t row = 0; row < done.height; row++)
        {
            // Each output line from the source line it falls on (no filter yet).
            const uint32_t y = std::min(top + row * sourceHeight / done.height, EfbHeight - 1);
            uint8_t* out = base + destination + row * done.stride;
            for (uint32_t x = 0; x + 1 < done.width; x += 2)
            {
                const uint32_t sx = std::min(left + x, EfbWidth - 2);
                int32_t y0, u0, v0, y1, u1, v1;
                ToYuv(efb.colour[y * EfbWidth + sx], y0, u0, v0);
                ToYuv(efb.colour[y * EfbWidth + sx + 1], y1, u1, v1);
                out[2 * x] = Clamp8(y0);
                out[2 * x + 1] = Clamp8((u0 + u1 + 1) / 2);
                out[2 * x + 2] = Clamp8(y1);
                out[2 * x + 3] = Clamp8((v0 + v1 + 1) / 2);
            }
        }
        if ((bp[0x52] & copy::ClearAfter) != 0)
        {
            const uint32_t a = (bp[0x4F] >> 8) & 0xFF, r = bp[0x4F] & 0xFF, g = (bp[0x50] >> 8) & 0xFF,
                           b = bp[0x50] & 0xFF;
            efb.Clear(r | g << 8 | b << 16 | a << 24, bp[0x51], left, top, done.width, sourceHeight);
        }
        return true;
    }

    void XfbToRgba(const uint8_t* yuyv, uint32_t width, uint32_t height, uint32_t stride, uint8_t* rgba)
    {
        for (uint32_t row = 0; row < height; row++)
        {
            const uint8_t* in = yuyv + row * stride;
            uint8_t* out = rgba + size_t(row) * width * 4;
            for (uint32_t x = 0; x + 1 < width; x += 2)
            {
                const int32_t d = int32_t(in[2 * x + 1]) - 128, e = int32_t(in[2 * x + 3]) - 128;
                for (uint32_t i = 0; i < 2; i++)
                {
                    const int32_t c = 298 * (int32_t(in[2 * x + 2 * i]) - 16);
                    uint8_t* pixel = out + 4 * (x + i);
                    pixel[0] = Clamp8((c + 409 * e + 128) >> 8);
                    pixel[1] = Clamp8((c - 100 * d - 208 * e + 128) >> 8);
                    pixel[2] = Clamp8((c + 516 * d + 128) >> 8);
                    pixel[3] = 255;
                }
            }
        }
    }
}
