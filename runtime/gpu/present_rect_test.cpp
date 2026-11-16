// Checks where the present pass draws the game's frame in the output, letterboxed or filled (gpu/present_rect.h).
// usage: ArkeyanPresentRectTest
#include "gpu/present_rect.h"
#include <cmath>
#include <cstdio>

namespace
{
    int g_failures = 0;
    int g_checks = 0;

#define CHECK(condition)                                                                                               \
    do                                                                                                                 \
    {                                                                                                                  \
        g_checks++;                                                                                                    \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                                               \
            g_failures++;                                                                                              \
        }                                                                                                              \
    } while (0)

    using gpu::presentrect::Fit;
    using gpu::presentrect::Rect;

    bool Is(const Rect& rect, int32_t x, int32_t y, uint32_t width, uint32_t height)
    {
        if (rect.x == x && rect.y == y && rect.width == width && rect.height == height)
            return true;
        fprintf(stderr, "  got %ux%u at (%d, %d), expected %ux%u at (%d, %d)\n", rect.width, rect.height, rect.x,
            rect.y, width, height, x, y);
        return false;
    }
}

int main()
{
    // Letterbox (no fill): today's rectangle, whole image, bars on what is left.
    CHECK(Is(Fit(1280, 720, 1280, 800, false), 0, 40, 1280, 720));
    CHECK(Is(Fit(1280, 720, 1920, 1080, false), 0, 0, 1920, 1080));
    CHECK(Is(Fit(1280, 720, 2560, 1080, false), 320, 0, 1920, 1080));
    CHECK(Is(Fit(1280, 720, 1024, 768, false), 0, 96, 1024, 576));

    // Fill on 16:10 (the Steam Deck): the height is covered, about 5 % of the width cut on each side.
    CHECK(Is(Fit(1280, 720, 1280, 800, true), -71, 0, 1422, 800));
    CHECK(Is(Fit(1280, 720, 2560, 1600, true), -142, 0, 2844, 1600));
    // Narrower than 16:10 (4:3): the same crop as 16:10, thinner bars above and below.
    CHECK(Is(Fit(1280, 720, 1024, 768, true), -57, 64, 1138, 640));
    // Portrait: still a 16:10 crop, the rest bars.
    CHECK(Is(Fit(1280, 720, 800, 1280, true), -45, 390, 889, 500));
    // Between 16:10 and 16:9 (1280x760): a smaller cut, no bars.
    CHECK(Is(Fit(1280, 720, 1280, 760, true), -36, 0, 1351, 760));

    // An output at least as wide as the image is never cut: the ultrawide path, a 16:9 window, a wider frame.
    CHECK(Is(Fit(1280, 720, 2560, 1080, true), 320, 0, 1920, 1080));
    CHECK(Is(Fit(1280, 720, 1920, 1080, true), 0, 0, 1920, 1080));
    CHECK(Is(Fit(1707, 720, 2560, 1080, true), 0, 0, 2560, 1080));
    // An image no wider than 16:10 (a 4:3 movie) keeps its letterbox: filling would only shrink it.
    CHECK(Is(Fit(640, 480, 1280, 800, true), 106, 0, 1067, 800));

    // A 1x1 or empty output, an empty image: no division by zero, nothing larger than asked for.
    CHECK(Is(Fit(1280, 720, 1, 1, true), 0, 0, 1, 1));
    CHECK(Is(Fit(1280, 720, 1, 1, false), 0, 0, 1, 1));
    CHECK(Is(Fit(1280, 720, 0, 0, true), 0, 0, 0, 0));
    CHECK(Is(Fit(0, 0, 1280, 800, true), 0, 0, 1280, 800));

    // The HUD's scale with fill: what of the 16:9 frame's width stays on screen. 16:10 keeps 90 %; narrower than 16:10
    // the same; 16:9, wider, or no fill: all of it.
    using gpu::presentrect::HudInset;
    CHECK(std::fabs(HudInset(1280, 800) - 0.9f) < 0.002f);
    CHECK(std::fabs(HudInset(2560, 1600) - 0.9f) < 0.002f);
    CHECK(std::fabs(HudInset(1024, 768) - 0.9f) < 0.002f);
    CHECK(std::fabs(HudInset(1280, 760) - 1280.0f / 1351.0f) < 0.002f);
    CHECK(HudInset(1280, 720) == 1.0f && HudInset(1920, 1080) == 1.0f && HudInset(2560, 1080) == 1.0f);
    CHECK(HudInset(0, 0) == 1.0f && HudInset(1, 1) == 1.0f);

    printf("ArkeyanPresentRectTest: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
