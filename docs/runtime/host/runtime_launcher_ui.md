# The runtime's launcher window: look and code

> **Purpose:** how is the runtime's own `--launcher` window drawn and laid out, and how is it tested? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA]

**Not Arkeyan Launcher.** This is the runtime's built-in window (`runtime/host/launcher*.cpp`, drawn with SDL_Renderer, no
ImGui), the ancestor of Arkeyan Launcher, which players use instead (separate public repo:
[release/launcher/README.md](../../release/launcher/README.md)). When it opens and how it checks a disc:
[runtime_launcher.md](runtime_launcher.md). The look has its own Skylanders style, kept
modular so it can change without touching the game runtime.

## Structure
- `host/launcher_theme.h` - every colour, size, radius and font setting. Change the look here first.
- `host/widgets/launcher_widgets.{h,cpp}` - reusable buttons, sidebar entries, panels and progress bars.
- `host/launcher_ui.{h,cpp}` - layout, screens, input, verification flow and screenshot test.
- `host/launcher_ui_draw.{h,cpp}` - drawing on an `SDL_Renderer` (`LauncherPainter`), font atlas, PNG loading and background.
- `host/launcher_assets.{h,cpp}` - reads frontend art from the player's disc, decodes it and caches it.
- `resources/launcher/fonts/` - licensed fonts (Bungee, Outfit) with their OFL licences.

## Layout
1. Left sidebar - game list (`LauncherGameCatalog()`: SSA enabled, the five others greyed "coming later", Giants included), Settings at the bottom.
2. Hero area - selected game title, icon from the XDBF, real disc art after verification, drop zone.
3. Bottom bar - large PLAY button, ISO status and change-image action.
4. Settings overlay - text/voice language, fullscreen, resolution scale, game data and quit.

## Disc art
The launcher never ships Skylanders artwork. After an ISO is verified it:
1. reads the title icon from `default.xex` (XDBF image resource);
2. reads `ui/eng/startmenu.str`, inflates the zlib stream at offset `0x14`;
3. finds APKF packages by their original asset path;
4. decodes the BC3/DXT5 payload at `APKF+0x1000` after undoing the Xbox 360 32x32-block tiling;
5. caches PNGs in `<data root>/<Game>/launcher-cache/<TITLEID>/` (`launcher_assets.cpp`; trimmed to 256 MB, oldest files first).

Current role mapping:
| Role | Disc asset | Size |
| --- | --- | --- |
| `logo.png` | `...\startmenu\skylanderslogo.dds` | 1024x512 |
| `hero_bg.png` | `...\pausemenu\godraysdark.dds` | 512x512 |
| `title_icon.png` | `default.xex` XDBF resource | as stored |

`manifest.ini` in the cache records the title ID and where each role came from. The cached files are
local only and are ignored by git; deleting the cache is always safe.

## How to change the look
- Colours: edit the `LauncherTheme` namespace in `launcher_theme.h`.
- Spacing/sizes: change constants such as `SidebarW`, `HeroH`, `BarH`, `FooterH`, `Radius*` (a 1280x800 design canvas, `DesignW`/`DesignH`).
- Fonts: add an OFL/Apache-compatible font under `resources/launcher/fonts/`, then update
  `SKY_LAUNCHER_FONT_DIR` / the font loading in `launcher_ui_draw.cpp`.
- Widgets: add methods to `LauncherWidgets`; keep all colours coming from the theme.

## Screenshot test
The launcher can drive itself without opening a visible window:
```sh
SDL_VIDEO_DRIVER=offscreen \
SKY_LAUNCHER_INI="$PWD/private/scratch/launcher.ini" \
SKY_COLLECTION="$PWD/private/scratch/collection" \
SKYLANDERS_DATA="$PWD/private/scratch/data" \
SKY_LAUNCHER_SHOTS="$PWD/private/launcher-shots" \
SKY_LAUNCHER_TEST_DROP="/path/to/game.iso" \
SKY_LAUNCHER_FONT_DIR="$PWD/resources/launcher/fonts" \
"./build/ArkeyanRuntime/Skylanders Spyro's Adventure.app/Contents/MacOS/Skylanders Spyro's Adventure" --launcher
```
It writes `01-no-iso.png`, `02-verified.png` and `03-settings.png` (or `03-failed.png` when the image is refused) at
1280x800, then quits (`main.cpp` returns when `SKY_LAUNCHER_SHOTS` is set).

## Notes
- Drag & drop ISO is supported through `SDL_EVENT_DROP_FILE`.
- The accepted image is saved to `launcher.ini`; the title and icon come from the player's own XEX.
- `ArkeyanRuntimeLauncherTest` checks verification, settings, font/text helpers and the disc texture decoder.
