// Trap Team's adapter: what the runtime asks of the game (runtime/game.h). Independent text and voices live in language.cpp.
// No portal prompt or start-up logo list yet. The first level is redirectable; it presents at every blank like the other
// Alchemy games, and SKY_LEVEL redirects the first level's two files (level\level_313_tutorial.arc and .bld, as Giants'
// tutorial) to the level asked for. Its button glyphs are the cells of one colour atlas (games/trapteam/docs/glyphs.md). fullScene: the 3D scene at the frame's 1280x720 instead of the console's 1040x620 (ultrawide.cpp).
#include "game.h"
#include "trapteam.h"

namespace
{
    // fonts\360\font_001_rgb.png (bootstrap.bld), 512x256, 16 bits a pixel with its mips: the coloured pictures of the text
    // (buttons, arrows, an envelope); the letters are another atlas (font_001.png, a distance field). Cells are the buttons'
    // bounding boxes in the stored rows. The four arrows, the envelope and the Start and Back pills' arrows are not glyphs here.
    using glyphs::Cell;
    using glyphs::Glyph;
    constexpr Cell ButtonCells[] = {
        { Glyph::A, 1, 1, 80, 80 },
        { Glyph::X, 88, 1, 167, 80 },
        { Glyph::Y, 175, 1, 254, 80 },
        { Glyph::B, 261, 1, 340, 79 },
        { Glyph::LeftTrigger, 349, 0, 426, 77 },
        { Glyph::LeftStickIcon, 92, 163, 174, 243 },
        { Glyph::RightStickIcon, 1, 163, 84, 242 },
        { Glyph::Start, 182, 178, 258, 232 },
        { Glyph::Back, 266, 178, 342, 232 },
        { Glyph::RightTrigger, 349, 161, 425, 237 },
    };
    // The title's "Press (A) to begin": a whole green A of title.bld, 64x64 DXT5, stored upside down (as Giants' a_button).
    // The world prompts (permanent\global.bld, every level): billboards above a cannon, a chest, a lever. A, RT and X are
    // flipbooks of 14 frames each (85x85 DXT5, the button pressed and released), each frame drawn as the same glyph; LT is
    // one 52x52 ARGB picture; the large A and Y coins are 128x128 DXT1 with mips (the Y is Giants' own). All upside down.
    // The RT prompt above a cannon, a chest or a lever (model Button_360_RT, global.bld): a card whose texture is a grey
    // gradient (128x128 DXT1, upside down) mapped flat on its face (u 0.22-0.78, v 0.09-0.91, measured from its vertices),
    // turned by a texture matrix that spins (vertex constants c5-c8 of its two passes' shaders: a sheen), and "RT" in letters of
    // geometry (639 indices) on it. The glyph is drawn over the gradient where the face is, the matrix held still and the
    // letters not drawn.
    constexpr Cell RtCard[] = { { Glyph::RightTrigger, 28, 12, 99, 116 } };
    constexpr uint64_t RtCardShaders[] = { 0xB1F0E0D50B9B5BEFull, 0x7C35D7F64961D2DFull };
    constexpr glyphs::Model RtCardModel = { 639, 0x66DB72A301169C73ull, RtCardShaders, 5 };
    constexpr glyphs::Texture GlyphTextures[] = {
        { 0x67DFCE1E73302C9Full, "font rgb atlas 512x256", Glyph::A, ButtonCells },
        { 0xA3E90BC0BE57F49Aull, "title a_button 64", Glyph::A, {}, true },
        { 0x31B32FEE3FD10FB3ull, "world a 85", Glyph::A, {}, true },
        { 0x6D31F1F2BEC681FEull, "world a 85", Glyph::A, {}, true },
        { 0xD9B0912A3049B33Eull, "world a 85", Glyph::A, {}, true },
        { 0x3F106FD03CAFBE4Full, "world a 85", Glyph::A, {}, true },
        { 0xC7FACCC4484080A6ull, "world a 85", Glyph::A, {}, true },
        { 0x764474B52D99BCF1ull, "world a 85", Glyph::A, {}, true },
        { 0x45DD348A90C75957ull, "world a 85", Glyph::A, {}, true },
        { 0x444EB61BCCA74415ull, "world a 85", Glyph::A, {}, true },
        { 0x3CDCF166B24BA5F3ull, "world a 85", Glyph::A, {}, true },
        { 0x330C79E9B05959F8ull, "world a 85", Glyph::A, {}, true },
        { 0xC500F130481258EDull, "world a 85", Glyph::A, {}, true },
        { 0xC1D9C017803B0A7Aull, "world a 85", Glyph::A, {}, true },
        { 0xB5E03D0706A1B433ull, "world a 85", Glyph::A, {}, true },
        { 0x3250C2C477346D08ull, "world a 85", Glyph::A, {}, true },
        { 0xFD80BEFAB3D17DCBull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x87F49C19453C2E4Full, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x6A1AEF565AC134D4ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x41142505B7FE20E7ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x1198CA4E327A8822ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x8E5E7455D6DD8138ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xC6DE07A07B3A9650ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xBE7B2783D22B9BE6ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xCB4A799C2E905CF2ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xC846BE15EB3A92E6ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xAA42E6E2A29091A2ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x28FCFBA5ABF8E361ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0xB72E4983FDE5EC66ull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x28BB2F101E172C2Eull, "world rt 85", Glyph::RightTrigger, {}, true },
        { 0x30901A1EFE5AF93Aull, "world x 85", Glyph::X, {}, true },
        { 0x784C768718A4774Cull, "world x 85", Glyph::X, {}, true },
        { 0x36139BABCE577E70ull, "world x 85", Glyph::X, {}, true },
        { 0x3E63B649B047860Aull, "world x 85", Glyph::X, {}, true },
        { 0xE3816E6E593846CCull, "world x 85", Glyph::X, {}, true },
        { 0x500AE23CB890DF3Eull, "world x 85", Glyph::X, {}, true },
        { 0xC61725E3D0D7916Cull, "world x 85", Glyph::X, {}, true },
        { 0x2F7984188A7AE426ull, "world x 85", Glyph::X, {}, true },
        { 0xF6F9C42072E5E049ull, "world x 85", Glyph::X, {}, true },
        { 0x11E96CB03F9AB847ull, "world x 85", Glyph::X, {}, true },
        { 0xC23C858CF841F95Bull, "world x 85", Glyph::X, {}, true },
        { 0xE2C603611B0749EFull, "world x 85", Glyph::X, {}, true },
        { 0xE2020BAA2806FE50ull, "world x 85", Glyph::X, {}, true },
        { 0xBFE2C3E13A7ED940ull, "world x 85", Glyph::X, {}, true },
        { 0x1D8F0223BF4D7422ull, "world lt 52", Glyph::LeftTrigger, {}, true },
        { 0x8446988D31491B4Dull, "world a_coin 128", Glyph::A, {}, true },
        { 0x6DE08CB96DD4DD23ull, "world y_coin 128", Glyph::Y, {}, true },
        { 0x0AB48F4714BAD3F3ull, "world rt card 128", Glyph::RightTrigger, RtCard, true, true, &RtCardModel },
    };

