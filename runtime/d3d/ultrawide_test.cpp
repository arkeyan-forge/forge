// Checks the wide scene and frame sizes, the scene's width bound, the layout a window's shape gives, the settling of a
// resize and the scene made for a wider display (d3d/ultrawide.h). usage: ArkeyanUltrawideTest
#include "d3d/ultrawide.h"
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
}

int main()
{
    // A full-resolution scene (1280x720) with no bound: 32:9 gives 2560x720.
    ultrawide::SetScene16x9(1280, 720, false);
    ultrawide::Configure(true, 3840, 1080);
    CHECK(ultrawide::Active());
    CHECK(ultrawide::SceneWidth() == 2560 && ultrawide::SceneHeight() == 720);
    CHECK(ultrawide::FrameWidth() == 2560);

    // Bounded at 2048 (SuperChargers): 32:9 keeps its shape at 2048x576; the frame does not change.
    ultrawide::SetScene16x9(1280, 720, false, 2048);
    ultrawide::Configure(true, 3840, 1080);
    CHECK(ultrawide::SceneWidth() == 2048 && ultrawide::SceneHeight() == 576);
    CHECK(ultrawide::FrameWidth() == 2560);

    // 21:9 is under the bound: 1696x720, as without it.
    ultrawide::SetScene16x9(1280, 720, false, 2048);
    ultrawide::Configure(true, 2560, 1080);
    CHECK(ultrawide::SceneWidth() == 1696 && ultrawide::SceneHeight() == 720);

    // 16:9: nothing is active.
    ultrawide::SetScene16x9(1280, 720, false, 2048);
    ultrawide::Configure(true, 1280, 720);
    CHECK(!ultrawide::Active());

    // The layout of a window's shape: 16:9 and within 1 % of it stays the console's; aspect=16:9 never widens.
    CHECK(ultrawide::LayoutFor(true, 1280, 720) == ultrawide::Layout{});
    CHECK(ultrawide::LayoutFor(true, 1366, 768) == ultrawide::Layout{});
    CHECK(ultrawide::LayoutFor(false, 2560, 1080) == ultrawide::Layout{});
    CHECK(ultrawide::LayoutFor(true, 1280, 1024) == ultrawide::Layout{}); // taller than 16:9: as the console
    CHECK(ultrawide::LayoutFor(true, 0, 0) == ultrawide::Layout{});
    {
        const ultrawide::Layout wide = ultrawide::LayoutFor(true, 2560, 1080);
        CHECK(wide.active && wide.frameWidth == 1707 && wide.aspect > 2.36f && wide.aspect < 2.38f);
        const ultrawide::Layout far = ultrawide::LayoutFor(true, 4000, 500); // 8:1 is clamped to 4:1
        CHECK(far.active && far.aspect == 4.0f && far.frameWidth == 2880);
    }

    // Settling: a size counts once it held for SettleMs; a drag's sizes, a minimised window's 0x0 and the size already
    // settled do not.
    {
        ultrawide::Settle settle(1280, 720);
        CHECK(!settle.Update(1280, 720, 0));
        CHECK(!settle.Update(1280, 720, 1000)); // unchanged
        uint64_t t = 2000;
        for (uint32_t w = 1300; w < 2560; w += 40, t += 16) // a drag: a new size every frame
            CHECK(!settle.Update(w, 1080, t));
        CHECK(!settle.Update(2560, 1080, t));
        CHECK(!settle.Update(2560, 1080, t + ultrawide::SettleMs - 1));
        CHECK(!settle.Update(0, 0, t + 100)); // minimised meanwhile: ignored, the watch goes on
        CHECK(settle.Update(2560, 1080, t + ultrawide::SettleMs));
        CHECK(settle.Width() == 2560 && settle.Height() == 1080);
        CHECK(!settle.Update(2560, 1080, t + 5000)); // once
        // Back to the settled size before the delay: nothing to do.
        CHECK(!settle.Update(1280, 720, t + 6000));
        CHECK(!settle.Update(2560, 1080, t + 6100));
        CHECK(!settle.Update(2560, 1080, t + 7000));
    }

    // Following the window: the camera's aspect and the frame change at the swap after the size settled; the scene keeps
    // the start's size. With aspect=16:9 nothing follows.
    {
        ultrawide::SetScene16x9(1120, 704, false);
        ultrawide::Configure(true, 1280, 720);
        CHECK(!ultrawide::Active() && !ultrawide::SceneWide() && ultrawide::FrameWidth() == 1280);
        ultrawide::WindowSize(2560, 1080, false, 10000);
        CHECK(!ultrawide::FrameEnd());
        ultrawide::WindowSize(2560, 1080, false, 10000 + ultrawide::SettleMs);
        CHECK(!ultrawide::Active()); // not before the swap
        CHECK(ultrawide::FrameEnd());
        CHECK(ultrawide::Active() && ultrawide::FrameWidth() == 1707 && ultrawide::Aspect() > 2.36f);
        CHECK(!ultrawide::SceneWide() && ultrawide::SceneWidth() == 1120 && ultrawide::SceneHeight() == 704);
        CHECK(!ultrawide::FrameEnd());
        // Minimised: nothing changes.
        ultrawide::WindowSize(0, 0, true, 20000);
        ultrawide::WindowSize(1280, 720, true, 20000);
        ultrawide::WindowSize(1280, 720, true, 30000);
        CHECK(!ultrawide::FrameEnd() && ultrawide::Active());
        // Back to 16:9.
        ultrawide::WindowSize(1280, 720, false, 40000);
        ultrawide::WindowSize(1280, 720, false, 40000 + ultrawide::SettleMs);
        CHECK(ultrawide::FrameEnd() && !ultrawide::Active() && ultrawide::FrameWidth() == 1280);
        CHECK(ultrawide::Aspect() > 1.777f && ultrawide::Aspect() < 1.778f);

        // Started wide: the scene stays wide when the window goes to 16:9.
        ultrawide::Configure(true, 2560, 1080);
        CHECK(ultrawide::SceneWide() && ultrawide::SceneWidth() == 1664);
        ultrawide::WindowSize(1920, 1080, false, 50000);
        ultrawide::WindowSize(1920, 1080, false, 50000 + ultrawide::SettleMs);
        CHECK(ultrawide::FrameEnd() && !ultrawide::Active() && ultrawide::SceneWidth() == 1664);
        // A size that gives the same layout (another 16:9 one): nothing for the swap.
        ultrawide::WindowSize(1280, 720, false, 60000);
        ultrawide::WindowSize(1280, 720, false, 60000 + ultrawide::SettleMs);
        CHECK(!ultrawide::FrameEnd());

        // A full-resolution scene started at 16:9 is the frame's size: it does not widen live (next start); started
        // wide, it follows both ways.
        ultrawide::SetScene16x9(1280, 720, false);
        ultrawide::Configure(true, 1280, 720);
        CHECK(!ultrawide::Follows(ultrawide::LayoutFor(true, 2560, 1080)));
        CHECK(ultrawide::Follows(ultrawide::LayoutFor(true, 1280, 720)));
        ultrawide::WindowSize(2560, 1080, false, 65000);
        ultrawide::WindowSize(2560, 1080, false, 65000 + ultrawide::SettleMs);
        CHECK(!ultrawide::FrameEnd() && !ultrawide::Active());
        ultrawide::Configure(true, 2560, 1080);
        CHECK(ultrawide::SceneWidth() == 1696);
        ultrawide::WindowSize(1280, 720, false, 66000);
        ultrawide::WindowSize(1280, 720, false, 66000 + ultrawide::SettleMs);
        CHECK(ultrawide::FrameEnd() && !ultrawide::Active());
        ultrawide::WindowSize(3440, 1440, false, 67000);
        ultrawide::WindowSize(3440, 1440, false, 67000 + ultrawide::SettleMs);
        CHECK(ultrawide::FrameEnd() && ultrawide::Active() && ultrawide::FrameWidth() == 1720);

        // aspect=16:9: a wide window changes nothing.
        ultrawide::Configure(false, 1280, 720);
        ultrawide::WindowSize(2560, 1080, false, 70000);
        ultrawide::WindowSize(2560, 1080, false, 70000 + ultrawide::SettleMs);
        CHECK(!ultrawide::FrameEnd() && !ultrawide::Active() && ultrawide::FrameWidth() == 1280);
    }

    // A 16:9 window on a wider display: the scene is made for the display (what a start maximised there gives), the
    // camera and the frame stay 16:9 until the window widens; then only the frame changes, the scene is already that
    // size. A display that is not wider, or none known, changes nothing.
    {
        ultrawide::SetScene16x9(1120, 704, false);
        CHECK((ultrawide::SceneShape({ 1280, 720 }, { 2560, 1080 }) == ultrawide::Shape{ 2560, 1080 }));
        CHECK((ultrawide::SceneShape({ 2560, 1080 }, { 1920, 1080 }) == ultrawide::Shape{ 2560, 1080 }));
        CHECK((ultrawide::SceneShape({ 1280, 720 }, { 0, 0 }) == ultrawide::Shape{ 1280, 720 }));
        CHECK((ultrawide::SceneShape({ 3840, 1080 }, { 3440, 1440 }) == ultrawide::Shape{ 3840, 1080 }));
        ultrawide::Configure(true, 1280, 720, 2560, 1080);
        CHECK(!ultrawide::Active() && ultrawide::FrameWidth() == 1280 && ultrawide::Aspect() < 1.78f);
        CHECK(ultrawide::SceneWide() && ultrawide::SceneWidth() == 1664 && ultrawide::SceneHeight() == 704);
        ultrawide::Configure(true, 2560, 1080);
        CHECK(ultrawide::SceneWidth() == 1664); // the same scene as a start at that size
        ultrawide::Configure(true, 1280, 720, 2560, 1080);
        ultrawide::WindowSize(2560, 1080, false, 100000);
        ultrawide::WindowSize(2560, 1080, false, 100000 + ultrawide::SettleMs);
        CHECK(ultrawide::FrameEnd() && ultrawide::Active() && ultrawide::FrameWidth() == 1707 &&
              ultrawide::SceneWidth() == 1664);
        ultrawide::Configure(true, 1280, 720, 1920, 1200); // 16:10: not wider than 16:9
        CHECK(!ultrawide::SceneWide() && ultrawide::SceneWidth() == 1120);
        ultrawide::Configure(false, 1280, 720, 2560, 1080); // aspect=16:9
        CHECK(!ultrawide::SceneWide() && ultrawide::SceneWidth() == 1120);

        // A full-resolution scene on a wide display is not the frame's size: it follows the window both ways.
        ultrawide::SetScene16x9(1280, 720, false, 2048);
        ultrawide::Configure(true, 1280, 720, 2560, 1080);
        CHECK(ultrawide::SceneWidth() == 1696 && ultrawide::Follows(ultrawide::LayoutFor(true, 2560, 1080)));
        ultrawide::Configure(true, 1280, 720, 3840, 1080); // the width bound holds
        CHECK(ultrawide::SceneWidth() == 2048 && ultrawide::SceneHeight() == 576);

        // A scene that keeps its pixel count (Giants) takes the window's shape: 16:9 keeps its 600 lines.
        ultrawide::SetScene16x9(1000, 600, true);
        CHECK((ultrawide::SceneShape({ 1280, 720 }, { 2560, 1080 }) == ultrawide::Shape{ 1280, 720 }));
        ultrawide::Configure(true, 1280, 720, 2560, 1080);
        CHECK(!ultrawide::SceneWide() && ultrawide::SceneWidth() == 1000 && ultrawide::SceneHeight() == 600);
    }

    printf("ultrawide test: %s\n", g_failures == 0 ? "ok" : "FAILED");
    return g_failures == 0 ? 0 : 1;
}
