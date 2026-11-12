// SuperChargers' adapter (runtime/game.h). Its portal prompt is read from the CMagicMoment object
// (superchargers_portal_prompt.h), for a Skylander or a vehicle; its voices can follow another language than the text
// (language.cpp); its button glyphs are pictures its CPU text renderer copies into strings, redrawn in guest memory
// (glyphs::MemoryImage), as Swap Force's. No input rows yet. The game asks for present
// interval 2 and paces itself on it, as Swap Force (the same studio's engine): presentIntervalPacesGame.
#include "game.h"
#include "host/portal.h"
#include "memory.h"
#include "superchargers.h"
#include "superchargers_portal_prompt.h"
#include <chrono>
#include <cstdio>

namespace
{
    using namespace portalprompt;

    constexpr std::chrono::milliseconds PromptHold{ 2500 };

    // Four guest reads a frame, no scan: the instance pointer is a global of the XEX.
    Prompt ReadPortalPrompt()
    {
        if (g_memory.base == nullptr)
            return {};
        uint32_t instance = g_memory.Read<uint32_t>(SuperChargersMagicMomentInstance);
        uint32_t state = 0, reason = 0, player = ~0u;
        if (instance >= 0x10000 && uint64_t(instance) + SuperChargersPlayerOffset + 4 <= 0xE0000000ull)
        {
            state = g_memory.Read<uint32_t>(instance + SuperChargersStateOffset);
            reason = g_memory.Read<uint32_t>(instance + SuperChargersReasonOffset);
            player = g_memory.Read<uint32_t>(instance + SuperChargersPlayerOffset);
        }
        static uint32_t lastState = ~0u, lastReason = ~0u, lastPlayer = ~0u;
        if (PortalLogEnabled() && (state != lastState || reason != lastReason || player != lastPlayer))
            fprintf(stderr, "[toybox] portal prompt: magic moment state %u, player %d, reason %u\n", state,
                int32_t(player), reason);
        lastState = state;
        lastReason = reason;
        lastPlayer = player;
        // The vehicle's intro passes through Idle for its driver even with the Skylander on the portal, and the game reads
        // the figure about a second later (run scV1): a prompt counts once it has held for PromptHold.
        static Prompt last;
        static std::chrono::steady_clock::time_point since;
        const auto now = std::chrono::steady_clock::now();
        const Prompt prompt = ClassifySuperChargers(state, reason, player);
        if (!(prompt == last))
        {
            last = prompt;
            since = now;
        }
        return now - since >= PromptHold ? prompt : Prompt{};
    }

    // The fonts' "button" images (fonts\ProximaNova-Body.igz in essentialui.pak, -Info.igz in languagestartup.pak; 32-bit
    // tiled ARGB), which the CPU text renderer copies into its strings, as Swap Force's: found by the XXH3 of their first 4096
    // bytes. The info image is Swap Force's, byte for byte (the same hash, the same cells); the body one is new, 128x209, the
    // buttons in a column of pairs (and a D-pad, which has no glyph, left as it is). Cells: the buttons' bounding boxes in the
    // stored rows (games/superchargers/docs/art.md, "Button pictures").
    using glyphs::Cell;
    using glyphs::Glyph;
    constexpr Cell BodyButtons[] = {
        // 128x209
        { Glyph::A, 1, 2, 38, 39 },
        { Glyph::B, 44, 1, 80, 39 },
        { Glyph::Y, 85, 1, 123, 39 },
        { Glyph::X, 2, 42, 39, 80 },
        { Glyph::LeftBumper, 44, 49, 105, 76 },
        { Glyph::LeftTrigger, 4, 83, 40, 121 },
        { Glyph::RightTrigger, 49, 83, 86, 121 },
        { Glyph::RightBumper, 3, 131, 65, 158 },
        { Glyph::LeftStickIcon, 73, 125, 110, 161 },
        { Glyph::RightStickIcon, 5, 166, 42, 202 },
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
        { 0x402F2F6915FA96F8ull, "body font buttons 128x209", 128, 209, 114688, BodyButtons },
        { 0x7360B650D459A892ull, "info font buttons 256x168", 256, 168, 196608, InfoButtons },
    };

    // The 3D prompts that float in the world (models/UI/ButtonPrompt/XENON in permanent_xenon.pak): a face and the letters
    // on it, coloured by one tiny map (games/superchargers/docs/art.md, "World prompts"). Indices as stored.
    constexpr uint64_t RbPicture = 0xB0770000000000A1ull; // the key of its picture's row, no guest texture has it
    constexpr glyphs::Texture GlyphTextures[] = {
        { RbPicture, "world prompt RB", Glyph::RightBumper, {}, false, false, nullptr, true },
    };
    constexpr glyphs::Prompt GlyphPrompts[] = {
        { 816, 0x21E1098D0C5EEC4Full, 2706, 0x7F26CABF048F8787ull, RbPicture, 320, 128, 0, 1, false, true, 2.55f },
    };

    // The two start-up logos (skip_intro_movies); the story movies (fmv01...) always play.
    constexpr std::string_view IntroMovies[] = { "bumper_atvi", "bumper_vvisions" };

    // The front end: the game's own menus (zone.cpp names the zones it enters).
    constexpr std::string_view MenuLevels[] = { "ui/mainmenubackground/mainmenubackground" };

    // fmv01's own skip never comes (its cutscene ignores every button; games/superchargers/docs/boot.md, "Movies: the
    // skip"): Y ends a story movie, the button Imaginators skips with.
    constexpr uint16_t SkipButtonY = 0x8000;

    // A 2560x720 scene (a 32:9 window at full resolution) stopped the game as it entered the Academy, at both resolution
    // scales, with a thread deep in a recursive walk (sub_8283F448) and the others waiting; 1696x720 (21:9) and the original
    // resolution's 1280x360 at 32:9 play (runs scU2, scU8, scU10). Wider windows get a scene this wide, less high.
    constexpr uint32_t MaxSceneWidth = 2048;

    // The font of the profile's name in the menus' corner (docs/runtime/host/profiles.md): every French capital, and in
    // lower case only these four; the others drew an empty box (runs prof-scfont1 to 5).
    constexpr std::string_view NameAccents = "ÀÂÄÆÇÈÉÊËÎÏÔÖÙÛÜæéêö";

    // SKY_LEVEL is zone.cpp's (the zone's name, not a file redirect: firstLevel stays empty).
    const game::Adapter g_adapter = { g_gameInfo, ReadPortalPrompt, true, superchargers::SetVoiceLanguage,
        GlyphTextures, IntroMovies, false, nullptr, {}, GlyphImages, true, { 1280, 720 }, nullptr, nullptr, 0, {}, {},
        {}, MenuLevels, MaxSceneWidth, GlyphPrompts, SkipButtonY, NameAccents };
}

const game::Adapter& game::Current() { return g_adapter; }

// Skylauncher's glow for this game (its StyleOf).
uint32_t game::Accent() { return 0xF07008; }
