# The in-game interface: design

> **Purpose:** the one design every in-game surface follows (Toy Box, wheel, F3 panel, the pages beside the rail, toasts and achievements, FPS counter): type, spacing, palette, the game's accent, motion, focus, and how to check it · **Status:** current (2026-11-06, Arkeyan Launcher's look) · **Tags:** [Xbox] [SSA] [Giants] [SwapForce] [TrapTeam]

Since 0.9.0 (2026-11-06) every in-game screen is in **Arkeyan Launcher's look** (the owner: "le style repris du launcher"; the
mockups, git-ignored: `private/design/*.dc.html`, `Look` for the system): the launcher's neutral greys, its fonts, one accent
that is the running game's own colour, the game kept visible (a side panel, corner toasts). No Microsoft or Xbox asset, font,
logo, sound or trade dress; the game's own art (portraits, artworks, achievement pictures) is the star. Code:
`runtime/host/toybox/look.h` (the tokens, the layouts and the pure logic, checked by `ArkeyanToyboxLookTest`), `draw.cpp` (every
screen), `model.cpp` / `menu.cpp` (the state), `toybox.cpp` (`ToyboxAchievement`, the prompt). The screens and their controls:
[toybox_ux.md](toybox_ux.md), [gamemenu.md](gamemenu.md).

## What changed (0.9.0)

