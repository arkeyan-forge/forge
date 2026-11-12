# Trap Team's button glyphs

> **Purpose:** which pictures show a button in Trap Team, how they are found on the disc, the cells the adapter redraws, and what still shows the 360's picture · **Status:** current (2026-11-03) · **Tags:** [Xbox] [Alchemy] [TrapTeam] [V] [?]

## What the game has
| Picture | Where | Status |
|---|---|---|
| **Colour atlas** `fonts\360\font_001_rgb.png`, 512x256, 16 bits a pixel (format hash `1CB93819`, tiled, with mips) | `permanent\bootstrap.bld`; guest hash (XXH3 of the 262,144-byte base level) `67DFCE1E73302C9F` | **redrawn** [V]: A X Y B, LT RT, LS RS, Start, Back (cells in `games/trapteam/adapter.cpp`); the pause menu's "Accept" / "Back" hints follow the device |
| The same atlas: four arrows, an envelope, the Start and Back pills' inner arrows | | stay the 360's (no `Glyph` kind for them) |
| **The title's A**, a whole picture, 64x64 DXT5, stored upside down, guest hash `A3E90BC0BE57F49A` | `level\title.bld` (the title's text is three strings, "Press", the picture, "to begin") | **redrawn** [V] (`GlyphTextures`, as Giants' `a_button`); the first guess, that it came from the distance-field atlas, was wrong |
| **Distance-field atlas** `fonts\360\font_001.png`, 1024x1024 (`1CB93819`), guest hash `2949820980389064` | `permanent\bootstrap.bld` | the letters (blue channel) and shapes without letters (arrows, circles, bubbles, pills); not a button picture, left alone |
| Controller diagrams `ui_main\sprites\controls\sky2\controls_{xb1,ps4,ps3,360,wiiu_all,wii}.png`, 955x331 | `level\title.arc`, `misc\ui_options.arc` | not replaced |
| **The world RT prompt** (above a cannon, a chest, a lever): model `Button_360_RT`, a card textured by a grey gradient (128x128 DXT1, upside down, guest hash `0AB48F4714BAD3F3`), "RT" in letters of geometry | `permanent\global.bld` | **replaced** [V] (2026-11-02): the glyph drawn on the card, the letters hidden; see "The world prompts" |
| World flipbooks `UI_SKY4/Icons/ButtonPrompts/360/{A_Mash,RT_Mash,X_Mash}` (14 frames each, 85x85 DXT5, upside down; "A/RT/X button flipbook sprites") | `permanent\global.bld` | in `GlyphTextures`, each frame drawn as the same glyph; [?] not seen in a run (mash prompts) |
| **The villain gauge's LT** (52x52, 32 bits a pixel, `1D8F0223BF4D7422`; the only LT picture of `global.bld`, which names `ui_sky4/hud/png/format_8888/360/hud_swapbutton.png`) | `permanent\global.bld` | **redrawn** [V] (2026-11-03), see "The villain gauge" |
| Large A and Y coins (model `ButtonIcons_HD_360`, 128x128 DXT1 with mips, `8446988D31491B4D`, `6DE08CB96DD4DD23`: the Y is Giants' own picture) | `permanent\global.bld` | in `GlyphTextures`; [?] not seen in a run |

## How they were found (about 2 h)
1. **Archives.** IGA v11 is read by `runtime/host/art/alchemy_archive.cpp`; **IGZ v8 was not** (2026-10-30: it needs the tags byte-swapped as v7, the sections at 0x14, and the types' names padded to 2 bytes; no `TSTR`). Fixed, the whole disc lists: 62,727 pictures in 1,916 archives. A texture's name is the archive entry's source path only in an `.arc` (`levels/includes/ui_main/...png`); in a `.bld` the one entry is `level.bld` and its pictures have no name (`fonts/360/font_001*.png` come from the `igImage2` of `bootstrap.bld`).
2. **Guest hash.** XXH3 of the **base level only**, `width * height * bytes a pixel` for a linear size: the hash of the whole storage (mips included) matches nothing. A tutorial run with `SKY_TEXTURE_DUMP=1 SKY_TEXTURE_DUMP_LOG=1` dumps 446 textures; 104 hashes are on the disc by the whole-storage hash, 2 more (the two font atlases) by the base level.
3. **Cells.** Measured on the dumped DDS (RGBA8, alpha 255 everywhere in the dump: non-white pixels, 8-neighbour components).
4. `GuestBaseRgba` (`runtime/d3d/render.cpp`) read DXT1 and DXT5 only: the 16-bit formats (1_5_5_5, 5_6_5, 4_4_4_4) are expanded as the renderer uploads them, so the atlas now reads (without it: `[glyphs] font rgb atlas 512x256: its format cannot be read here`). Other games' atlases are DXT: unchanged.

## Where the text uses a button
The English string tables (472 distinct of the 653 `ENGLISH.pak`, UTF-16BE, 2026-10-30) spell a button as **one character** in the text: `]` (271 "Press/Hold ... to"), `{` (232), `}` (213), `[` (8), `<` (9, "Press < to Continue"), `&` (6, "Use & to guide the Lock Master") and `_` (2, "Press _ to Invert Up/Down Controls") [V]: seven codes, the colour atlas's A X Y B, the triggers and the sticks; which code is which button was not read [?], but the context says: `[` "Press [ to jump" (A), `]` "Press ] for Primary attack", `}` "} Show Help" and "Use ] and } to defeat monsters", `<` the title and menus ("< Continue Game", "< New Game", "< Next", "Press < to Start") [V] (title and tutorial string tables, 2026-10-31). No text names a bumper, an arrow or the envelope ("shoulder" is an ability's name; "PRESS D-PAD UP TO BEGIN" is a demo screen's plain words). So the arrows, the envelope and LB/RB are **left as the game's**: no prompt uses them [V] for the text; the in-world prompts were not reached.

## Cells (base level, inclusive)
A 1,1-80,80 · X 88,1-167,80 · Y 175,1-254,80 · B 261,1-340,79 · LT 349,0-426,77 · RS 1,163-84,242 · LS 92,163-174,243 · Start 182,178-258,232 · Back 266,178-342,232 · RT 349,161-425,237. No LB or RB in the atlas, and no text asks for one.

## Captures [V]
Isolated, offscreen, `SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:450 SKY_LEVEL=level_313_tutorial`, A every 4 s to 76 s, Start at 90 s (the pause menu), `SKY_GLYPHS_SCRIPT=ps5:97,switch:107,xboxone:117,ps3:127,xbox360:137`, `SKY_GPU_SNAPSHOT_SECONDS=94,102,112,122,132,142`. Frames (local, not in git): `private/runs/tg5/snap_t<s>.ppm`.

| Frame | Style | The "Accept" / "Back" hints |
|---|---|---|
| `t94` | keyboard (no pad yet) | keycaps |
| `t102` | PlayStation (PS5 pad) | cross, circle |
| `t112` | Nintendo (Switch pad) | letters by position (B, A) |
| `t122` | Xbox One | the Xbox One buttons |
| `t132` | PlayStation 3 | PS3 shapes |
| `t142` | Xbox 360 | the game's own green A and red B |

The log has `[glyphs] font rgb atlas 512x256 drawn as <style>` for each. The five hint frames are darker than the 360's: the menu was fading in at 94-132 s, the 360 frame is 10 s older [?] (not checked at the same age).

## The title [V]
`SKY_GLYPHS_SCRIPT=ps5:34,switch:44,xboxone:56,ps3:68,key:80`, no presses, `SKY_GPU_SNAPSHOT_SECONDS=26,40,50,62,74,86` (local frames `private/runs/tg6/snap_t<s>.ppm`, run through `../gamelock.sh`): "Press (A) to begin" shows, in turn, the keycap (SPACE, 26 s), the PlayStation cross (40 s), the Switch's B (50 s), the Xbox One A (62 s), the PS3 cross (74 s) and the keycap again (86 s), all upright. The log has `title a_button 64 drawn as <style>` each time.

## The launcher's keys [V] (2026-10-31)
`[trapteam.input]` with `key_a=G key_b=H key_x=F key_y=B key_start=Tab key_lb=R key_rb=Y key_lt=U key_rt=O` in `SKY_SETTINGS`: the log says `keycaps: a=G b=H x=F y=B start=Tab back=Bksp`, the difficulty screen's hint reads "G Accept" and the pause's "G Accept, H Back" (keycaps); `SKY_GLYPHS_SCRIPT=ps5:100,key:115`: the cross and circle at 108 s, the keycaps again at 122 s. Frames (local) `private/runs/tgk1/t{26,94,108,122}.png`. The defaults are the runtime's and the launcher's shared Swap Force table. LB, RB, Start and the sticks were not on a screen.

## The world prompts (2026-11-02)
**Found.** The owner's shot: an "RT" card above a chest in the tutorial ("Press 3 to fire cannon" was already a keycap).
A scan of every picture of every archive by the renderer's hash (a local tool over `alchemy_archive.cpp`: 62,727
pictures; the hashes it gives back match the texture dumps of a run) and a contact sheet of `global.bld`'s small
pictures found the flipbooks and coins above; the names come from the strings of `global.bld` (`Button_360_RT.mdl`,
`360 Gesture Sprite RT 000`, `UI_SKY4/Icons/ButtonPrompts/360/RT_Mash/RT_Mash_0000.png`). The RT card above the
cannon is none of them: a probe of the draws showed a model, two draws a frame on one card mesh (768 indices,
vertex shaders `B1F0E0D50B9B5BEF` and `7C35D7F64961D2DF`) whose texture is the gradient, and a third (639 indices,
the letters "RT", extruded) [V].

**How it is replaced** (`glyphs::Model`, `runtime/host/glyphs.h`; the adapter's `RtCardModel`). The card's face maps
the gradient flat (u 0.22-0.78, v 0.09-0.91, read from its vertices), turned by a texture matrix that spins (vertex
constants c5-c8: a rotation about the centre, a sheen) [V]. While a style other than the 360's is in use: the glyph
(the RT keycap of the bound key, R2, ZR, RT) is drawn over the gradient in the face's cell (texels 28,12-99,116,
upside down); the renderer sets c5-c8 to the identity for the card's two shaders when the glyph is bound
(`glyphs::UvMatrix`), so the glyph stays upright and still; the letters' draw (639 indices, XXH3 of its indices
`66DB72A301169C73`) is skipped (`glyphs::HidesDraw`). The Xbox 360 style leaves all three as the game draws them.
Unit test: `ArkeyanGlyphModelsTest`.

**Captures** [V]: `SKY_PORTAL_PLACE=new:450`, the tutorial route of `tools/regress/trapteam_capture.route` (the cannon
at 157-169 s after the level opens, the first chest at about 306 s), a shot every 1.5 s (local: `private/glyphs3d-runs/proof/`).

| Run | The cannon's prompt |
|---|---|
| `before` (no change, keyboard) | the 360's grey card with "RT" |
| `key3` (keyboard, default keys) | a keycap "3" on the card, upright and still at 157-167 s |
| `ps5` (`SKY_GLYPHS_SCRIPT=ps5:5`: PlayStation) | a dark R2 pill on the card; the hint's R2 too |
| `chest` (keyboard, `[trapteam.input] key_rt=O`, to 322 s) | the cannon's card and hint read O; at 306 s the **chest's** card (the owner's scene, "Each Chapter contains 3 hidden treasure chests") reads O |
## The RT card showed the keycap and the 360's letters together (0.9.0, fixed 2026-11-06) [V]
**What the owner saw.** On 0.9.0 with the keyboard style, the cannon's card showed the keycap "3" and, under it, the
extruded "RT". **Cause [V]:** `1441c5f` (2026-11-05, world prompts pair face and letters, for SuperChargers and
Imaginators) replaced the renderer's `glyphs::HidesDraw` call in `IssueDraw` (`runtime/d3d/render.cpp`) by the pairing,
so the 639-index letters of `RtCardModel` were drawn again; nothing in `ArkeyanGlyphModelsTest` called the renderer, so
it stayed green. **Fix:** the call is back, before the pairing (only a `Model` texture's letters; a `Prompt` is still told by
its pair). **Proof:** the tutorial's cannon (`tools/regress/trapteam_capture.route`, keyboard, a shot every 2 s; local
`private/runs/before1` and `after2`): the 0.9.1 binary draws "3" over "RT", the fixed build draws the "3" alone, same
moment. The log of the card's draws in the fixed run: the card's face (768 indices, hash `148CE04D4F1F9E49`) in five
shader pairs, the letters never reaching the draw (hidden); no other index count samples the card's picture besides a
scene shader (`4A09A50DF47BAD6F`) that keeps the slot bound from the previous draw [?]. **Not found:** another copy of
the RT letters (levels' own `Button_360_RT`, other LODs) or an LT card: only the tutorial was reached, and its chest
uses the same hash; the face also has two passes by a vertex shader (`443541580AF981B0`) that `RtCardModel` does not list, so
its sheen matrix is not held still in them [?] (not seen on a frame). Unit test: a second model with other letters
(`ArkeyanGlyphModelsTest`: each hides only its own).

## The keycap's label on a French keyboard (2026-11-03)
**What the owner saw.** With the keyboard style, the RT keycap of the cannon's hint ("Press RT to fire cannon") showed two
vertical bars. **Cause [V]:** a keycap shows the bound key's name on the keyboard's layout (`SDL_GetKeyFromScancode`
without modifiers); RT's default key is the scancode `3`, and on the French layout that key types `"` without Shift, which
Bungee draws as two bars. A probe on this Mac (French layout, `SDL_GetKeyName` of each scancode, the cocoa video driver)
gives `1 -> &`, `2 -> é`, `3 -> "`, `4 -> '` ... `0 -> à`, and the digit with Shift; under `SDL_VIDEO_DRIVER=offscreen`
SDL keeps the US map (`3 -> 3`), which is why every scripted run showed "3". The same keycap is drawn for the 3D RT card
and for LT (default `1`: `&` on AZERTY). **Fix:** `glyphs::KeycapLabel` (`runtime/host/glyph_models.cpp`) shows the
digit a number-row key types with Shift when the layout types a symbol without it (the keycap's printed digit); other keys
are unchanged. Unit test: `ArkeyanGlyphModelsTest` (`"` with `3` gives `3`, US `3` stays, `Backspace` gives `Bksp`). Not
seen in a window [?]: our runs are offscreen; the owner checks it on the real layout (below).

## The villain gauge (2026-11-03) [V]
The gauge is the purple bar under the trap's hexagon, bottom centre. While the villain plays, a red timer dial sits left
of it (`DamageClock`); once the villain is back in the trap and the bar is refilling, the dial is replaced by the
**LT** button: `world lt 52`, the only LT picture of `global.bld` (its 43 pictures of 32 bits a pixel decoded, local tool
over `alchemy_archive.cpp`). It is first drawn about 8 s after the villain goes back (the log's `[glyphs] world lt 52
drawn as <style>`). Captures (a variant of `tools/regress/trapteam_villain.sh`: LT at +20 s and +28 s after the level
opens, a shot every 2 s from +30 to +90; local `private/runs/ttg_g2`, `ttg_p1`, crops in `private/ttglyphs-runs/`):
keyboard: a keycap "1" (the default `key_lt`); `SKY_GLYPHS_SCRIPT=ps5:5,switch:140`: an L2 pill, then ZL after the switch,
upright. So the 0.8.0 build (commit `7ab174b` listed the texture) already redraws it in the tutorial; what the owner saw is
not reproduced [?]: on the French layout it was the keycap `&` (fixed above), or another scene (another level's HUD,
two players) that these runs did not reach.

## What still shows the wrong picture
- The four arrows, the envelope (and no LB/RB exists): nothing in the text uses them, see above.
- The controller diagrams (`controls_*.png`, 955x331 photographs of a pad with leader lines; the "Game Paused" screen reached showed only Accept and Back, no Options page). Not replaced: the screen was not reached, so the guest hash of `controls_360` is unverified (the storage hash `044282E6C5B32129` is the disc's, not checked against the renderer's), and the keyboard version needs its own layout (a new `Glyph` kind and drawing path), which Giants does not have either.
- The world flipbooks and the coins are listed but were not seen in a run [?]; a level's own models were not
  probed (only `global.bld`'s prompts and the tutorial's cannon).
- **Not reached:** a prompt in a sentence of the tutorial ("Press X to ..."): no scripted play (A every 4 s, then the stick) met one in 190 s of `level_313_tutorial`; Start, Back and the sticks were not seen on a screen either (A and B in the pause menu and the title, RT on the cannon and its hint, LT on the villain gauge).
