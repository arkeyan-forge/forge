// Spyro's Adventure's adapter: what the runtime asks of the game (runtime/game.h).
#include "game.h"
#include "game_hooks.h"
#include "host/portal.h"
#include "d3d/ultrawide.h"
#include "memory.h"
#include "ssa.h"
#include "ssa_portal_prompt.h"
#include <atomic>
#include <cctype>
#include <cstdio>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    // Guest pointers into the heaps (0x10000-0x80000000) or the image; anything else is not followed.
    bool Plausible(uint32_t address)
    {
        return (address >= 0x10000 && address < 0x80000000) || (address >= 0x82000000 && address < 0x83000000);
    }

    portalprompt::Prompt ReadPortalPrompt()
    {
        using namespace portalprompt;
        if (g_memory.base == nullptr)
            return {};
        uint32_t mode = g_memory.Read<uint32_t>(SsaPromptObject + SsaPromptModeOffset);
        static uint32_t lastMode = ~0u, lastHash = ~0u;
        auto seen = [&](uint32_t hash) {
            // SKY_PORTAL_LOG=1: what the title shows, when it changes.
            if (PortalLogEnabled() && (mode != lastMode || hash != lastHash))
                fprintf(stderr, "[toybox] portal prompt: mode %u, text hash 0x%08X\n", mode, hash);
            lastMode = mode;
            lastHash = hash;
        };
        if (mode != SsaPromptScreenMode)
        {
            seen(0);
            return {};
        }
        // The HUD's portal message (embedded in it) -> the text element it points to -> the text's hash.
        uint32_t message = SsaHud + SsaHudMessageOffset; // an object inside the HUD
        uint32_t text = g_memory.Read<uint32_t>(message + SsaMessageTextOffset);
        uint32_t hash = Plausible(text) ? g_memory.Read<uint32_t>(text + SsaTextHashOffset) : 0;
        seen(hash);
        return ClassifySsa(mode, hash);
    }

    // The title's Xbox glyph textures, all 2D DXT textures from the disc's .str packages (by the hash of their base level).
    using glyphs::Glyph;
    constexpr glyphs::Texture GlyphTextures[] = {
        { 0x5CFC101A45FC493Full, "a_mark", Glyph::A },     // global.str, hudmenu.str: dialogs, prompts ("A OK")
        { 0x6B77FA0C8847038Eull, "a_button", Glyph::A },   // pausemenu.str
        { 0x955A4B4B799FF8E9ull, "b_mark", Glyph::B },     // "B Skip", "B Back"
        { 0x43493F9409303C0Aull, "b_button", Glyph::B },   // pausemenu.str
        { 0x37ACF5999A3F9F99ull, "x_mark", Glyph::X },
        { 0x16DB4319FA0899CBull, "y_mark", Glyph::Y },
        { 0x629C5B2704B8047Eull, "start", Glyph::Start },  // the title's "Press START to begin"
        { 0xE217909DBA45F2A0ull, "360_rs", Glyph::RightStick }, // hudmenu.str, 256x256 DXT1 (opaque)
        { 0xB20E0561D39D1F43ull, "back_mark", Glyph::Back }, // uigameimage.str, the BACK pill (&ICON(0,ICON_DETAILS):
        // "Skylander Info")
        { 0x24D8176CD8936388ull, "360_rsmark",
            Glyph::RightStickIcon }, // uigameimage.str, "RS" (&ICON(0,ICON_RS): "Unjoin
        // Game"); the keyboard has no right-stick click: its keys show
        // Drawn on the keyboard only (glyphs.h): the Controls page's pad (Options > Controls), keycaps where its buttons are,
        // and the blue arrows of &ICON(0,ICON_RL) / ICON_UD (the left stick's keys; stick_text.cpp puts them in the hints
        // that name the stick).
        { 0xE27708D85EE5F3B7ull, "360_controller", Glyph::Controller }, // startmenu.str, pausemenu.str, 256x256
        { 0x7BBEBB5A5444F7C2ull, "arrow_rl", Glyph::ArrowsLeftRight }, // uigameimage.str, 64x32
        { 0xD6AA804FD58B90A5ull, "arrow_ud", Glyph::ArrowsUpDown }, // uigameimage.str, 32x32
    };

    constexpr std::string_view IntroMovies[] = { "legal", "atvilogo", "xpeclogo", "tfb" };

    // The level a file starts: \levels\<name>\mission.str (the front end's is one too).
    std::string LevelOfFile(std::string_view path)
    {
        constexpr std::string_view marker = "\\levels\\", mission = "\\mission.str";
        std::string lower(path);
        for (char& c : lower)
            c = char(tolower(static_cast<unsigned char>(c)));
        size_t at = lower.find(marker), end = lower.rfind(mission);
        if (at == std::string::npos || end == std::string::npos || end <= at + marker.size() ||
            lower.size() != end + mission.size())
            return {};
        std::string name = lower.substr(at + marker.size(), end - at - marker.size());
        return name.find('\\') == std::string::npos ? name : std::string();
    }

    // The F3 menu's Commands page (host/commands.h): Skylauncher's rows for this game, in its order and words.
    constexpr commands::Row InputRows[] = {
        { "left_up", "Move up", "Haut", nullptr },
        { "left_down", "Move down", "Bas", nullptr },
        { "left_left", "Move left", "Gauche", nullptr },
        { "left_right", "Move right", "Droite", nullptr },
        { "a", "1st Power / Accept", "1er pouvoir / Accepter", "a" },
        { "x", "2nd Power", "2e pouvoir", "x" },
        { "y", "Use / Talk", "Utiliser / Parler", "y" },
        { "b", "Special Power / Back", "Pouvoir spécial / Retour", "b" },
        { "back", "Skylander Info", "Infos du Skylander", "back" },
        { "start", "Pause", "Pause", "start" },
        { "right_up", "Shake / Insert / Swing", "Agiter / Insérer / Balancer", nullptr },
        { "right_down", "Same", "Idem", nullptr },
        { "right_left", "Same", "Idem", nullptr },
        { "right_right", "Same", "Idem", nullptr },
    };

    // The mouse once on: attack 1 on the left button, attack 2 on the right (Skylauncher's rows name them so).
    constexpr mouse::Default MouseDefaults[] = { { "a", mouse::Left }, { "x", mouse::Right } };
    // The front end: the game's own menus, where the mouse does nothing.
    constexpr std::string_view MenuLevels[] = { "frontend" };

    // SKY_LEVEL: the first story level's files are levels\level_027\..., audio\levels\level_027.bnk,
    // audio\level_027.lxb and text\<lang>\level_027.lxb.
    const game::Adapter g_adapter = { g_gameInfo, ReadPortalPrompt, true, ssa::SetVoiceLanguage, GlyphTextures,
        IntroMovies, false, nullptr, { "\\level_027", "\\." }, {}, false, {}, nullptr, LevelOfFile, 0, {}, InputRows,
        MouseDefaults, MenuLevels };
}

