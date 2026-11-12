// Imaginators' adapter (runtime/game.h), SuperChargers' line. Its portal prompt is read from the CMagicMoment object
// (imaginators_portal_prompt.h); its text and voices follow the settings' language among the disc's two (language.cpp);
// its button glyphs are pictures its CPU text renderer copies into strings, redrawn in guest memory (glyphs::MemoryImage),
// as SuperChargers'; its 3D button prompts follow the device too (glyphs::Prompt). The game asks for present interval 2 and paces itself on it, as SuperChargers and Swap Force:
// presentIntervalPacesGame (at fps_limit 60 it held 33.3 ms a frame without it) [V].
#include "game.h"
#include "host/portal.h"
#include "imaginators.h"
#include "imaginators_portal_prompt.h"
#include "memory.h"
#include <chrono>
#include <cstdio>

namespace
{
    using namespace portalprompt;

    // SuperChargers' hold: a prompt counts once it has held this long (a figure already on the portal is read about a
    // second after Idle comes up).
    constexpr std::chrono::milliseconds PromptHold{ 2500 };

    // Three guest reads a frame, no scan: the instance pointer is a global of the XEX.
    Prompt ReadPortalPrompt()
    {
        if (g_memory.base == nullptr)
            return {};
        uint32_t instance = g_memory.Read<uint32_t>(ImaginatorsMagicMomentInstance);
        uint32_t state = 0, player = ~0u;
        if (instance >= 0x10000 && uint64_t(instance) + ImaginatorsPlayerOffset + 4 <= 0xE0000000ull)
        {
            state = g_memory.Read<uint32_t>(instance + ImaginatorsStateOffset);
            player = g_memory.Read<uint32_t>(instance + ImaginatorsPlayerOffset);
        }
        static uint32_t lastState = ~0u, lastPlayer = ~0u;
        if (PortalLogEnabled() && (state != lastState || player != lastPlayer))
            fprintf(stderr, "[toybox] portal prompt: magic moment state %u, player %d\n", state, int32_t(player));
        lastState = state;
        lastPlayer = player;
        static Prompt last;
        static std::chrono::steady_clock::time_point since;
        const auto now = std::chrono::steady_clock::now();
        const Prompt prompt = ClassifySwapForce(state, player);
        if (!(prompt == last))
        {
            last = prompt;
            since = now;
        }
        return now - since >= PromptHold ? prompt : Prompt{};
    }

    // The fonts' "button" images (fonts\ProximaNova-Body.igz in essentialui.pak, -Info.igz in languagestartup.pak; 32-bit
    // tiled ARGB), which the CPU text renderer copies into its strings, as SuperChargers': found by the XXH3 of their first
    // 4096 bytes. The info image is SuperChargers' and Swap Force's, byte for byte (the same hash); the body one is new,
    // 128x250: SuperChargers' 128x209 column of pairs with a row added, two "Back" pills (one picture for two character
    // codes, 0x86 and 0x2020) and a D-pad, which has no glyph and is left as it is. Cells: the buttons' bounding boxes in
    // the stored rows (games/imaginators/docs/art.md, "Button pictures").
    using glyphs::Cell;
    using glyphs::Glyph;
    constexpr Cell BodyButtons[] = {
        // 128x250
        { Glyph::A, 1, 1, 38, 39 },
        { Glyph::B, 44, 1, 80, 39 },
        { Glyph::Y, 85, 1, 123, 39 },
        { Glyph::X, 2, 42, 39, 80 },
        { Glyph::LeftBumper, 43, 49, 106, 75 },
        { Glyph::LeftTrigger, 4, 83, 40, 121 },
        { Glyph::RightTrigger, 49, 83, 86, 121 },
        { Glyph::RightBumper, 3, 131, 66, 157 },
        { Glyph::LeftStickIcon, 73, 125, 110, 161 },
        { Glyph::RightStickIcon, 5, 166, 42, 202 },
        { Glyph::Back, 47, 172, 83, 196 },
        { Glyph::Back, 89, 172, 125, 196 },
    };
    constexpr Cell InfoButtons[] = {
        // 256x168
        { Glyph::A, 3, 7, 45, 49 },
        { Glyph::B, 51, 6, 92, 49 },
        { Glyph::Y, 97, 7, 139, 49 },
        { Glyph::X, 144, 7, 186, 49 },
        { Glyph::LeftTrigger, 82, 58, 122, 110 },
        { Glyph::RightTrigger, 132, 58, 174, 110 },
        { Glyph::LeftBumper, 5, 71, 74, 101 },
        { Glyph::RightBumper, 182, 71, 250, 101 },
        { Glyph::LeftStickIcon, 144, 117, 188, 160 },
        { Glyph::RightStickIcon, 201, 117, 245, 160 },
        { Glyph::Start, 202, 7, 244, 49 }, // ours, "Press Start" (engines/alchemy/prompt_text.cpp)
    };
    constexpr glyphs::MemoryImage GlyphImages[] = {
        { 0x3B9E95737FB43E23ull, "body font buttons 128x250", 128, 250, 131072, BodyButtons },
        { 0x7360B650D459A892ull, "info font buttons 256x168", 256, 168, 196608, InfoButtons },
    };

    // The two start-up logos (skip_intro_movies); the story movies always play.
    constexpr std::string_view IntroMovies[] = { "bumper_atvi", "bumper_tfb" };

    // The front end: the game's own menus (zone.cpp names the zones it enters).
    constexpr std::string_view MenuLevels[] = { "ui/mainmenubackground/mainmenubackground" };

    // SuperChargers' bound on the scene's width (a 2560x720 scene stopped that game in its hub) [?].
    constexpr uint32_t MaxSceneWidth = 2048;

    // The 3D prompts of models/UI/ButtonPrompt/XENON (permanent_xenon.pak), the same meshes as SuperChargers' (the RB bumper's are
    // byte for byte): a face and the letters on it (games/imaginators/docs/art.md, "World prompts"). Indices as stored.
    constexpr uint64_t RbPicture = 0xB0770000000000A1ull; // the key of its picture's row, no guest texture has it
    constexpr glyphs::Texture GlyphTextures[] = {
        { RbPicture, "world prompt RB", Glyph::RightBumper, {}, false, false, nullptr, true },
    };
    constexpr glyphs::Prompt GlyphPrompts[] = {
        { 816, 0x21E1098D0C5EEC4Full, 2706, 0x7F26CABF048F8787ull, RbPicture, 320, 128, 0, 1, false, true, 2.55f },
    };

    // SKY_LEVEL is zone.cpp's (the zone's name, not a file redirect: firstLevel stays empty).
    const game::Adapter g_adapter = { g_gameInfo, ReadPortalPrompt, true, imaginators::SetVoiceLanguage, GlyphTextures,
        IntroMovies, false, imaginators::SetTextLanguage, {}, GlyphImages, true, { 1280, 720 },
        AlchemySetNightmareFromStart, nullptr, 0, {}, {}, {}, MenuLevels, MaxSceneWidth, GlyphPrompts };
}

const game::Adapter& game::Current() { return g_adapter; }

// Skylauncher's glow for this game (its StyleOf).
uint32_t game::Accent() { return 0x58A8D8; }
