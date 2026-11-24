#include "render_scale.h"
#include <cmath>
#include <cstdio>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void Check(bool condition, int line)
    {
        g_checks++;
        if (!condition)
        {
            fprintf(stderr, "FAIL line %d\n", line);
            g_failures++;
        }
    }

#define CHECK(condition) Check((condition), __LINE__)

    void TestTableScale()
    {
        struct Size
        {
            uint32_t width, height;
            bool table;
        };
        constexpr Size Sizes[]{ { 32, 33, true }, { 33, 32, true }, { 32, 32, true }, { 33, 33, false },
            { 1024, 32, true }, { 256, 16, true }, { 64, 1, true }, { 64, 2, true }, { 1280, 720, false } };
        for (const Size& size : Sizes)
        {
            CHECK(d3d::IsTableSize(size.width, size.height) == size.table);
            CHECK(d3d::IsTableSize(size.height, size.width) == size.table);
            for (int32_t scale = 1; scale <= 3; scale++)
            {
                const int32_t expected = size.table ? 1 : scale;
                CHECK(d3d::TargetScale(size.width, size.height, float(scale)) == float(expected));
                CHECK(d3d::TargetScale(size.height, size.width, float(scale)) == float(expected));
                const uint32_t hostWidth = size.width * expected;
                const uint32_t hostHeight = size.height * expected;
                CHECK(d3d::TargetScale(hostWidth, hostHeight, float(scale)) == float(expected));
            }
        }
    }

    void TestHostKernel()
    {
        constexpr d3d::HostKernel Guest{ { 1.0f / 1280, 1.0f / 720, -0.5f / 1280, -0.5f / 720, 2.0f / 1280, 2.0f / 720,
                                             -3.0f / 1280, -3.0f / 720 },
            0x80000005u };
        constexpr std::array<float, 8> Expected[]{ Guest.constants,
            { 1.0f / 2560, 1.0f / 1440, -0.5f / 2560, -0.5f / 1440, 2.0f / 2560, 2.0f / 1440, -3.0f / 2560,
                -3.0f / 1440 },
            { 1.0f / 3840, 1.0f / 2160, -0.5f / 3840, -0.5f / 2160, 2.0f / 3840, 2.0f / 2160, -3.0f / 3840,
                -3.0f / 2160 } };
        for (int32_t scale = 1; scale <= 3; scale++)
        {
            const auto kernel = d3d::ScaleHostKernel(Guest, float(scale));
            for (size_t i = 0; i < kernel.constants.size(); i++)
                CHECK(std::fabs(kernel.constants[i] - Expected[scale - 1][i]) < 1.0e-9f);
            CHECK(kernel.scaledTextures == (scale == 1 ? Guest.scaledTextures : 0u));
            // One normalized pixel step and a half-texel corner tap stay on the host pixel grid.
            CHECK(std::fabs(kernel.constants[0] * (1280 * scale) - 1.0f) < 1.0e-6f);
            CHECK(std::fabs(kernel.constants[1] * (720 * scale) - 1.0f) < 1.0e-6f);
            const float fetchScale = (kernel.scaledTextures & 1) != 0 ? float(scale) : 1.0f;
            CHECK(0.5f * fetchScale == 0.5f);
            CHECK(d3d::ScaleHostKernel({ Guest.constants, 0 }, float(scale)).scaledTextures == 0);
        }
        CHECK(d3d::ScaleHostKernel(Guest, 1.0f).constants == Guest.constants);
    }

    void TestDrawScissor()
    {
        using d3d::Rect;
        // The test off: the whole target, whatever the device holds (an empty rectangle, the default 65535s).
        Rect full = d3d::DrawScissor(false, { 0, 720, 0, 720 }, { 2, 2 }, 2560, 1440);
        CHECK(full.left == 0 && full.top == 0 && full.right == 2560 && full.bottom == 1440);
        // The test on: scaled, then clamped to the target.
        Rect on = d3d::DrawScissor(true, { 10, 20, 640, 65535 }, { 2, 2 }, 2560, 1440);
        CHECK(on.left == 20 && on.top == 40 && on.right == 1280 && on.bottom == 1440);
        Rect empty = d3d::DrawScissor(true, { 0, 720, 0, 720 }, { 1, 1 }, 1280, 720);
        CHECK(empty.right <= empty.left);
        Rect negative = d3d::DrawScissor(true, { -5, -5, 100, 100 }, { 1, 1 }, 1280, 720);
        CHECK(negative.left == 0 && negative.top == 0);
        // A fractional scale: the guest's full rectangle is the target's.
        const d3d::AxisScale axis = d3d::TargetAxisScale(1707, 1024, 1000.0f * 2.56f / 1500.0f);
        Rect whole = d3d::DrawScissor(true, { 0, 0, 1000, 600 }, axis, 1707, 1024);
        CHECK(whole.left == 0 && whole.top == 0 && whole.right == 1707 && whole.bottom == 1024);
    }

    // The upscaled scene's fractional scales (d3d/upscale.h): sizes rounded, each axis's own scale exact at the edges,
    // whole scales unchanged.
    void TestFractionalScale()
    {
        for (uint32_t scale = 1; scale <= 4; scale++)
        {
            CHECK(d3d::HostSize(1000, float(scale)) == 1000 * scale);
            const d3d::AxisScale whole = d3d::TargetAxisScale(1280 * scale, 720 * scale, float(scale));
            CHECK(whole.x == float(scale) && whole.y == float(scale));
            CHECK(d3d::ToHost(637, whole.x) == int32_t(637 * scale));
        }
        const float scale = 2560.0f / (1000.0f * 1.5f); // 1440p quality, a 1000x600 scene
        CHECK(d3d::HostSize(1000, scale) == 1707 && d3d::HostSize(600, scale) == 1024);
        // The scene's own size and a quarter-size target of it: their guest sizes come back, their edges map exactly.
        for (const auto [guestWidth, guestHeight] :
            { std::array<uint32_t, 2>{ 1000, 600 }, { 250, 150 }, { 1040, 620 } })
        {
            const uint32_t width = d3d::HostSize(guestWidth, scale), height = d3d::HostSize(guestHeight, scale);
            const d3d::AxisScale axis = d3d::TargetAxisScale(width, height, scale);
            CHECK(d3d::ToHost(int32_t(guestWidth), axis.x) == int32_t(width));
            CHECK(d3d::ToHost(int32_t(guestHeight), axis.y) == int32_t(height));
            CHECK(std::fabs(axis.x - scale) < 0.01f && std::fabs(axis.y - scale) < 0.01f);
        }
        // Colour tables keep their guest size.
        const d3d::AxisScale table = d3d::TargetAxisScale(256, 16, scale);
        CHECK(table.x == 1.0f && table.y == 1.0f);
    }

}

int main()
{
    TestTableScale();
    TestHostKernel();
    TestDrawScissor();
    TestFractionalScale();
    fprintf(stderr, "%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
