# Ultrawide: a Hor+ camera, a scene and a frame as wide as the window, the HUD where it belongs

> **Purpose:** how does `aspect=auto` widen the scene, the frame and the HUD? · **Status:** current (2026-11-16) · **Tags:** [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [SuperChargers] [Imaginators]

`[video] aspect=auto` (default), `fill` or `16:9` (`--aspect`, [settings.md](../../getting-started/settings.md)). With `auto`, a window or display more than 1 %
wider than 16:9 shows more of the world to the sides; a 16:9 window (or `16:9`) runs exactly as before. `fill` is `auto`
plus a narrower window covered without bars (below, "Narrower than 16:9: fill"). Code:
`d3d/ultrawide.{h,cpp}` (the sizes), `engines/lux/ultrawide.cpp` and `engines/alchemy/ultrawide.cpp` (the games' sides),
`games/<id>/adapter.cpp` (the camera), `d3d/render.cpp` (the wide frame). SSA **[Lux]**, Giants and Swap Force **[Alchemy]**
have it; the per-game constants are in `games/<id>/addresses.h` (`Ultrawide*`). [Xbox] for the frame part.
The scene's size is chosen once, when the window is created, for the widest shape the window can take there: the
window's (`SDL_GetWindowSizeInPixels`, offscreen runs: `SKY_WINDOW_SIZE=WxH`) or its display's when that is wider
(`SDL_GetDisplayForWindow`, `SDL_GetDisplayBounds`; offscreen runs: `SKY_DISPLAY_SIZE=WxH`); the camera and the frame then follow the window live
(below, "Following the window").

## Following the window (issue #14) [V] (2026-11-12)

Resizing the window, maximising it or toggling fullscreen widens or narrows the view while the game runs, in every game
that has the wider view. Code: `ultrawide::WindowSize` (the main loop gives it the window's size every pass, `main.cpp`
`FollowWindowShape`), `ultrawide::Settle` (the debounce), `ultrawide::FrameEnd` (the swap, `d3d/render.cpp`).

- **When.** A new size counts once it has held for 300 ms (`ultrawide::SettleMs`): a drag's many sizes do nothing, a
  minimised window (or a 0x0 size) is ignored. The settled shape becomes the next layout (`ultrawide::LayoutFor`: active
  above 16:9 + 1 %, aspect clamped to 4:1, frame `720 x aspect`), applied at the next swap. `aspect=16:9` never follows.
- **What changes live.** The camera's aspect (SSA and Giants: the display aspect global is written at that swap; the
  later games' hooks read `ultrawide::Aspect()` at each call) and the frame: the host's 1280x720 targets are made again
  at the new width (`DropFrameTargets`; the others keep their contents), so the composite fills the new frame and the
  HUD, menus and movies are centred at 16:9 as before. FSR's output and jitter phases follow the new frame (not run with FSR on [?]).
- **The scene is made for the window's display (2026-11-12, second pass) [V].** The games make their scene targets once
  (SSA and Giants before the title, every call of their texture and size hooks comes in the first second) and keep copies
  of them the runtime cannot follow, so the scene's size is chosen at start for the widest shape the window can take:
  `ultrawide::SceneShape` takes the shape of the display the window is on when it is wider than the window's (`main.cpp`
  `WindowDisplay`; not the widest of all: a laptop with a 32:9 monitor attached would draw a 32:9 scene for its own screen). A game started in a 16:9 window on a 21:9 display has the 21:9 scene from the start (SSA 1664x704,
  Swap Force and the later games 1696x720), the camera and the frame at 16:9: the composite fits the wide scene into the
  16:9 frame (more pixels across than shown); maximised or widened later, the scene is exactly the one a start at that size
  makes, as sharp. A window wider than its display at start (moved to a wider monitor, or dragged wider than the screen's
  shape) still stretches the start's scene (Hor+ shapes, softer across) until the next start; a narrower one fits it.
  - **Giants and `original_resolution=1`** (`game::UltrawideSceneKeepsArea`: the scene keeps its 16:9 pixel count, wider
    means less tall) take the window's shape: made for a 21:9 display, a 16:9 window would show 496 lines instead of 600.
    Widened, Giants' 1000x600 scene is stretched over the frame: fewer pixels across than a wide start (1000 against 1184),
    more lines (600 against 496), about the same pixel count.
  - **Cost**: with the display wider than a 16:9 window, the scene has the wide start's pixel count (SSA at scale 2: 3328x1408
    instead of 2240x1408); 30 FPS held in every run below.
  - **Why not rebuild the targets live** (tried in SSA, not kept): the Lux scene targets are 132-byte texture objects (the
    D3D header at +20, the EDRAM surface the engine makes from it at +72), and rewriting their pitch, size and memory at the
    swap (26 of them) gave the same objects as a wide start, yet the picture broke (the lit half black, the other half
    garbage): the game keeps copies made at start, fetch constants in its state blocks and command buffers (up to 930 per
    target, most in old command buffers) and the scene depth's address in the light tile job's globals (`0x82BC0820`, with
    the object). Each game would need its own hunt for these copies; the display's shape gives the same result for all six.
