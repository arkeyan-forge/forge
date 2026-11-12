// Giants' adapter: what the runtime asks of the game (runtime/game.h). Its voice language is language.cpp's; its portal prompt
// is read from the game script's variables (giants_portal_prompt.h); its button glyphs are cells of its font atlases;
// ultrawide sets the display's aspect (below).
#include "game.h"
#include "d3d/ultrawide.h"
#include "game_hooks.h"
#include "giants.h"
#include "giants_portal_prompt.h"
#include "host/portal.h"
#include "memory.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <ppc_config.h>
#include <ppc_context.h>

namespace
{
    using namespace portalprompt;

    // Guest pointers into the heaps; anything else is not followed.
    bool Plausible(uint32_t address, uint32_t size)
    {
        return address >= 0x10000 && uint64_t(address) + size <= 0xE0000000ull;
    }

    uint32_t Word(uint32_t address) { return g_memory.Read<uint32_t>(address); }

    // The C string at the pointer stored at `address`, empty if it is not one.
    std::string StringAt(uint32_t pointerAddress)
    {
        uint32_t text = Word(pointerAddress);
        if (!Plausible(text, 128))
            return {};
        const char* chars = static_cast<const char*>(g_memory.Translate(text));
        size_t length = strnlen(chars, 128);
        return length < 128 ? std::string(chars, length) : std::string();
    }

    // The name of the object at `object` if it is of class `vtable`.
    std::string NameOf(uint32_t object, uint32_t vtable)
    {
        return Plausible(object, 0x20) && Word(object) == vtable ? StringAt(object + GiantsNameOffset) : std::string();
    }

    // The name of what the script variable at `variable` holds, if that is of class `vtable`.
    std::string ValueName(uint32_t variable, uint32_t vtable)
    {
        return NameOf(Word(variable + GiantsValueOffset), vtable);
    }

    bool IsVariable(uint32_t object, std::string_view name)
    {
        return Plausible(object, 0x20) && NameOf(object, GiantsScriptReferenceVtable) == name;
    }

    bool IsPortalStateVariable(uint32_t object)
    {
        return IsVariable(object, GiantsStateVariable) &&
               ValueName(object, GiantsStateVtable).starts_with(GiantsPortalStatePrefix);
    }

    // Several state machines have a "current state name": the one whose state is a "Portal Control - ..." is this.
    // The script variables sit in the physical heap (the 0xA0000000 window); a scan of its allocations finds them in a
    // few tens of milliseconds, so it runs on its own thread.
    struct Variables
    {
        uint32_t error = 0, state = 0;
    };

    Variables FindVariables()
    {
        Variables found;
        g_physicalHeap.ForEachAllocation([&](uint32_t base, uint32_t size) {
            if (found.error != 0 && found.state != 0)
                return;
            uint32_t begin = 0xA0000000u | base;
            for (uint32_t offset = 0; offset + 0x20 <= size; offset += 4)
            {
                if (Word(begin + offset) != GiantsScriptReferenceVtable)
                    continue;
                if (found.error == 0 && IsVariable(begin + offset, GiantsErrorVariable))
                    found.error = begin + offset;
                else if (found.state == 0 && IsPortalStateVariable(begin + offset))
                    found.state = begin + offset;
            }
        });
        return found;
    }

    // What the watcher last read, as one packed value: kind in the low byte, player above it.
    std::atomic<uint32_t> g_prompt{ 0 };

    void Watch()
    {
        using namespace std::chrono;
        Variables vars;
        auto lastScan = steady_clock::now() - seconds(10);
        std::string lastError, lastState;
        for (;;)
        {
            if (g_memory.base != nullptr)
            {
                if (vars.error != 0 && !IsVariable(vars.error, GiantsErrorVariable))
                    vars.error = 0;
                if (vars.state != 0 && !IsPortalStateVariable(vars.state))
                    vars.state = 0;
                auto now = steady_clock::now();
                if ((vars.error == 0 || vars.state == 0) && now - lastScan >= seconds(2))
                {
                    lastScan = now;
                    Variables found = FindVariables();
                    if (vars.error == 0)
                        vars.error = found.error;
                    if (vars.state == 0)
                        vars.state = found.state;
                    if (PortalLogEnabled() && (found.error != 0 || found.state != 0))
                        fprintf(stderr, "[toybox] portal prompt: script variables at %08X (message) and %08X (state)\n",
                            vars.error, vars.state);
                }
                std::string error = vars.error != 0 ? ValueName(vars.error, GiantsStringInfoVtable) : std::string();
                std::string state = vars.state != 0 ? ValueName(vars.state, GiantsStateVtable) : std::string();
                if (PortalLogEnabled() && (error != lastError || state != lastState))
                    fprintf(
                        stderr, "[toybox] portal prompt: state \"%s\", message \"%s\"\n", state.c_str(), error.c_str());
                lastError = error;
                lastState = state;
                Prompt prompt = ClassifyGiants(error, state);
                g_prompt.store(uint32_t(prompt.kind) | uint32_t(prompt.player) << 8);
            }
            std::this_thread::sleep_for(milliseconds(30));
        }
    }

    Prompt ReadPortalPrompt()
    {
        static std::once_flag started;
        std::call_once(started, [] { std::thread(Watch).detach(); });
        uint32_t packed = g_prompt.load();
        return { Kind(packed & 0xFF), int(packed >> 8) };
    }