    // The water's ripple simulation (three 512x512 passes a frame, constants fixed: damping 0.955, a step per rendered frame):
    // at 60 its waves ran twice as fast. The pixel shaders of the three passes; the pacer keeps 30 steps a second.
    constexpr uint64_t RippleSimulation[] = { 0x0682358CD557265Dull, 0xC7B28C596E0E48C7ull, 0xC8C6CF1A59A33A12ull };

    // The F3 menu's Commands page (host/commands.h): Skylauncher's rows for this game, in its order and words.
    constexpr commands::Row InputRows[] = {
        { "left_up", "Move up", "Haut", nullptr },
        { "left_down", "Move down", "Bas", nullptr },
        { "left_left", "Move left", "Gauche", nullptr },
        { "left_right", "Move right", "Droite", nullptr },
        { "a", "Jump", "Sauter", "a" },
        { "x", "Attack 1", "Attaque 1", "x" },
        { "b", "Attack 2", "Attaque 2", "b" },
        { "y", "Attack 3", "Attaque 3", "y" },
        { "lb", "Attack 3", "Attaque 3", "lb" },
        { "rb", "Attack 2", "Attaque 2", "rb" },
        { "lt", "Switch Skylander / Villain", "Changer Skylander / Vilain", "lt" },
        { "rt", "Interact", "Interagir", "rt" },
        { "back", "Skylander/Villain Stats", "Stats Skylander/Vilain", "back" },
        { "start", "Pause", "Pause", "start" },
    };

    // The mouse once on: attack 1 on the left button, attack 2 on the right (Skylauncher's rows name them so).
    constexpr mouse::Default MouseDefaults[] = { { "x", mouse::Left }, { "b", mouse::Right } };
    // The front end: the game's own menus, where the mouse does nothing.
    constexpr std::string_view MenuLevels[] = { "title" };
    // The two start-up logos, Giants' own names (the story movies follow, fmv1 first): skip_intro_movies.
    constexpr std::string_view IntroMovies[] = { "atvinewlogo_640", "tfb_logo_640x456" };

    // presentIntervalPacesGame: at fps_limit 60 the game still held 33.3 ms a frame (cutscene runs, 29.6 fps), as Swap Force's
    // Alchemy does at interval 2. maxFpsLimit 60 until a rate above it is measured (games/trapteam/docs/research.md).
    const game::Adapter g_adapter = { g_gameInfo, nullptr, true, trapteam::SetVoiceLanguage, GlyphTextures, IntroMovies,
        true, trapteam::SetTextLanguage, { "\\level\\level_313_tutorial", std::string_view("\0.", 2) }, {}, true,
        { 1280, 720 }, nullptr, game::LevelArcOfFile, 60, RippleSimulation, InputRows, MouseDefaults, MenuLevels };
}

const game::Adapter& game::Current() { return g_adapter; }

// Skylauncher's glow for this game (its StyleOf).
uint32_t game::Accent() { return 0xDE0018; }