| Before (2026-11-02) | Now |
|---|---|
| A dark blue glass, gold the only accent, a white focus ring 4 px outside | The launcher's greys; the game's colour as the one accent; focus a 2 px ring in it, on the element's edge |
| Outfit Black titles on a gold mark | Bungee for page titles and the game's name, Outfit (SemiBold, Bold) for the rest |
| F3: a 520 px sheet on the left, every setting in it | F3: a 420 px panel on the right (volume, Resume, Toy Box, Achievements, Settings, Performance, Report, Quit), the game visible on the left |
| Settings in the F3 sheet; Commands a sheet of its own | Full pages beside a rail (Toy Box, Achievements, Settings, Controls, Performance); Settings one list in sections with a stepper and a right panel (what it does, its cost, when it applies) |
| No achievements page | Achievements: a ring, the count and gamerscore, All / Unlocked / Locked, four columns of cards |
| Toasts at the bottom centre | Corner toasts: the portal's prompt top centre, an achievement top right, a figure bottom left, a notice bottom right |
| Filled coloured A/B/X/Y discs | Outlined circles (PlayStation's shapes drawn) and outlined key caps, by the device in use |
| Seven card columns, the seats on a blue panel | Six columns; the seats under "On the portal", an empty seat dashed |

## Tokens (`look.h`)

- **Canvas**: 1280x720, letterboxed 16:9 over the game at any window size (`gpu::SetOverlay`); an ultrawide window shows
  it centred, the game around it. Sizes below are canvas pixels.
- **Safe area** (TV action-safe, 5 % a side): x 64-1216, y 36-684. What is drawn over the game while it plays (toasts 32 px from
  the edges, the FPS counter) stays in its corners; the full pages and the F3 panel reach the edges, as the launcher's pages do,
  their hints in a 64 px bar at the bottom.
- **Type**: Bungee (the launcher's title face) for page titles and the game's name; Outfit SemiBold for running text, Outfit
  Bold for labels and values (static instances of the launcher's variable Outfit, `resources/toybox/fonts`). Caption 12, Small
  14, Body 16, Label 17, Heading 20, Title 30.
- **Spacing**: 4, 8, 12, 16, 24, 32. **Radii** (the launcher's): cards and panels 14, rows 12, controls 10, keys 6.
- **Palette** (`look::Colour`, the launcher's `launcher_theme.h`): page `1E1E24`, surface `26262C` (the rail, cards), raised
  `2C2C34`, raised2 `3A3A42` (buttons, a focused row, the selected rail row), hairline `3A3A44`, border `4A4A55`, control
  `55555E`, key outline `7A7A88`, well `15171C`, field `1C1C21`; text `F4F4F6` / `D0D0D8` / `B8B8C4`; success `48D17A`,
  warning `FFB43A`, danger text `FF817A`; the out-of-health cross keeps `F85149` (`tools/regress/out_of_health.sh` counts it).
- **The accent**: the game's glow (`GameInfo::glow` in `games/<id>/info.cpp`, the same numbers as Arkeyan Launcher's `StyleOf`:
  Spyro's Adventure `6BBAFF`, Giants `FFB43A`, Swap Force `BFE04A`, Trap Team `DE0018`, SuperChargers `F07008`, Imaginators
  `58A8D8`) lightened toward white in tenths until it reads at 4.5:1 on the surface grey (`look::ReadableAccent`, as Arkeyan Launcher's
  `ReadableAccent`: Trap Team's red becomes a light red). It lights the focus, the selected rail row's bar, switches that are
  on, values being changed, the game's name and the chosen seat's label.
- **Focus**: a 2 px ring in the accent on the element's edge (`FocusRing`), the element in raised2; the pointer's hover is a
  thin grey line 3 px outside it. On the accent's own fill (the main button) the ring is white.
- **Icons**: line icons drawn on a 24-unit grid, 2 units wide (`DrawIcon`: box, trophy, sliders, gamepad, activity, play,
  volume, flag, quit, lock, plus), our own geometry.
- **Button hints** follow the device in use (`glyphs::Current`): an outlined circle with A, B, X, Y (PlayStation's cross,
  circle, square, triangle drawn), outlined caps for LB/RB (L1/R1, L/R), View/Back/Share, Menu/Start/Options and for keys.

## Motion

- **The toast** slides 20 px in from the edge it rests on and fades in over 240 ms (out-cubic), rests, then fades out over
  200 ms (in-quad, drifting 8 px back). Only over the game (Toy Box closed): inside the Toy Box's screens and the F3 menu
  it shows and goes at once, so that a full screen is never redrawn every frame.
- **Reduced motion**: `[video] reduce_motion=1` (or `[<game>.video]`; [settings.md](../../getting-started/settings.md)):
  a 120 ms fade, no slide.
- **Cost**: the overlay is drawn only when something changed (`Model::Dirty`); while a toast moves, one frame each frame
  (about 0.45 s a message). A frame of the slide allocates nothing (the canvas is the caller's buffer, the toast's lines are
  cut once per message, the baked corners are kept per 1/16 of alpha). Nothing shown: no overlay at all (`gpu::SetOverlay(nullptr)`).
  Measured [V] (2026-11-02, SSA's title, macOS, offscreen, no frame limit, 22-56 s, load about 3; `private/runs/ingameui/perf.sh`):
  nothing shown 64.4 fps, gap p50 15.43 p99 17.58 ms; FPS counter on and an achievement sliding in every 2 s (19 of them)
  64.4 fps, p50 15.46-15.47 p99 17.53-17.56 ms. A first run with nothing shown gave 83.7 fps; its cause was not checked [?],
  so it is left out of the comparison.

## The surfaces

- **The rail** (`DrawRail`): 232 px on the left of every full page; the game's name in Bungee and the accent, Toy Box,
  Achievements, Settings, Controls, Performance with their icons (the page shown on raised2 with a 4 px accent bar), the
  profile (`SKY_PLAYER_NAME`, its picture cut round, else its initials on the accent) at the bottom.
- **F3 panel** (`DrawMenu`, `look::Main`): 420 px on the right, page grey at 98 %, a soft shadow on the game, which is dimmed
  by 28 % on its left. The game's name and "Arkeyan · version", the profile's pill; the volume's card with its slider; rows of 52
  px (Report and Quit 48, after a line; Quit in the danger text); the hints under a line. Toasts sit at its left, over the game.
- **Settings** (`DrawSettings`): the list (rows 50 px, section headings in caps), a stepper on the focused row (chevrons, the
  value in the accent, small steps for its place), on the right the setting's name, one line on what it does, its cost to the
  graphics card in five steps and "Applies at the next start" in the warning colour.
- **Achievements** (`DrawAchievements`): the 84 px ring (the accent arc over raised2, the share in Bungee), the title, "6 of
  38 · 60 / 1000 G", the tabs as a segmented control; cards 196 px high in four columns: the medal (the accent and a trophy
  unlocked, raised2 and a lock locked), the points, the name, the description, the date in the success colour or "Locked".
- **Controls** (`DrawCommands`, `look::Commands`): rows 44 px apart (11 a page), cells in three device columns, the page
  buttons at the top right.
- **Toy Box** (`DrawHome`, `DrawSeats`, `Card`): "On the portal" and its one-line help, the game's seats in a row (92 px; the
  chosen one raised with its name in the accent, an empty one dashed; + Add and Manage stacked at the end), "FOR PLAYER 1 · 12
  FIGURES", the search field and Filters, six columns of cards (the portrait, the name, the level after a dot in the element's
  colour). Imaginators' creation crystal sits on a player's seat ("Your Imaginator"); Trap Team's trap and SuperChargers'
  vehicles are seats of their own (the catalogue's `games[].seats`). The Toy Box's other screens (the Figure page, Add a
  figure, Manage, Edit stats...) keep their layouts, restyled through the shared primitives.
- **Toasts** (`DrawToast`, `DrawPrompt`): the game's prompt as a pill at the top centre ("Put a Skylander for Player 1 · on
  the portal", while the game asks and the Toy Box is not open over it); an achievement top right (340 px: its picture, else
  a trophy on the accent; "ACHIEVEMENT", its name, "20 G"); a figure put on a seat bottom left (320 px: its portrait, name,
  seat and level); any other notice bottom right (one or two lines, at most 460 px). On a full page a toast rests above the
  hints at the right; with the F3 panel open, at the game's bottom left.
- **FPS counter** (`DrawFps`): the top right corner of the safe area.
- **Debug screen** (`gamemenu::DrawDebugScreen`): its own look, approved by the owner for it (Minecraft's F3 layout,
  Arkeyan Launcher's greys and fonts, the game's glow as the accent), at the canvas's edges rather than in the safe area; under the
  F3 panel: [gamemenu.md](gamemenu.md), "Debug screen".
- Long names and French labels are fitted (`Fit`, `Wrap`): a label leaves room for its value, a toast takes two lines.

## How to check it

- `ArkeyanToyboxLookTest` (in `tools/test_all.sh`): the toast's curve with and without reduced motion, each game's accent at
  4.5:1 (never darkened) and the greys' contrast, the F3 panel's rows in order above its hints, the rail, `PageStep`, the
  Settings list's scroll, the Achievements grid's rows, and Controls paging with and without a notice.
- Shots of every screen: `private/runs/overlay/shoot.sh <game> <en|fr> <glyphs>` (git-ignored; Imaginators and Trap Team,
  English and French, pad, PlayStation and keyboard glyphs, in `private/runs/overlay/<game>-<lang>/`).
- Shots: `SKY_TOYBOX_SCRIPT` with `shot=<name>` and the test aid **`achievement`** (the title's first achievement announced as
  if unlocked, nothing saved). The lane's script: `private/runs/ingameui/shoot.sh <before|after> <game> <WxH> <en|fr>
  <keyboard|xboxone>` (`SKY_WINDOW_SIZE`, `--language`, `--glyphs`).
- Drawn on the CPU into one RGBA image (`imgui_software`), so Metal, Vulkan and D3D12 show the same pixels: no backend draws
  any of it.