    // The button glyphs (docs/runtime/xbox360/input.md, "Glyphs"). Giants draws its prompts' buttons from its fonts:
    // each font atlas (DXT5, one level) has A, X, Y, B, the d-pad arrows (kept: they suit every device), LS, RS and the
    // START (play arrow) and BACK (back arrow) pills after the letters. The cells are the pictures' bounding boxes.
    // A third atlas (1024x1024, hash 1F11BD3F9CB5136B) holds Wii buttons (A, B, Z, C, the nunchuk, +, -): never shown here.
    using glyphs::Cell;
    using glyphs::Glyph;
    constexpr Cell SmallFontCells[] = {
        // 512x512
        { Glyph::A, 3, 451, 44, 492 },
        { Glyph::X, 48, 451, 89, 492 },
        { Glyph::Y, 96, 451, 137, 492 },
        { Glyph::B, 151, 451, 192, 492 },
        { Glyph::LeftStickIcon, 367, 451, 411, 492 },
        { Glyph::RightStickIcon, 457, 401, 501, 442 },
        { Glyph::Start, 417, 457, 460, 486 },
        { Glyph::Back, 464, 457, 507, 486 },
    };
    constexpr Cell LargeFontCells[] = {
        // 1024x1024
        { Glyph::A, 31, 915, 114, 998 },
        { Glyph::X, 118, 915, 201, 998 },
        { Glyph::Y, 210, 915, 293, 998 },
        { Glyph::B, 312, 915, 395, 998 },
        { Glyph::LeftStickIcon, 722, 915, 809, 998 },
        { Glyph::RightStickIcon, 722, 820, 809, 898 },
        { Glyph::Start, 814, 927, 900, 986 },
        { Glyph::Back, 903, 927, 989, 986 },
    };
    constexpr glyphs::Texture GlyphTextures[] = {
        { 0xAD2E31ADEC576354ull, "font atlas 512", Glyph::A, SmallFontCells },
        { 0x462EB82E5F1D8851ull, "font atlas 1024", Glyph::A, LargeFontCells },
        { 0xF4C85169D542FC77ull, "a_button 64", Glyph::A, {}, true }, // 64x64 DXT5, a whole green A ("Press A to
        // begin"), stored upside down
        { 0xE88F26B298A077B7ull, "y_coin 64", Glyph::Y, {},
            true }, // 64x64 DXT1, the gold Y coin of the dialogue button's
        // model (ButtonIcons_wii_c), stored upside down
        // The world prompts' coins (model ButtonIcons_HD_360, driven by UI_Controller_Prompt.ai: "press Y" above a rock to
        // smash with a Giant, in the first levels): 128x128 DXT1, stored upside down, in permanent\global.bld.
        { 0x6DE08CB96DD4DD23ull, "y_coin 128", Glyph::Y, {}, true },
        { 0x7FAE9AE5883D1AB9ull, "a_coin 128", Glyph::A, {}, true },
        // A white RT keycap of the giant robot's HUD (ui_main\sprites\robot\triggerr_360, 86x85 DXT5, upside down), in
        // Level_209_GiantRobot.bld only; its hash assumes the tiled pitch of 32 blocks, not seen in game yet.
        { 0x179828095390F2C2ull, "rt 86x85", Glyph::RightTrigger, {}, true },
    };

    // The two start-up logos (\movies\atvinewlogo_640.mov, \movies\tfb_logo_640x456.mov); the story movies are fmv01..12.
    constexpr std::string_view IntroMovies[] = { "atvinewlogo_640", "tfb_logo_640x456" };

    // The F3 menu's Commands page (host/commands.h): Skylauncher's rows for this game, in its order and words.
    constexpr commands::Row InputRows[] = {
        { "left_up", "Move up", "Haut", nullptr },
        { "left_down", "Move down", "Bas", nullptr },
        { "left_left", "Move left", "Gauche", nullptr },
        { "left_right", "Move right", "Droite", nullptr },
        { "a", "Attack 1 / Accept", "Attaque 1 / Accepter", "a" },
        { "x", "Attack 2", "Attaque 2", "x" },
        { "y", "Action", "Action", "y" },
        { "b", "Attack 3 / Back", "Attaque 3 / Retour", "b" },
        { "back", "Skylander Info", "Infos du Skylander", "back" },
        { "start", "Pause", "Pause", "start" },
    };

    // The mouse once on: attack 1 on the left button, attack 2 on the right (Skylauncher's rows name them so).
    constexpr mouse::Default MouseDefaults[] = { { "a", mouse::Left }, { "x", mouse::Right } };
    // The front end: the game's own menus, where the mouse does nothing.
    constexpr std::string_view MenuLevels[] = { "title" };

    // SKY_LEVEL: the first level is the tutorial, level\level_213_tutotial.arc and .bld (and that path alone).
    const game::Adapter g_adapter = { g_gameInfo, ReadPortalPrompt, true, giants::SetVoiceLanguage, GlyphTextures,
        IntroMovies, true, giants::SetTextLanguage, { "\\level\\level_213_tutotial", std::string_view("\0.", 2) }, {},
        false, {}, nullptr, game::LevelArcOfFile, 60, {}, InputRows, MouseDefaults, MenuLevels };
}

const game::Adapter& game::Current() { return g_adapter; }

// Skylauncher's glow for this game (its StyleOf).
uint32_t game::Accent() { return 0xFFB43A; }

// Ultrawide, the camera: sub_8219B900 reads the video mode, keeps the UI's screen size (1280x720, floats at +180/+184 of
// the display object) and stores the display's aspect (game::UltrawideDisplayAspect). Every camera reads it there when
// it builds its projection (sub_82198DB0 each frame for the level's cameras, sub_8218E2A8, the projection helpers
// sub_821864D0/82186CF8/82186F90), so setting that one global widens them: Hor+, as in SSA. The scene targets' size is
// the Alchemy engine's (engines/alchemy/ultrawide.cpp).
#ifdef GAME_HOOK_Ultrawide_CameraAspect
PPC_FUNC_IMPL(GAME_IMP(Ultrawide_CameraAspect));
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
