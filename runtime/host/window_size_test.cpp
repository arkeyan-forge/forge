// Checks the game window's size: parsing window_size, and the size the window opens at (host/window_size.h).
// usage: ArkeyanWindowSizeTest
#include "host/window_size.h"
#include <cstdio>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

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
}

int main()
{
    using windowsize::Size;
    Size size;
    CHECK(windowsize::Parse("1600x900", size) && size == (Size{ 1600, 900 }));
    CHECK(windowsize::Parse("2560X1440", size) && size == (Size{ 2560, 1440 }));
    CHECK(windowsize::Parse("100x1000", size) && size == (Size{ 640, 1000 }));
    size = { 7, 7 };
    CHECK(!windowsize::Parse("", size) && !windowsize::Parse("0x0", size) && !windowsize::Parse("abc", size) &&
          !windowsize::Parse("x900", size) && !windowsize::Parse("1600 x 900", size) && size == (Size{ 7, 7 }));
    CHECK(windowsize::Format({ 1600, 900 }) == "1600x900");

    const Size screen{ 1920, 1050 }; // a 1080p display's usable area, under its task bar
    CHECK(windowsize::Choose({}, {}, screen) == (Size{ 1280, 720 }));
    CHECK(windowsize::Choose({}, {}, {}) == (Size{ 1280, 720 }));
    CHECK(windowsize::Choose({}, { 1500, 800 }, screen) == (Size{ 1500, 800 }));
    CHECK(windowsize::Choose({ 1600, 900 }, { 1500, 800 }, screen) == (Size{ 1600, 900 }));
    // Remembered on a bigger screen: kept inside this one.
    CHECK(windowsize::Choose({}, { 3440, 1440 }, screen) == (Size{ 1920, 1050 }));
    CHECK(windowsize::Choose({ 2560, 1440 }, {}, {}) == (Size{ 2560, 1440 }));
    CHECK(windowsize::Choose({ 10, 10 }, {}, screen) == (Size{ 640, 360 }));
    // A display smaller than the minimum wins over it.
    CHECK(windowsize::Choose({}, {}, { 600, 340 }) == (Size{ 600, 340 }));

    printf("window size test: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
