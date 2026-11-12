#include "d3d/ultrawide.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <mutex>

namespace
{
    constexpr float Wide = float(ultrawide::FrameWidth16x9) / ultrawide::FrameHeight;
    constexpr float MaxAspect = 4.0f; // 32:9 is 3.56; anything beyond is clamped (the sides would be mostly sky)

    uint32_t g_baseWidth = ultrawide::SceneWidth16x9, g_baseHeight = ultrawide::SceneHeight16x9;
    bool g_keepsArea = game::UltrawideSceneKeepsArea;
    uint32_t g_sceneWidth = ultrawide::SceneWidth16x9, g_sceneHeight = ultrawide::SceneHeight16x9;
    uint32_t g_maxWidth = 0;
    bool g_sceneWide;
    std::atomic<int> g_movies;

    // The layout in use (render thread writes it at the swap; the guest's camera hooks read it any time).
    std::atomic<bool> g_active;
    std::atomic<float> g_aspect = Wide;
    std::atomic<uint32_t> g_frameWidth = ultrawide::FrameWidth16x9;

    // The window's side (main thread) and the layout it settled on, for the next swap.
    bool g_automatic;
    ultrawide::Settle g_settle;
    std::mutex g_nextMutex;
    ultrawide::Layout g_next;
    bool g_hasNext;
}

void ultrawide::SetScene16x9(uint32_t width, uint32_t height, bool keepsArea, uint32_t maxWidth)
{
    g_baseWidth = g_sceneWidth = width;
    g_baseHeight = g_sceneHeight = height;
    g_keepsArea = keepsArea;
    g_maxWidth = maxWidth;
}

uint32_t ultrawide::Scene16x9Width() { return g_baseWidth; }
uint32_t ultrawide::Scene16x9Height() { return g_baseHeight; }

ultrawide::Layout ultrawide::LayoutFor(bool automatic, uint32_t windowWidth, uint32_t windowHeight)
{
    const float aspect = windowHeight != 0 ? float(windowWidth) / float(windowHeight) : Wide;
    // A window within 1 % of 16:9 (1366x768, 1920x1080 with a title bar...) behaves exactly as before.
    if (!automatic || aspect <= Wide * 1.01f)
        return {};
    Layout layout;
    layout.active = true;
    layout.aspect = std::min(aspect, MaxAspect);
    layout.frameWidth = std::max(FrameWidth16x9, uint32_t(std::lround(FrameHeight * layout.aspect)));
    return layout;
}

bool ultrawide::Settle::Update(uint32_t width, uint32_t height, uint64_t nowMs)
{
    if (width == 0 || height == 0)
        return false;
    if (width != m_seenWidth || height != m_seenHeight)
    {
        m_seenWidth = width;
        m_seenHeight = height;
        m_since = nowMs;
        return false;
    }
    if (nowMs - m_since < SettleMs || (width == m_width && height == m_height))
        return false;
    m_width = width;
    m_height = height;
    return true;
}

ultrawide::Shape ultrawide::SceneShape(Shape window, Shape display)
{
    const auto wider = [](Shape a, Shape b) { return uint64_t(a.width) * b.height > uint64_t(b.width) * a.height; };
    if (g_keepsArea || display.width == 0 || display.height == 0 || window.height == 0 || !wider(display, window))
        return window;
    return display;
}

void ultrawide::Configure(
    bool automatic, uint32_t windowWidth, uint32_t windowHeight, uint32_t displayWidth, uint32_t displayHeight)
{
    g_automatic = automatic;
    g_settle = Settle(windowWidth, windowHeight);
    g_hasNext = false;
    const Layout layout = LayoutFor(automatic, windowWidth, windowHeight);
    g_active = layout.active;
    g_aspect = layout.aspect;
    g_frameWidth = layout.frameWidth;
    const Shape scene = SceneShape({ windowWidth, windowHeight }, { displayWidth, displayHeight });
    const Layout sceneLayout = LayoutFor(automatic, scene.width, scene.height);
    g_sceneWide = sceneLayout.active;
    g_sceneWidth = g_baseWidth;
    g_sceneHeight = g_baseHeight;
    if (!sceneLayout.active)
        return;
    const float aspect = sceneLayout.aspect;
    if (g_keepsArea)
    {
        float area = float(g_baseWidth) * float(g_baseHeight);
        g_sceneWidth = std::min(FrameWidth16x9, uint32_t(std::sqrt(area * aspect) / 16.0f) * 16);
        g_sceneHeight = uint32_t(float(g_sceneWidth) / aspect / 8.0f) * 8;
    }
    else
    {
        g_sceneWidth = std::max(g_baseWidth, uint32_t(std::lround(g_baseHeight * aspect / 32.0f)) * 32);
        if (g_maxWidth != 0 && g_sceneWidth > g_maxWidth)
        {
            g_sceneWidth = g_maxWidth / 32 * 32;
            g_sceneHeight = uint32_t(std::lround(g_sceneWidth / aspect / 8.0f)) * 8;
        }
    }
    fprintf(stderr, "[ultrawide] window %ux%u: aspect %.3f, scene %ux%u%s, frame %ux%u\n", windowWidth, windowHeight,
        layout.aspect, g_sceneWidth, g_sceneHeight,
        scene == Shape{ windowWidth, windowHeight } ? "" : " (the display's shape)", layout.frameWidth, FrameHeight);
}

void ultrawide::WindowSize(uint32_t width, uint32_t height, bool minimized, uint64_t nowMs)
{
    if (!g_automatic || minimized || !g_settle.Update(width, height, nowMs))
        return;
    const Layout layout = LayoutFor(true, width, height);
    if (layout.active && !Follows(layout))
    {
        fprintf(stderr,
            "[ultrawide] window %ux%u settled: the scene is the frame's size this run (%ux%u): wider at the next "
            "start\n",
            width, height, g_sceneWidth, g_sceneHeight);
        return;
    }
    std::lock_guard lock(g_nextMutex);
    g_next = layout;
    g_hasNext = true;
    fprintf(stderr, "[ultrawide] window %ux%u settled: aspect %.3f, frame %ux%u (the scene stays %ux%u)\n", width,
        height, layout.aspect, layout.frameWidth, FrameHeight, g_sceneWidth, g_sceneHeight);
}

bool ultrawide::Follows(const Layout& layout)
{
    // The renderer tells the frame by its guest size (1280x720): a scene of that size too (a full-resolution scene
    // started at 16:9: Swap Force and the later games) would be widened and moved as the frame.
    return !layout.active || g_sceneWidth != FrameWidth16x9 || g_sceneHeight != FrameHeight;
}

bool ultrawide::FrameEnd()
{
    Layout next;
    {
        std::lock_guard lock(g_nextMutex);
        if (!g_hasNext)
            return false;
        g_hasNext = false;
        next = g_next;
    }
    const Layout now{ g_active, g_aspect, g_frameWidth };
    if (next == now)
        return false;
    g_aspect = next.aspect;
    g_frameWidth = next.frameWidth;
    g_active = next.active;
    return true;
}

bool ultrawide::Active() { return g_active; }
float ultrawide::Aspect() { return g_aspect; }
bool ultrawide::SceneWide() { return g_sceneWide; }
uint32_t ultrawide::SceneWidth() { return g_sceneWidth; }
uint32_t ultrawide::SceneHeight() { return g_sceneHeight; }
uint32_t ultrawide::FrameWidth() { return g_frameWidth; }
void ultrawide::MovieOpened() { g_movies++; }
void ultrawide::MovieClosed() { g_movies = std::max(g_movies - 1, 0); }
bool ultrawide::MoviePlaying() { return g_movies > 0; }