- **A scene of the frame's own size** (the full-resolution 1280x720 scene of Swap Force, Trap Team, SuperChargers and
  Imaginators, started at 16:9 with no wider display): the renderer tells the frame by its guest size and cannot tell the
  two apart, so that run does not widen live (`ultrawide::Follows`; log `the scene is the frame's size this run (1280x720):
  wider at the next start`; the picture stays 16:9 with bars), as FSR's own rule for these games. Started wide, on a wide
  display (or with `original_resolution=1`), they follow both ways.
- **Trap Team**: one of its two world-camera builders takes the scene target's shape; a call with that shape now gets the
  window's aspect (16:9 included), so a wide start shown in a 16:9 window is not squeezed.
- **Logs.** At start on a wider display: `[ultrawide] window 1280x720: aspect 1.778, scene 1664x704 (the display's
  shape), frame 1280x720`. Then `[ultrawide] window 2560x1080 settled: aspect 2.370, frame 1707x720 (the scene stays
  1664x704)`, and `[ultrawide] frame 1707x720 from frame 1096: camera aspect 2.370` at the swap that applies it.
- **Tests.** `ArkeyanUltrawideTest` (the layout, the debounce through a drag and a minimised window, the follow both
  ways, the frame-sized scene, `aspect=16:9`). Scripted runs (`SKY_WINDOW_SCRIPT="<s>:<W>x<H>,<s>:min,<s>:restore"`,
  offscreen: the output follows the window), macOS, Metal, frames before and after in `private/shots/uw14/`:
  SSA level_027 1280x720 -> a 20-step drag -> 2560x1080 -> minimised and restored -> 1920x1080 -> 3440x1440 ->
  1280x720 (each settled shape applied once, the drag once at its end; frame intervals 33 ms around every apply, no
  fault); Giants' tutorial 1280x720 -> 2560x1080 -> 3440x1440 -> 1280x720; Swap Force started 2560x1080 -> 1280x720 ->
  3440x1440, with `original_resolution=1` 1280x720 -> 2560x1080 -> 1280x720, and at 1280x720 full resolution (stays
  16:9, the log line); Trap Team (`level_314_tutorial_2`) started 2560x1080 -> 1280x720 -> 3440x1440 and with
  `original_resolution=1` 1280x720 -> 2560x1080 -> 1280x720. In each, the wide frames show more world at the sides
  with round things round, the HUD at its 16:9 place. SuperChargers and Imaginators were not run (their camera hook reads
  the aspect at each call, as Swap Force's) [?]; Windows and Linux windows, a real drag and a real fullscreen toggle are
  untested [?] (the offscreen output resizes, not a swap chain).
  Second pass (2026-11-12, `SKY_DISPLAY_SIZE=2560x1080`, a 1280x720 window maximised to 2560x1080, against a 2560x1080
  start, frames at the same moments in `private/shots/uw14b/`): every game makes the start's scene of a 2560x1080 window
  (SSA 1664x704; Swap Force, Trap Team, SuperChargers, Imaginators 1696x720; Giants 1000x600, its window's), shapes right
  in the 16:9 window and once widened. Mean horizontal luminance step of the picture's upper half (higher is sharper), SSA
  2.83 widened against 2.81 started wide (2.56 with the start's 16:9 scene stretched, the first pass); Trap Team 3.29 and
  3.29, Imaginators 3.34 and 3.36. SSA with FSR 3 (`SKY_UPSCALER=fsr3`): `FSR 3.1 upscaler 1664x704 -> 1664x704`,
  a 20-step drag settled once, minimised and restored, 3440x1440, back to 1280x720, no fault. `aspect=16:9` with the wide
  display: the console's scene (no texture widened). Unit test: `ArkeyanUltrawideTest` (`SceneShape`, a 16:9 start on a
  wide display, the width bound, Giants' window shape).

## Narrower than 16:9: fill (issue #24) (2026-11-16)

On a window narrower than 16:9 (16:10: the Steam Deck's 1280x800, most laptops) the frame stays 1280x720 and the
present letterboxes it: 40-pixel bars at 1280x800. With `aspect=fill` the present scales the game's frame up to cover
the window instead and lets its sides go past the window's edges: `gpu/present_rect.h` (`presentrect::Fit`), used by
`gpu::Present` for the game's frame and for frame generation's generated one (`gpu/gpu.cpp`, `FrameViewport`); the
viewport starts left of the output (negative x), the full-output scissor clips it (checked on Metal with API
validation; D3D12 and Vulkan allow a viewport past the target's edges by their specs, not run here [?]).

- **How much is cut**: `scale = (outW / w) x (w / h) / max(outW / outH, 16:10)`, centred, offsets rounded down. On
  16:10 the height is covered and about 5 % of the width goes on each side (1280x720 in 1280x800: 1422x800 at -71,0).
  The cut stops at a 16:10 screen's: narrower (4:3, 5:4, portrait) gets that crop with thinner bars above and below
  (1024x768: 1138x640 at -57,64). An output at least as wide as the frame (less than 1 % narrower: the wide frame's
  rounded width) keeps the letterbox, so a wider window takes the ultrawide path unchanged; an image no wider than
  16:10 (a 4:3 movie presented on its own) too.
- **The HUD stays whole**: the cut would take a corner HUD piece with it (Trap Team's save icon lost about a fifth of
  itself), so the HUD is drawn as much smaller as the frame is cut, towards the frame's centre
  (`gpu::presentrect::HudInset`: 0.9 on 16:10). It ends where the letterboxed HUD would be, whole, over a 3D scene
  that covers the window. The frame is the colour target whose copy the last swap presented (`d3d/render.cpp`,
  `g_presentedTarget`). Its draws split as in the wide frame (`ultrawide::FillsFrame`): the composite and the passes
  that draw the frame back over itself cover it; so does, with fill only, any screen-covering pass of the composite's
  shape that reads a resolve (`g_frameTargetRead`: Trap Team's title draws its 3D backdrop through passes that read
  smaller copies). Every other draw is the HUD. Movies are filled, a Trap Team corner piece (`d3d/frame_anchor.h`)
  stays in its corner. The scale follows the window like the layout (`ultrawide::SetFill`, `HudInset`, applied at the
  swap after the size settled).
- **A full-resolution scene** (1280x720: Swap Force and the later games) shares the frame's EDRAM surface, so the
  renderer cannot tell its draws from the HUD's: with fill it is made one 32-pixel tile wider (1312x720, `Configure`,
  `[ultrawide] window WxH: fill, scene 1312x720`), the state of a game started on a wider display (the camera stays
  16:9, the composite fits the scene to the frame). About 2.5 % more scene pixels.
- **Full-screen 2D**: a quad over the whole frame drawn with `game::UltrawideHudVertexShader` (Trap Team's fades and
  full-screen pages: the start-up legal page fading in, the black behind a level's loading picture) is placed `Whole`
  (`d3d/frame_anchor.h`) and covers the frame at full size; drawn smaller, it left the scene undimmed in 40-pixel bands
  above and below. The other games have no HUD shader set (`0`): a fade or a menu drawn all in 2D there would be drawn
  smaller with the HUD, bands above and below [?] (none seen in the SSA and Giants runs). The host overlay (Toy Box,
  F3 menu) stays letterboxed.
- **Log**: `[present] fill: 1280x720 in 1280x800 -> 1422x800 at -71,0`, once per change of the frame's or the output's
  size.
- **Not done**: a taller view (Vert+, the camera showing more above and below instead of the sides cut). It needs a
  frame taller than 720 (`d3d/render.cpp`, `FrameShift` is horizontal), taller scene targets in every game, and a
  vertical field-of-view hook in SSA, Giants and Swap Force (their cameras keep the vertical field).
- Tests: `ArkeyanPresentRectTest` (the rectangles, the HUD's scale), `ArkeyanUltrawideTest` (the scale follows the
  window), `ArkeyanRecompGpuTest` (`fill`, `fill_4x3`, `fill_wide`: a pattern presented and read back against a CPU
  reference), `ArkeyanSettingsTest` (`aspect=fill`). In game [V] (2026-11-16, Metal, offscreen 1280x800, the
  black rows of the presented picture counted, `auto` 40 at the top and the bottom): Trap Team (Europe disc) from the
  logos through the menus and the opening movie to the tutorial with Gusto, SSA (level_027, Gill Grunt), Giants
  (level_203_the_machine, Jet-Vac): 0 black rows, the HUD (portrait, health, coins) whole at the letterbox's place;
  29 fps in `fill` and `auto` (limit 30). Swap Force, SuperChargers and Imaginators not run [?].

## What changes

| Piece | 16:9 (the console) | Wide window (aspect A) |
|---|---|---|
| Camera | aspect 16:9 | aspect A, vertical field of view kept (so horizontal widens: culling follows) |
| Scene targets | 1120x704, half 560x352, quarter 280x176 and 288x176, eighth 144x96 | width = 704 x A rounded to 32 (2560x1080: 1664; 3440x1440: 1696), half, quarter, eighth of it |
| Sun tile grid | 35 x 22 cells of 32 pixels | ceil(width / 32) x 22 |
| Frame (back buffer, its depth, the copy that is swapped) | 1280x720 | 720 x A wide (2560x1080: 1707; 3440x1440: 1720), same height |
| Composite (scene + bloom + colour LUT) | fills the 1280x720 frame | fills the wide frame |
| HUD, menus, dialogue, movies | drawn into the frame | drawn at their own 16:9 layout, centred (the host moves their viewport and scissor by half the extra width) |

Host render targets are `scale` times these (`resolution_scale`), as before.

## How it is hooked in SSA [Lux] (`docs/giants/ssa_hooks.json` lists them)

- **Camera.** `sub_827F56A0` divides the video mode's width by its height and stores the display's aspect at
  `0x82BB45EC`. The renderer's state copies it and every camera takes its aspect from there and widens its horizontal
  field of view with it (tan(h/2) = tan(v/2) x aspect: the game is Hor+ already), so the hook writes the window's aspect
  there after the original returns. There is no perspective builder to hook, unlike in the later games.
  The UI's screen width and half width (`0x82BB45DC`, `0x82BB45E4`: 1280 and 640) are left alone: that is what keeps the
  HUD at its 16:9 layout.
- **Scene targets.** Textures are made by `sub_829611D8` (r5 width, r6 height; usage 0x80 also makes the same-size
  depth): its hook widens exactly the sizes above. One more 1120x704 header goes through `sub_823B38E0` (called from
  `sub_82929EE8`, return address `0x82929FCC`). The sun's light manager (`sub_828924B8`) keeps the scene width at +488
  and the grid's columns at +496, read by the sun tile passes and the CPU tile classification job; they are set after
  the setup returns (it reads neither afterwards).
- **The frame** (`d3d/render.cpp`, no game address involved except the shape): a target of 1280x720 guest pixels is made
  `FrameWidth()` wide (`TargetFor`), and so is the texture its resolve fills (`Resolve`), which is what the swap
  presents. The composite is the one non-indexed three-vertex draw into the frame that reads a scene-sized texture (the
  pass is `LUT(saturate(scene + bloom x k))`, a screen triangle): its viewport and scissor cover the whole frame, and
  its `VposParams` (pixel constant c0, which turns the pixel position into texture coordinates for 1280 pixels) are
  scaled by 1280 / frame width. Every other draw into the frame gets its viewport and scissor moved right by
  `(frame width - 1280) / 2`; screen-space draws (viewport transform off) still convert guest pixels with 1280. Clears of
  the guest's whole frame clear the whole wide frame; smaller ones are moved like the draws.
- **The pause menu's backdrop (2026-11-18, launcher #170) [V].** The pause menu (Objectives and the pages under it too)
  draws the paused scene through a 3D sheet in front of the camera: pixel shader `game::UltrawideBackdropPixelShader`, a
  refraction that projects each pixel's bent view ray with a fixed orthographic map (`mViewToScreen`, 3/1280 across) and
  falls back to the unbent pixel (`VposParams`, pixel constant c12, set for 1280 pixels). The sheet is laid out for a
  16:9 view and projected by the widened camera, so it covered only the middle 16:9; centred like the HUD it was smaller
  still, its fallback read past the copy's right edge (smeared streaks) and the sides kept the scene of the last frame
  (a house drawn twice). It now fills the frame (`ultrawide::FillsFrame`, `backdrop`), its position matrix's x is
  widened back to the window's aspect and its `VposParams` scaled by 1280 / frame width (`ultrawide::FitBackdrop`). The
  menu's dimming that follows it, a quad of the HUD's vertex shader (`game::UltrawideBackdropDimVertexShader`, position
  matrix in c8-c11) over the whole 16:9 frame (`d3d/frame_anchor.h`, `Whole`), fills the frame in a frame that drew the
  backdrop. Checked offscreen on Metal in level_027: a 2560x1080 start and a 16:9 start widened to 2560x1351, the paused
  picture as at 16:9 and no step at the 16:9 edges (column brightness 109.7 / 110.4 across the left edge, 66.6 / 66.1
  across the right; 168 / 110 and 66 / 101 before).

## Giants [Alchemy] [Giants] [V] (2026-10-26)

| Piece | 16:9 | Wide window (aspect A) |
|---|---|---|
| Camera | the display's aspect 16:9 (`0x826565C0`) | A |
| Scene | 1000x600 (2x MSAA), halves 500x300, 250x150, 125x75 | **the same pixel count**, at most 1280 wide: 2560x1080 gives 1184x496, 3440x1440 1184x488 |
| Frame | 1280x720, two predicated tiles (0-864, 864-1280) | 720 x A wide; each tile's resolve lands at its wide position |
| HUD, menus, text | | centred at 16:9, as in SSA |
| Movies (Bink 2) | | centred at 16:9 with **black bars**: while a movie is open the scene behind it stays 16:9 and the sides are cleared |

How it was found: a diagnostic build that logs every guest function storing the 16:9 float or writing chosen addresses
(`-include` a header that overrides XenonRecomp's `PPC_STORE_U32`, `ppc_context.h` guards it with `#ifndef`; local
only, `private/`), after a memory scan of a level had found the camera fields (aspect, near 0.2, far, field of view) and
the render manager's size chain 1000, 600, 500, 300, 250, 150, 125, 75. Recipe: [debugging.md](../../porting/debugging.md).
- **Camera.** `sub_8219B900` reads the video mode (`XGetVideoMode`), keeps the UI's screen size (1280x720 floats at +180,
  +184 of the display object: untouched, so the HUD keeps its layout) and stores the display's aspect at `0x826565C0`.
  Every camera reads it when it builds its projection (`sub_82198DB0` each frame for the level's cameras, `sub_8218E2A8`,
  the helpers `sub_821864D0`, `sub_82186CF8`, `sub_82186F90`): the hook writes the window's aspect there after the
  original, as SSA's. Hor+ (the vertical field of view stays).
- **Scene.** `sub_8218ED90` returns min(back buffer width, 1000) and `sub_8218EDD8` min(height, 600); `sub_82196600`
  derives the size chain from them, `sub_8219EC20` and `sub_8219F328` the passes. `sub_821939E0` and `sub_8219E0A8` cap
  the size inline and pass it to `sub_82196150` and `sub_8219EC20` (r4, r5): those two are hooked to receive the wide size.
- **Why the scene keeps its pixel count.** A 1408x600 scene (600 x A) does not fit the 10 MB of EDRAM with 2x MSAA:
  the game then renders it in two predicated tiles (1024 + 384), and here the left tile came out black; the game's
  viewports are also capped at the back buffer's 1280. Keeping 1000 x 600 pixels at the window's shape, at most 1280
  wide, avoids both (`game::UltrawideSceneKeepsArea`). The host scale (`resolution_scale`) still multiplies it: at scale 2,
  2368x992 for a 2560x1080 window.
- **Composite.** One non-indexed quad in pixels (vertex constant c0 = 2/1280) reading the scene from slot 0: given the
  whole wide viewport it fills the frame as it is (no `VposParams` rewrite, unlike SSA). Only slot 0 counts: the HUD's
  quads keep stale scene-sized textures in other slots (`game::UltrawideCompositeSlot`).
- **Menu blur (2026-11-14, launcher #78).** The menus blur the finished frame and draw it back: a quad of the
  composite's shape reads the frame (slot 0, frame-sized) into the guest's whole viewport, once to copy it, once to mix
  it with its blurred 1000x600 copy. Centred like the HUD, that pass squeezed the wide frame into the middle 16:9 between
  black sides on every frame it ran, and the menu jumped between the two pictures (a 2560x1367 window, frame 1348x720).
  It fills the frame as the composite does (`ultrawide::FillsFrame`: the composite's shape, reading the scene, or reading
  the frame over the whole guest frame); during a movie it stays centred.
- **Tiled frame.** The frame's two resolves (0-864, 864-1280) become 0 to the centred 864 and the centred 864 to the right
  edge, each copied to the same place of the wide front buffer.
- **Movies.** `BinkOpen` / `BinkClose` (`sub_82155B08`) count the open movies (`ultrawide::MoviePlaying`); during one the
  composite is centred like the HUD and the frame is cleared to black first.

Tests (macOS, Metal, offscreen, `SKY_WINDOW_SIZE`): 2560x1080 and 3440x1440 from the title to Chapter 1 (`private/ga`
shots): the level fills the window, more of the world at the sides, the HUD at its 16:9 place, no new `[stub]`, fault or
`[guard]`, 55.7 fps at 3440x1440 under a load average of 33; the movies (logos, `fmv01`) at 16:9 with black bars; menus
centred. 1280x720 unchanged (the harness).

Limits (Giants): the HUD's own screen-edge shading sits at the 16:9 edges (a visible step in the sky at the top, at the
16:9 border); the legal screen and the dimmed dialogs leave the 3D scene undimmed at the sides; the scene has fewer
pixels per degree than a 16:9 one (1184x496 instead of 1000x600, before the host scale); 32:9 gives a 1280x360 scene.

## Swap Force [Alchemy] [SwapForce] [V] (2026-10-27)

Same shape as Giants (a scene at the window's shape, the frame `720 x A` wide, HUD and menus centred at 16:9, movies with black
bars), found again: Giants' hooks do not exist in this engine version (shape similarity under 0.3).

| Piece | 16:9 | Wide window (aspect A) |
|---|---|---|
| Camera | the viewport object's aspect 16:9 (`+8`) | A, for the world camera's set-up only (`sub_82B992A8`) |
| Scene (default, full resolution) | **1280x720** (the chain's root pass `sub_8258EBE8` asked for the frame's size; halves 640x360, 320x180, reductions 128x720) | 720 high at the window's shape, a multiple of 32 wide: 2560x1080 gives **1696x720** (`SetScene16x9(1280, 720, keepsArea false)`: no EDRAM budget to keep, the game's tiling and EDRAM plan are handled by the hooks, [boot.md](../../../games/swapforce/docs/boot.md#the-scene-at-full-resolution-2026-10-27-alchemy-swapforce-v)) |
| Scene (`original_resolution=1`) | 1040x620 (the console's: the root pass's explicit size), halves 520x310, 260x155, reductions 104x620 | **the same pixel count** at the window's shape, at most 1280 wide, a multiple of 16 by 8: 2560x1080 and 3440x1440 give 1232x512 (children follow their parent's size) |
| Composite | one **indexed** quad, 6 indices, a 16 x 9 world-unit rectangle seen by a 16:9 camera | fills the wide frame (`GAME_ULTRAWIDE_COMPOSITE_INDEXED` in `addresses.h`; Giants' is 4 vertices, not indexed) |
| HUD, menus, text, movies | | as Giants |

Details and how it was found: [boot.md](../../../games/swapforce/docs/boot.md) ("Ultrawide, frame rate, glyphs, texture
packs, icon, saves"). Two traps: the quad and the HUD cameras read the same aspect field as the world camera, so it is set
for the world camera's call only (set for good, the composite filled 74 % of the frame's height); the scene's pixel count is
kept for the EDRAM, as in Giants.

Full resolution (2026-10-27, `SKY_WINDOW_SIZE=2560x1080`, the tutorial) [V]: `[ultrawide] window 2560x1080: aspect 2.370, scene
1696x720, frame 1707x720`, `[scene] full resolution: scene 1040x620 -> 1696x720`, `[scene] 1696x720 targets kept in one piece`;
the level and the cutscenes fill the window, the HUD at its 16:9 place; the HUD mask limit below is the same (a dark shape
left of the portrait).

Tests (macOS, Metal, offscreen, `SKY_WINDOW_SIZE`, the tutorial, `original_resolution=1` since 2026-10-27): `[ultrawide] window 2560x1080: aspect 2.370, scene 1232x512,
frame 1707x720`, `camera aspect 1.778 -> 2.370`, `scene 1040x620 -> 1232x512`; the level fills the window with more world at
the sides (no stretching: Eruptor and the ship keep their shape), the HUD at its 16:9 place; 3440x1440 the same (aspect 2.389);
the logos and the opening movie `fmv01` at 16:9 with black bars, the menus centred; 1280x720 unchanged (194 draws at
1040x620 in a frame capture, no `[ultrawide]` line). Limit: the HUD's dark stencil mask, drawn into the scene, is not centred and shows as a black
shape left of the portrait. Windows and Linux are untested.

## Trap Team [Alchemy] [TrapTeam] [V] (2026-10-31)

Swap Force's engine again, with its full-resolution scene (`game::Adapter::fullScene` = 1280x720, `original_resolution=1` keeps the game's 1040x620): the scene is 1280x720 at 16:9 and 720 high at the window's
shape when wide (2560x1080: 1696x720; with `original_resolution` 1232x512, the 1040x620's pixel count); the frame is 1707x720. The composite is one non-indexed 3-vertex draw (vertex constant c0 = 2/1280, slot 0,
`UltrawideCompositeVertices = 3`).

| Piece | Hook |
|---|---|
| Scene size | `sub_8243F650` (a pass object: explicit size +32/+36, computed +92/+96) |
| No predicated tiling, the console's EDRAM plan | `sub_823EBCD0` (a 1280x720 G-buffer splits into 2 tiles; kept in one) and `sub_823EF930` (the plan, called from `sub_82443DF8`, counts the original size's tiles) |
| Camera | the projection builder `sub_823B1AF0`: the game gives a horizontal field of view (84.3 degrees) and derives the vertical one from the aspect, so the hook widens the horizontal one with the aspect (100.7 degrees at 2.37). Two callers feed the world camera: `sub_8243C700` (the target's shape) and `sub_8230F3D8` (a 16:9 global; its two calls get the window's aspect in the hook) |

The menus' vignette (2026-11-14, launcher issue 104) is four 200x320 corner pieces of the HUD's shader `3C49092050CA673F`
(a 171x171 gradient, quarter-turned): centred, they ended at the 16:9 edge with a step (8 grey levels at 2560x1080 above the
autosave notice); `game::UltrawideHudVertexShader` sends them to the window's corners (the step 0.9, the save slot menu and
the notice's other 34 draws unmoved, `SKY_DUMP_FRAME` viewports).

The same shader draws the full-screen effects (2026-11-19): the fades, the zoom as a villain is beaten (one quad growing
from 4172 to 12779 pixels across in five frames) and the flash into the capture screen (turning sheets 3000 to 4000
pixels across). They are `Whole` (`d3d/frame_anchor.h`); centred like the HUD, they were cut at the 16:9 edges and the
scene showed untouched at the sides. In a wide frame a `Whole` quad now gets the whole frame (viewport and scissor
times the frame's ratio to 1280) and its clip x a factor (`d3d::WholeScaleX`, applied to c0-c3's x): the least stretch
that still covers the frame, from 1 (a quad of the frame's size, a fade: stretched with it) to 1280 / frame width (a
quad reaching that far past the sides, the flash: its shape kept). Not during a movie. A textured full-screen page of
the frame's size (the start-up page) is stretched too.

The blue swirl after the zoom (2026-11-24, forge issue 136) is not a HUD draw: it is a 3D mesh (8 vertices, 12 indices,
vertex shader `02E5370C68A717D2`, the only draw of that shader, for about 300 frames) drawn into the scene in front of the
camera and sized to cover a 16:9 view. With the wider camera its hard sides showed, at 2.3 to 2.5:1 of a 2.8 scene.
`game::UltrawideSceneVeilVertexShader` names it and its clip x is stretched by the scene's ratio to 16:9
(`d3d::SceneVeilScaleX`, 1.575 at 2.8), so it reaches both sides; 16:9 is unchanged. Writing the wide field of view into
the camera (below) does not change it.

The camera object keeps the field of view it was given (+48, in degrees, before the zoom) and the aspect (+52) next to
the matrix the hook builds (r3 = camera + 176 at the first call, `0x8230F788`); the game reads them again for the
shadows, which stopped short of the sides on the way to the tutorial's first jump. The hook writes the wide values there
too; the camera's framing is unchanged (tutorial snapshots at 40 s, with and without).

The 16:9 aspect global (`0x82826308`) is left alone: the camera rig reads it, and a wide value pulls the camera back from the character. Details and the limits:
[games/trapteam/docs/status.md](../../../games/trapteam/docs/status.md) ("The picture", "The constant blur").

## SuperChargers [Alchemy] [SuperChargers] [V] (2026-11-04)

Swap Force's game side on Trap Team's engine build: the scene is the console's **1040x620** (the chain's root pass, run `scU1`), so the
adapter's `fullScene` is 1280x720 as both (`original_resolution=1` keeps 1040x620), and wide windows get a scene 720 high at their shape
(2560x1080: 1696x720) and a frame of 1707x720. Hooks (`games/superchargers/ultrawide.cpp`, addresses in `games/superchargers/addresses.h`):

| Piece | Hook |
|---|---|
| Scene size | `sub_826E8718`, Trap Team's pass object (explicit +32/+36, computed +92/+96); found as the only user of the targets' format names that Swap Force's `sub_8258EBE8` uses |
| No predicated tiling | `sub_82600C10` (Swap Force's and Trap Team's `Scene_Tiles`, shape 1.00). No EDRAM plan calls the tile count here (Trap Team's `Scene_SurfaceTiles` hook): not hooked |
| Camera | the projection builder `sub_8250E610` (Trap Team's `sub_823B1AF0`, shape 1.00), from the world camera's set-up `sub_82B0BC00` (Swap Force's `sub_82B992A8`, shape 0.62) only (return `0x82B0BCA4`): the aspect becomes the window's and the horizontal field of view grows (80.0 to 96.4 degrees at 2.37) |
| Composite | an indexed quad of 6 indices (`GAME_ULTRAWIDE_COMPOSITE_INDEXED`), Swap Force's |

Run `scU2` (a continue into the Academy, `SKY_WINDOW_SIZE=2560x1080`): `[ultrawide] window 2560x1080: aspect 2.370, scene 1696x720, frame
1707x720`, `[scene] full resolution: scene 1040x620 -> 1696x720`; the Academy fills the window, the HUD at its 16:9 place, the pause
menu centred at 16:9 (the sides grey), the movies at 16:9 with bars (run `scU1`).

**32:9 and the scene's width bound** (2026-11-05) [V]: with a 3840x1080 window the full-resolution scene was 2560x720 and the game
stopped as it entered the Academy, at scale 2 and at scale 1 (runs `scU3`, `scU7`, `scU8`): no frame after, one guest thread deep
in a recursive walk (`sub_8283F448`, a `sample` of the process), the others waiting. Not the field of view (capped at 100 degrees it
still stopped, run `scU9`), not the tiling (`Scene_Tiles` is not called); with `original_resolution` (a 1280x360 scene) it played
(run `scU10`). So `game::Adapter::maxSceneWidth` (runtime, 0 by default) bounds a wide full-resolution scene: past it the scene keeps
the window's shape at a smaller height (`ultrawide::SetScene16x9`, `ArkeyanUltrawideTest`). SuperChargers' is 2048: at 32:9 the scene is
2048x576 (fewer pixels than 21:9's 1696x720), the frame 2560x720; run `scU11` enters the Academy and plays on, the world filling the
window, the HUD centred, Hor+ 118.4 degrees. Which size the game cannot take (the width or the pixel count) was not narrowed [?].

## Imaginators [Alchemy] [Imaginators] [V] (2026-11-05)

SuperChargers' hooks on Imaginators' addresses: `games/imaginators/ultrawide.cpp` includes `games/superchargers/ultrawide.cpp`, whose
one literal, the world camera's return address, is now `game::UltrawideWorldCameraCall` in each `addresses.h`. Found with
`tools/xmatch.py ../scday1:superchargers imaginators shape`: the scene pass `sub_826F96D0` (0.87; `offsets` shows no structure
displacement changed), `Scene_Tiles` `sub_82614310` (1.00), the world camera `sub_82BA8430` (1.00), whose call at the same place goes to
the projection builder `sub_82547C30` (the frustum code inlined there, so shape 0.30; the same arguments; return `0x82BA84D4`). The
scene is again 1040x620; `maxSceneWidth` is SuperChargers' 2048 (not tried wider here [?]).
Runs (`SKY_LEVEL=hub_2016`): `imgB1` at 2560x1080, `scene 1040x620 -> 1696x720`, Hor+ 80.0 -> 96.4 degrees, the hub filling the
window, the HUD at its 16:9 place, the pause menu centred at 16:9 (grey sides, as SuperChargers'); `imgC1` at 3840x1080, scene 2048x576,
frame 2560x720, 118.4 degrees, plays on in the hub.

## Tests (2026-10-25, macOS, M-series, Metal, level_027 by the fast recipe, offscreen)

- `SKY_WINDOW_SIZE=2560x1080` and `3440x1440`: frames are 2560x1080 and 3440x1440; no black column at either side
  (left and right 5 % of the picture: 0 black pixels); the health bar of the HUD measures 176.5 x 26.5 guest pixels
  at 238.5 in both (16:9: 177 x 27 at 238), so the HUD is neither stretched nor moved; no new `[stub]`, `[guard]`,
  `[heap]`, fault or missing shader against a 16:9 run (the same 18 stubs).
- 1280x720: with `Active()` false nothing differs from `main`; the best match of 66 frames of each run (the idle camera
  turns, frames cannot be paired by index) has a mean absolute difference of 0.017 on the title screen.
- Speed at 30 FPS (the game's pace), median work per frame: scale 1: 14.1 ms at 1280x720, 25.4 ms at 2560x1080;
  scale 2: 17.0 ms and 31.0 ms (the 33 ms budget). Draws per frame 1786 and 2766: the wider view draws more objects.
- Shots: `private/shots/ultrawide_2560x1080_level.png`, `ultrawide_3440x1440_level.png`, and the 16:9 reference.

## Limits

- The scene's targets keep the start's size, made for the window's display: a window widened past that display's shape
  (or moved to a wider monitor)
  shows a stretched (softer) scene until the next start; Giants (and `original_resolution=1`) keep the window's shape; a
  full-resolution scene started at 16:9 with no wider display does not widen live at all (above). A real display's shape
  is untested (the runs set `SKY_DISPLAY_SIZE`) [?].
- Menus (title, slots) and anything the game draws to fill the 16:9 frame stay 16:9 (SSA's pause menu fills it, above):
  the sides are black (menus) or show the 3D scene undimmed (anything dimming over it). Movies keep 16:9 with bars.
  A HUD piece in a corner of the 16:9 frame goes to that corner of the window instead (`d3d/frame_anchor.h`): the draws
  of `game::UltrawideHudVertexShader` (Trap Team's; `0` in the other games, not looked for) are placed from their
  vertices and the position matrix in c0-c3; a piece that reaches one side's corner moves to that side, the rest (a
  full-width draw included) stays centred.
- The HUD is not anchored to the window's edges: the game lays it out for 1280x720 and the host centres that.
  Anchoring to the edges would mean giving the UI a wider screen (`0x82BB45DC`), which brings parked off-screen notices
  into view.
- The sun's light tile pass has more points (the grid grows with the width): more GPU work at very wide windows.
- Tested on Metal at 2560x1080 and 3440x1440 only (the pause menu at 2560x1080 and 2560x1351); Windows and Linux are untested.
- Aspects above 4:1 are clamped to 4:1.