const game::Adapter& game::Current() { return g_adapter; }

// Skylauncher's glow for this game (its StyleOf).
uint32_t game::Accent() { return 0x6BBAFF; }

// Ultrawide, the camera: sub_827F56A0 divides the video mode's width by its height and stores it as the display's aspect
// (game::UltrawideDisplayAspect); the renderer's state copies it and every camera takes its aspect from there and widens
// its horizontal field of view with it itself (tan(h/2) = tan(v/2) x aspect: Hor+ already), so culling follows. Setting
// that one global is the camera hook; there is no perspective builder to hook as in the later games. The scene targets'
// sizes are the Lux engine's (engines/lux/ultrawide.cpp).
#ifdef GAME_HOOK_Ultrawide_CameraAspect
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_CameraAspect));
// The display's size becomes the globals the cameras read (aspect, screen width and height...): the aspect is the
// window's. The UI's screen width and height stay 1280x720, so the HUD keeps its 16:9 layout.
PPC_FUNC(GAME_HOOK(Ultrawide_CameraAspect))
{
    GAME_IMP(Ultrawide_CameraAspect)(ctx, base);
    if (!ultrawide::Active())
        return;
    static std::atomic<bool> logged;
    if (!logged.exchange(true))
        fprintf(stderr, "[ultrawide] camera aspect %.3f -> %.3f\n", g_memory.Read<float>(game::UltrawideDisplayAspect),
            ultrawide::Aspect());
    g_memory.Write<float>(game::UltrawideDisplayAspect, ultrawide::Aspect());
}
#endif
