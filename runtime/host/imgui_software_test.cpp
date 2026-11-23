// The CPU ImGui renderer at a framebuffer scale: a frame drawn at scale 2 is the scale-1 frame with every pixel doubled
// (rectangle fills, a clipped fill, a nearest-sampled picture), and text grows with it.
#include "host/imgui_software.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <functional>

namespace
{
    int g_checks = 0, g_failures = 0;
    void Check(bool ok, int line)
    {
        g_checks++;
        if (!ok)
        {
            g_failures++;
            fprintf(stderr, "FAIL line %d\n", line);
        }
    }
#define CHECK(test) Check((test), __LINE__)

    constexpr int Width = 64, Height = 32;

    imgui_software::Texture g_picture;

    // One frame of the test scene at `scale` into a (Width x Height) * scale image.
    imgui_software::Texture Frame(int scale, bool text)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(Width, Height);
        io.DisplayFramebufferScale = ImVec2(float(scale), float(scale));
        io.DeltaTime = 1.0f / 60;
        imgui_software::NewFrame();
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        if (text)
            draw->AddText(ImVec2(2, 2), IM_COL32_WHITE, "Toy Box");
        else
        {
            draw->AddRectFilled(ImVec2(4, 4), ImVec2(20, 12), IM_COL32(255, 0, 0, 255));
            draw->AddRectFilled(ImVec2(6, 14), ImVec2(14, 30), IM_COL32(0, 255, 0, 128));
            draw->PushClipRect(ImVec2(40, 0), ImVec2(50, 20));
            draw->AddRectFilled(ImVec2(36, 4), ImVec2(60, 16), IM_COL32(0, 0, 255, 255));
            draw->PopClipRect();
            draw->AddImage(imgui_software::Ref(&g_picture), ImVec2(24, 16), ImVec2(32, 24));
        }
        ImGui::Render();
        imgui_software::Texture target{ Width * scale, Height * scale,
            std::vector<uint32_t>(size_t(Width * scale) * size_t(Height * scale)) };
        imgui_software::Render(ImGui::GetDrawData(), target);
        return target;
    }

    // One frame drawn by `scene` at `scale`.
    imgui_software::Texture Shot(float scale, const std::function<void(ImDrawList*)>& scene)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(Width, Height);
        io.DisplayFramebufferScale = ImVec2(scale, scale);
        io.DeltaTime = 1.0f / 60;
        imgui_software::NewFrame();
        scene(ImGui::GetBackgroundDrawList());
        ImGui::Render();
        const int w = int(std::lround(Width * scale)), h = int(std::lround(Height * scale));
        imgui_software::Texture target{ w, h, std::vector<uint32_t>(size_t(w) * size_t(h)) };
        imgui_software::Render(ImGui::GetDrawData(), target);
        return target;
    }

    uint32_t At(const imgui_software::Texture& t, int x, int y) { return t.pixels[size_t(y) * size_t(t.width) + x]; }

    int Lit(const imgui_software::Texture& t)
    {
        int lit = 0;
        for (uint32_t pixel : t.pixels)
            lit += int((pixel >> 24) != 0);
        return lit;
    }
}

int main()
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefaultBitmap();
    imgui_software::Init();
    g_picture = { 4, 4, std::vector<uint32_t>(16) };
    for (int i = 0; i < 16; i++)
        g_picture.pixels[size_t(i)] = 0xFF000000u | uint32_t(i * 16);
    g_picture.nearest = true;

    imgui_software::Texture one = Frame(1, false), two = Frame(2, false);
    CHECK(Lit(one) > 0);
    int mismatches = 0;
    for (int y = 0; y < two.height; y++)
        for (int x = 0; x < two.width; x++)
            mismatches += int(At(two, x, y) != At(one, x / 2, y / 2));
    CHECK(mismatches == 0);
    CHECK(At(two, 2 * 4, 2 * 4) == 0xFF0000FFu);
    CHECK(At(two, 2 * 45, 2 * 10) == 0xFFFF0000u); // inside the clip
    CHECK(At(two, 2 * 52, 2 * 10) == 0); // clipped

    // Text at scale 2 covers about four times the pixels (glyphs laid out and drawn at the target's size).
    imgui_software::Texture text1 = Frame(1, true), text2 = Frame(2, true);
    CHECK(Lit(text1) > 0);
    CHECK(Lit(text2) > 2 * Lit(text1));
    CHECK(Lit(text2) < 6 * Lit(text1));

    // A picture whose drawn size is its own within float error (a fractional scale's) is copied texel for texel, not
    // resampled one pixel narrower.
    {
        imgui_software::Texture picture{ 19, 18, std::vector<uint32_t>(19 * 18) };
        for (size_t i = 0; i < picture.pixels.size(); i++)
            picture.pixels[i] = 0xFF000000u | uint32_t(i * 37 % 251) << 8 | uint32_t(i * 11 % 253);
        picture.blend = false;
        io.DisplaySize = ImVec2(Width, Height);
        io.DisplayFramebufferScale = ImVec2(1, 1);
        imgui_software::NewFrame();
        ImGui::GetBackgroundDrawList()->AddImage(
            imgui_software::Ref(&picture), ImVec2(2.3f, 2), ImVec2(2.3f + 18.9999f, 20));
        ImGui::Render();
        imgui_software::Texture target{ Width, Height, std::vector<uint32_t>(size_t(Width) * Height) };
        imgui_software::Render(ImGui::GetDrawData(), target);
        int exact = 0;
        for (int y = 0; y < 18; y++)
            for (int x = 0; x < 19; x++)
                exact += int(At(target, 2 + x, 2 + y) == picture.pixels[size_t(y) * 19 + size_t(x)]);
        CHECK(exact == 19 * 18);
    }

    // At a fractional scale, shapes that touch still touch: no gap between two fills or two pictures side by side (the
    // panels' pieces, a rounded box's middle and edges).
    {
        imgui_software::Texture solid{ 4, 4, std::vector<uint32_t>(16, 0xFFFFFFFFu) };
        solid.nearest = true;
        const float scale = 2682 / 1280.0f;
        imgui_software::Texture t = Shot(scale, [&](ImDrawList* draw) {
            for (int i = 0; i < 6; i++)
                draw->AddRectFilled(ImVec2(1.3f + 3.1f * i, 2), ImVec2(1.3f + 3.1f * (i + 1), 8), IM_COL32_WHITE);
            for (int i = 0; i < 6; i++)
                draw->AddImage(imgui_software::Ref(&solid), ImVec2(30 + 3.0f * i, 2), ImVec2(30 + 3.0f * (i + 1), 8));
        });
        int gaps = 0;
        const int y = int(5 * scale);
        for (int x = int(std::ceil(1.3f * scale)); x < int(19.9f * scale); x++)
            gaps += int((At(t, x, y) >> 24) == 0);
        for (int x = int(std::ceil(30 * scale)); x < int(47.9f * scale); x++)
            gaps += int((At(t, x, y) >> 24) == 0);
        CHECK(gaps == 0);
    }

    // Every font's glyphs are rasterized at the frame's scale, not only the current font's: text in another font, drawn
    // with AddText(font, ...), is the same as with that font pushed.
    {
        ImFont* second = io.Fonts->AddFontDefaultVector();
        const char* text = "Add a figure";
        // Direct first: pushing a font leaves its density set for the frames after.
        imgui_software::Texture direct =
            Shot(2, [&](ImDrawList* draw) { draw->AddText(second, 13, ImVec2(2, 2), IM_COL32_WHITE, text); });
        imgui_software::Texture pushed = Shot(2, [&](ImDrawList* draw) {
            ImGui::PushFont(second, 13);
            draw->AddText(ImVec2(2, 2), IM_COL32_WHITE, text);
            ImGui::PopFont();
        });
        CHECK(Lit(pushed) > 0);
        CHECK(pushed.pixels == direct.pixels);
    }

    imgui_software::Shutdown();
    ImGui::DestroyContext();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
