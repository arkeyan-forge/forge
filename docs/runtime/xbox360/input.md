# Input

> **Purpose:** how does pad and keyboard input reach the title (XAM input, key map, scripted presses, glyphs)? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy]

Code: [`kernel/input.cpp`](../../../runtime/kernel/input.cpp) (XAM), [`host/input.{h,cpp}`](../../../runtime/host/input.cpp)
(state + scripted presses), [`main.cpp`](../../../runtime/main.cpp) (SDL reading), [`host/glyphs.cpp`](../../../runtime/host/glyphs.cpp),
[`host/uwp/main.cpp`](../../../runtime/host/uwp/main.cpp) (Windows.Gaming.Input). Class **[Xbox]** (XInput model); glyphs **[Lux]** and **[Alchemy]**.

## XAM side

- **Users 0 and 1 have a device** (`host/input.cpp` keeps a state, a packet counter and a script per user). User 0 is the
  keyboard and the first pad; **user 1 (Player 2) exists only while it has a device**: a second SDL gamepad (any pad but
  the first; `main.cpp`: when the first pad goes away the second becomes user 0), or a `SKY_PRESS2` script. `XamInputGetCapabilities(user, flags,
  X_INPUT_CAPABILITIES*)` / `XamInputGetState(user, flags, X_INPUT_STATE*)` return `ERROR_DEVICE_NOT_CONNECTED` (0x48F) for
  any other user, and for user 1 without a device. [V] with `SKY_PRESS2` on Giants (a Player 2 joins, a second figure is placed and both play); a real second
  pad is untried (no second pad here). Not done: Windows.Gaming.Input on Xbox UWP (`host/uwp/main.cpp` feeds user 0 only),
  the Toy Box filter for pad 2, the keyboard for Player 2.
- Capabilities (0x14 bytes): `+0` type 1 (gamepad), `+1` subtype 1, `+4` a gamepad block with every input present
  (buttons `0xF3FF`, triggers 0xFF, sticks -1), `+0x10`/`+0x12` vibration 0xFFFF.
- `X_INPUT_STATE`: `+0` packet number, `+4` `XINPUT_GAMEPAD`: u16 buttons, u8 left/right trigger, s16 thumbLX/LY/RX/RY.
  Button masks: DpadUp 1, Down 2, Left 4, Right 8, Start 0x10, Back 0x20, LeftThumb 0x40, RightThumb 0x80, LB 0x100, RB 0x200,
  A 0x1000, B 0x2000, X 0x4000, Y 0x8000.
- **Packet number** = the host's counter that changes whenever the state does; scripted presses add `0x10000` so they differ from host-only states.
- `XamInputSetState` (vibration, `user`, `flags`, `X_INPUT_VIBRATION*`: two u16 motors) returns success and keeps the request (`SetHostVibration`); the host's main loop sends it to that user's pad, scaled by the player's rumble setting, below.
- The portal uses ordinals 0x4A1/0x4A2 of the same module ([lux/portal_title_side.md](../../engines/lux/portal_title_side.md)).

## Host side

- `main.cpp` reads the keyboard and the SDL gamepad each loop: keyboard actions in `g_keyActions` order `a b x y start back lb rb lt rt`,
  left stick (4 keys), right stick (4 keys), d-pad (4 keys). Defaults: **a Space, b X, x C, y V, start Return, back Backspace, lb Q, rb E,
  lt 1, rt 3, left stick W S A D, right stick I K J L, d-pad arrows**; Escape is always Back as well. Gamepad: SDL south/east/west/north =
  A/B/X/Y; sticks and triggers (`axis >> 7`); the keyboard's stick keys win over the pad's when non-zero.
- Settings: `[input] device = auto|keyboard|gamepad`, `gamepad = <GUID>` (replaces another pad when plugged in later),
  `key_<action>`; `--list-gamepads` prints `GUID  name`.
- **The Toy Box filter**: while the Toy Box or its wheel is open `ToyboxFilterGamepad(state)` removes what they use from the title's
  pad state and gives a *tap* of View back to the title when released within 0.25 s. After closing, input stays off until buttons
  are released **and sticks/triggers are within a dead zone** (sticks past 8000 and triggers past 30 count as held): SDL and
  Windows.Gaming.Input apply no dead zone, so waiting for exactly 0 kept the title deaf forever (fixed).
- **Toy Box keys**: `gamemenu::Init` applies the parsed `[input] key_toybox`, `key_toybox_add`, `key_toybox_clear` through
  `ToyboxSetKeys`; F3 > Commands applies them again on each change. Defaults are F6/F7/F8, with Tab as a second open key:
  tap for the Toy Box, hold 0.25 s for the wheel, release to choose. An explicit `key_toybox` transfers Tab's tap/hold
  behaviour to that key. Open/close, repeat suppression and release all use the same scancode; the portal event watch
  reads the configured add/clear scancodes. `ArkeyanCommandsTest` checks defaults, saved keys, Tab, reset and hit testing.
  Swap notices identify right-stick directions in English and French even when the page abbreviates their action as “Same”.

The menu mouse script uses canvas coordinates (1280×720): `SKY_TOYBOX_SCRIPT="F3:30,move=640/172:31,click=640/172:32"`.
`move=x/y` hovers without changing keyboard focus; `click=x/y` and `rclick=x/y` click; `wheel=up|down` scrolls. Each logs
the selected row/card and `hover menu=<row>`, `hover commands=<row>/<column>`, `hover card=<index>` or `hover none` from the
actual topmost drawn hit. Hover and click share hit testing; a dialog backdrop blocks the content beneath it.
[`tools/regress/f3mouse.sh`](../../../tools/regress/f3mouse.sh) `<SSA disc>` automatically wraps the whole run in
`../gamelock.sh`, uses scratch settings/figures/saves, and ends with PASS or FAIL. The game and its timer close fd 3;
the timer signals only the captured game PID, escalates to KILL and is reaped on exit. No process-name search is used.
A real run must show an outline over the FPS row and the fourth Toy Box card without moving their keyboard selection,
then select them by click; enable the mouse and rebind B's mouse cell on Commands; save and apply the bindings, scroll
the cards, write all nine screenshots and leave no game process. Also check a saved custom open key by tapping it and
holding it for the wheel, use the saved add/clear keys, restart to confirm persistence, then Reset to restore Tab/F6/F7/F8.
Rebind a right-stick key to an occupied key and check the toast names the other action; an unused or unchanged key has no swap toast.
Run the host startup/pointer unit checks with `python3 -I tools/regress/f3mouse_unit.py` and timer checks with
`bash tools/regress/pid_timer_test.sh`; both also run in `tools/test_all.sh --quick`, without starting a game.

## Mouse [Xbox] (2026-11-02, all four games)

Code: `host/mouse.{h,cpp}` (the names, the bindings over the game's defaults, the capture's state machine; no SDL),
`host/mouse_input.{h,cpp}` (the SDL side: events, SDL's relative mouse mode, the actions held), `main.cpp` (`ReadHostGamepad` takes the
mouse's actions for Player 1 beside the keyboard's), `game::Adapter::mouseDefaults` and `menuLevels` (each game's adapter),
`game::InMenuLevel` (`game.cpp`). Test: `ArkeyanMouseTest` (names, bindings, every capture transition below), `ArkeyanSettingsTest` (the lines).

- **Off by default.** `[<game>.input] mouse=1` turns it on ([settings.md](../../getting-started/settings.md)). Off, or nothing mapped:
  `mouseinput` makes no SDL call, `ReadHostGamepad` gets `nullptr` and runs the path it ran before; the pointer is never hidden.
- **Bindings.** One button (or wheel direction) per action, any action (movement included): `mouse_<action>`. With no line, the game's
  attack 1 is on the left button and attack 2 on the right (SSA `a`/`x`, Giants `a`/`x`, Swap Force and Trap Team `x`/`b`). A mouse
  action is held like its key: buttons while pressed, a wheel tick for 120 ms. Moving the mouse does nothing. Player 1 only.
- **Capture** (`mouse::Capture`): the pointer is hidden and kept in the window (`SDL_SetWindowRelativeMouseMode`) only while all hold:
  a mouse action is mapped, the window has the focus and is not minimized, no F3 menu, Toy Box or wheel is open (`ToyboxOverlayOpen`),
  and the level is not one of the game's own menus (`menuLevels`: SSA `frontend`, Giants and Trap Team `title` (`\level\<name>.arc`,
  `game::LevelArcOfFile`), Swap Force `ui\mainmenubackground\mainmenubackground` (its zone lookup); before the first level, the logos and the title, counts as a menu). A loss of focus
  (Alt-Tab, Cmd-Tab, a dialog) or a minimized window frees it until a click in the window takes it back; **that click, and any button
  still held when the pointer is taken, never reach the game** (released first). Closing the F3 menu or the Toy Box takes it back at
  once. Quitting frees it before anything else. A crash ends the process: the system frees the pointer. The log says
  `[mouse] on: a=Mouse Left, x=Mouse Right`, `[mouse] pointer held by the game` / `free`.
- **What the runtime cannot tell**: the menus a game draws inside a level (pause, shops, dialogs) look like play: the pointer stays
  hidden there and a mapped click does its action (left click is Accept in SSA's and Giants' menus). The game-menu cursor is for 0.8.1.
- **Test aid**: `SKY_PRESS` items `mleft`, `mright`, `mmiddle`, `m4`, `m5` (`mleft:20` clicks at 20 s, `mleft:20:1` holds 1 s) and
  `wheelup`, `wheeldown` (one tick) go through the same bindings and capture; a script stands for a focused window. Each logs
  `[mouse] script: Mouse Left (held pointer: the game's)` or `(free pointer: not the game's)`; `SKY_INPUT_LOG=1` shows what the title read.
- **By hand only** (no real mouse or keyboard in tests): Alt-Tab and Cmd-Tab while playing, a minimized window, quitting with the pointer
  held, a crash, the click that takes it back.

## Remapping and rumble [Xbox] [V] (2026-10-28, all three games)

Settings (docs/getting-started/settings.md): `[<game>.input] pad_<button>=<button>|none` and `rumble_p1` / `rumble_p2`
(`off`, `low`, `normal`, `strong`), read from `[input]` then the game's own section, which wins. The launcher writes them on
the game's settings page, Controller tab ([settings.md](../../getting-started/settings.md); the launcher's own docs).
- **Remapping** (`host/pad_map.cpp`, `RemapPad`; applied in `main.cpp`'s `ReadHostGamepad` to the **real pad's** state only,
  before it is merged with the keyboard's, so the Keyboard tab's keys keep their meaning and the Toy Box, which filters the
  merged state, sees the pad as the player holds it). Every button that is remapped is first taken out of the state, then
  each pressed one adds the button it now stands for: `rb=b` gives B for RB and no RB; `a=b b=a` is a swap; two buttons on
  one are one B; `none` removes a button; LT and RT count as pressed past 30 and a button that becomes a trigger pulls it to
  255; the sticks' axes are never remapped. The button names are `a b x y lb rb lt rt start back ls rs dpad_up dpad_down
  dpad_left dpad_right`. Unit tests: `ArkeyanSettingsTest` (parse, precedence, bad values, every case above).
- **Rumble**: `XamInputSetState` stores the two motors per user; the main loop sends them to the user's SDL gamepad
  (`SDL_RumbleGamepad`, 1 s, sent again every 0.5 s while it runs, 0/0 stops it) after `ScaleRumble`: off 0, low 40 %,
  normal as asked, strong 160 % (capped at 65535). Player 1 is the first pad, Player 2 the second. The log says
  `[input] rumble player 1: the game asked 40000/20000, strong: 64000/32000` at each change. Whether the title ever asks is
  its own business (`SKY_INPUT_LOG=1` below shows what it reads, not what it sets).
- **The test**: [`tools/regress/pad_qol.sh`](../../../tools/regress/pad_qol.sh) `<ssa|giants|swapforce> <disc>` (about 1 min a
  game): a virtual Xbox 360 pad (`SKY_GLYPHS_SCRIPT` actions `btn:<name>` / `btnup:<name>` hold and release a button,
  `vibrate` / `vibrate0` make the title ask Player 1's pad to rumble, and the virtual pad's rumble callback logs what
  reached it) holds RB, LT and D-pad up while `SKY_INPUT_LOG=1` logs what the title reads
  (`[input] user 0: the title reads buttons 0x2000, triggers 0/0`). With `pad_rb=b`, `pad_lt=a`, `pad_dpad_up=rt` and
  `rumble_p1=strong` the title reads 0x2000, 0x1000 and trigger 255, never RB's 0x0200 nor 0x0001, and the pad rumbles
  64000/32000; with no map it reads 0x0200, trigger 255 and 0x0001 and 40000/20000. It runs in `tools/test_all.sh`.

## SKY_PRESS script (test aid)

`SKY_PRESS="start:6,a:9,lup:20:2.5"`: `<button>:<seconds since launch>[:<hold seconds>]`, 150 ms presses unless a hold is given.
Buttons: `start back a b x y up down left right` (d-pad); `lb rb` the shoulders, `lt rt` the triggers (full), `lup ldown lleft lright` push the **left stick** all the way. Each menu takes the first press it accepts, so a slow screen only shifts the step.
`SKY_PRESS_FILE=<file>` reads lines appended to a file while the game runs as presses made at once (`a`, `lup:2`, `2 start` for pad 2): an
unattended run steered from outside by looking at the frames it writes ([`tools/regress/live_run.sh`](../../../tools/regress/live_run.sh)).
With it, the first controller is connected from the start [V] (2026-10-27): Swap Force's title ignored every press
when no pad existed before the file's first line (a pad that appears later is not taken by the title).
`SKY_PRESS2` is the same script for the second controller (user 1, Player 2): setting it makes that controller exist.
**The menus belong to the first pad that presses a button at the title** [V] (2026-10-28, SSA): with `SKY_PRESS2="a:3"` and no press for pad 1, Player 2's
pad drove CHOOSE PLAY and Battle Mode, and pad 1's `down` and `a` did nothing; steer with `2 <button>` lines ([`tools/regress/ssa_arena.sh`](../../../tools/regress/ssa_arena.sh)).
Recipes: [testing.md](../../getting-started/testing.md).

## Glyphs (the button pictures the game shows) [Xbox] [Lux] [Alchemy]

`host/glyphs.cpp` replaces the game's Xbox 360 button pictures **by the XXH3 content hash of the texture's base level as
the renderer reads it** (tiled, big-endian), in `TextureFor`, with pictures it draws (signed-distance shapes, Bungee for
letters, never Sony/Nintendo/Microsoft artwork). **One style per pad family** (2026-10-28; every glyph of every style on
one picture: `SKY_GLYPHS_SHEET=<file.png>`, a test aid that writes it at start-up with the bindings in use):
- `xbox360`: the game's own pictures; where we add a picture the game lacks (Swap Force's Start), a silver round button
  with a play arrow (START) or a back arrow (BACK), as on the 360 pad;
- `xboxone` (default on Xbox): dark round buttons with coloured letters, Menu = three lines for START, View = two windows
  for BACK, LB RB LT RT, LS/RS;
- `playstation` (PS4 / PS5): cross/circle/square/triangle by position, OPTIONS for START, **the touchpad** for BACK (the
  runtime reads the touchpad's click as BACK too, besides Share / Create: `main.cpp`), L1 R1 L2 R2, L3/R3;
- `playstation3` (a PS3 pad; not a setting value): the same shapes, START and SELECT;
- `nintendo` (Switch): letters **by position** (the 360's A, the south button, is the Switch's B; B is A, X is Y, Y is X),
  + and -, L R ZL ZR, LS/RS;
- `keyboard`: keycaps of the bound keys (a stick shows its four keys, "WASD"), each key named with the letter the
  player's layout prints at the bound place (the bindings are scancodes: [settings.md](../../getting-started/settings.md)).

The **keys are the game's own**: the runtime reads `[input]` then the game's `[<game>.input]` (Arkeyan Launcher's per-game
copy, which wins), so each game's keycaps show its own bindings [V] (2026-10-28: `key_y=G` for Giants only, the Mash! coin
reads G in Giants and SSA's keycaps keep V; `key_a=G` for Giants: "Press G to begin"; `key_start=Tab` for Swap Force:
"PRESS TAB"). The log says `[glyphs] keycaps: a=... b=... x=... y=... start=... back=...` at start-up.

`auto` follows the last device used (a key press -> keyboard; a pad's family from `SDL_GetGamepadType`: PS4/PS5 ->
PlayStation, PS3 -> PlayStation 3, Switch Pro/Joy-Cons -> Nintendo, Xbox One/Series -> Xbox One, anything else Xbox 360).
The change shows on the next frame (each texture is checked once a frame).

**Keyboard-only glyphs** (`Glyph::Controller`, `ArrowsLeftRight`, `ArrowsUpDown`): pictures that suit every pad as they are
but not the keyboard; on a pad style the game's picture stays.

Each game lists its glyph textures in its adapter (`game::Adapter::glyphTextures`, `games/<id>/adapter.cpp`), in two kinds:
- **whole textures** (the texture is one button): drawn at the texture's size, with its mips;
- **atlas cells** (the texture is a font atlas and only some cells are buttons): the guest's base level is decoded (DXT1 or
  DXT5, `GuestBaseRgba` in `d3d/render.cpp`), each cell (its picture's bounding box, plus one pixel) is cleared and
  redrawn, the rest (the letters) is the game's. The result is an RGBA8 texture of the same size.
A texture stored upside down (`flipped`) is drawn upside down too. **Models** (`glyphs::Model`, 2026-11-02): a 3D prompt
whose button is letters of geometry on a textured card (Trap Team's RT): its cells are drawn over the game's pixels
(`over`), the letters' indexed draw (index count + XXH3 of its indices) is skipped, and the card's shaders get the
identity for the texture matrix they spin (a vertex constant), all only while a style other than the 360's is in use
(`HidesDraw`, `UvMatrix`, called by `d3d/render.cpp`; [glyphs.md](../../../games/trapteam/docs/glyphs.md#the-world-prompts-2026-11-02)); `IssueDraw` calls `HidesDraw` first, before the `Prompt` pairing: it was dropped by mistake in 0.9.0 and the 360's "RT" showed over the keycap, [glyphs.md](../../../games/trapteam/docs/glyphs.md#the-rt-card-showed-the-keycap-and-the-360s-letters-together-090-fixed-2026-11-06)).
**World prompts** (`glyphs::Prompt`, `game::Adapter::glyphPrompts`, 2026-11-05): the 3D prompts of the later Alchemy engine
(`models/UI/ButtonPrompt/XENON`, SuperChargers and Imaginators) are two meshes, a flat **face** (a disc, a pill) and the
button's **letters** of geometry on it, both coloured by a flat 4x4 map and a tint, so no texture holds the button. Each
prompt lists the face's indexed draw (index count and XXH3 of the indices as stored) and the letters', and a picture (a
`Texture` row with `plate`: the glyph at 4/5 of a dark opaque plate, drawn at the size the prompt gives). The order is the game's
and differs: SuperChargers draws the face, then its letters, in both passes of the RB prompt; Imaginators the same in its depth pass and the
letters first in its colour pass (measured: the first renderer that held only a face left a plain white pill there). So a draw whose
indices are a listed face or letters (`PromptPartOf`) is built with CPU copies of its streams and **held for one draw** (the held draw owns
copies of its streams, indices and constants, and is issued before any other draw, clear, resolve or the present); when the next draw is
the other part of a listed prompt (`PromptOf`: the face's and the letters' hashes name the prompt, so that A and Y, which share a face, are
told apart) the face is issued with the picture as sampler 0's texture instead of the map and its TEXCOORD0 rewritten from its
positions (`PlanarUv`: SNORM16 x y z and FLOAT16_2 pairs, the bounding box of the draw's indices covers the picture; `uAxis`, `vAxis` and the
flips orient it, `uScale` undoes the stretch the game's shader applies to u), and the letters are dropped; any other next draw issues the
held draw as the game drew it, first. A prompt's draws repeat in the depth pass and the colour pass; both are treated
alike. A face whose layout is not those formats stays the game's, with one
log line. The Xbox 360 style changes nothing. Unit test: `ArkeyanGlyphModelsTest`. Per game: SuperChargers
([art.md](../../../games/superchargers/docs/art.md), "World prompts"), Imaginators
([art.md](../../../games/imaginators/docs/art.md), "World prompts").

**SSA [Lux]**: every glyph is a whole texture (128x128 DXT5, the stick 256x256 DXT1) loaded from the `.str` packages
(`APKF` images) and drawn as a quad next to the text: `5CFC101A45FC493F` a_mark, `955A4B4B799FF8E9` b_mark,
`37ACF5999A3F9F99`/`16DB4319FA0899CB` x/y, `6B77FA0C8847038E`/`43493F9409303C0A` hud a/b (pause menu), `629C5B2704B8047E`
START pill, `E217909DBA45F2A0` right stick, and (2026-10-28, `uigameimage.str`) `B20E0561D39D1F43` back_mark (the BACK pill
of `&ICON(0,ICON_DETAILS)`, "Skylander Info") and `24D8176CD8936388` 360_rsmark ("RS" of `&ICON(0,ICON_RS)`, "Unjoin
Game"; drawn as the right-stick icon: the keyboard has no right-stick click, its keycap shows the camera keys) [?] (hashes
from the disc by the renderer's method, which gives back every other SSA hash; not seen in a run). Text buttons are the
markup `&ICON(0,ICON_<name>)` (ENTER A, UNDO B, HATK X, SP Y, RL arrow_rl, UD arrow_ud, RS 360_rsmark, DETAILS
back_mark): the CRC-32 of the name is a key of `text/global_alllang.lxb`, whose text is the image's name in
`uigameimage.str`; no icon exists for START, the shoulders or the left stick [V]. 360_rsmark was seen drawn as a keycap
(Player 2's "Unjoin Game", 2026-10-28 [V]); back_mark is a HUD hint not reached in a run [?]. The keyboard (2026-10-28):
- **the Controls page** (Options > Controls): its pad `360_controller` (`E27708D85EE5F3B7`, 256x256) is drawn as a plain
  pad shape with a keycap of the bound key where each button is (centres measured on the disc's texture), so the
  page's leader lines (`controller_360line`, another texture) point at keycaps [V] (shot);
- **the blue arrows** `arrow_rl` (`7BBEBB5A5444F7C2`, 64x32) and `arrow_ud` (`D6AA804FD58B90A5`, 32x32) are drawn as the
  left stick's keys (A D, W S) [V] (`arrow_ud drawn as keyboard` in a menu);
- **the two hints that name the stick** (no string says START or BACK by word; scan of every `text/<lang>/*.lxb`):
  level_008's boat (`A0DDCEC4`) and level_025's War Machine (`7A811D43`), in ten languages. `games/ssa/stick_text.cpp`
  hooks the text lookup (`Text_Lookup` `sub_82861060`, r5 the key) and its compile (`Text_Compile` `sub_82860FC8`, r3 the raw
  text): on the keyboard style the language's noun for the stick (a short word list, no game text) becomes
  `&ICON(0,ICON_RL)` (boat) or `&ICON(0,ICON_RL)&ICON(0,ICON_UD)` (aim), drawn as the keys. [?] the two scenes were not
  reached in a run (the hook runs on every text and the texts read right); "Move right stick" (`4AFF071A`) is used by
  nothing found. The disc also holds unused `ps3_*` and `pc_*_mark` textures.

**Giants [Alchemy] [V]** (2026-10-26, coin 2026-10-27): the buttons are **characters of the UI fonts**: each font atlas (DXT5, one level)
has, after the letters, A, X, Y, B, the four d-pad arrows (kept: they suit every device), LS, RS, and the START (play
arrow) and BACK (back arrow) pills. Atlases: `AD2E31ADEC576354` (512x512, the prompts' font: "New Game", "Back",
"Accept"), `462EB82E5F1D8851` (1024x1024, the large font); their cells are in `games/giants/adapter.cpp`. Whole
textures: `F4C85169D542FC77`, a 64x64 DXT5 green A ("Press A to begin" on the title), stored upside down;
`E88F26B298A077B7`, a 64x64 DXT1 **gold Y coin** floating above a rock to smash (a world prompt in the tutorial: it only
shows when a Giant is on the portal; stored upside down too), drawn as the style's Y (the coin's gold is replaced by the
style's button); and (2026-10-28) the **large coins of the world prompts' model** (`ButtonIcons_HD_360`, driven by
`UI_Controller_Prompt.ai` in 79 levels), 128x128 DXT1, upside down: `6DE08CB96DD4DD23` the Y of the **"Mash!"** prompt
when a Giant lifts the rock (the Y the user still saw on the keyboard in 0.6.0) and `7FAE9AE5883D1AB9` its A [V]; and the
giant robot's RT keycap `179828095390F2C2` (86x85, [?] not seen). A third atlas, `1F11BD3F9CB5136B` (1024x1024), has **Wii** buttons (A, B, Z, C, the nunchuk, +, -):
loaded, never shown on the 360, left alone. Every inventory (what was searched, what stays): [art.md](../../../games/giants/docs/art.md), "Button pictures".

**Swap Force [Alchemy] [SwapForce] [V]** (2026-10-27): the game draws every string into small textures it fills on the CPU
(the HUD's "Press (X) to Attack", the menus' "Select" / "Back"), so the GPU never samples a font atlas or a button. The
buttons come from **two `igImage2` named "button"** in the fonts (`fonts\proximanova-body.igz`, 256x83, and
`-info.igz`, 256x168; 32-bit ARGB, tiled, big-endian; A, B, X, Y, LB, RB, LT, RT, LS, RS side by side), which the CPU
renderer copies into each string. They are loaded into physical memory and stay there, so `glyphs::MemoryImage`
(adapter `glyphImages`, `host/glyphs.cpp`) **redraws them in place**: a thread looks for each image in the guest heaps
by the XXH3 of its first 4096 bytes (the pixels start on a 4 KB boundary; every 2 s until found, `[glyphs] <name> found
in guest memory at <address>`), keeps the game's bytes, and at each style change redraws the cells over them (the
Xbox 360 style puts the game's bytes back; `[glyphs] <name> (guest memory) drawn as <style>`). Strings the game
composes afterwards carry the new buttons, so a switch shows at the next prompt or menu, not on a string already on
screen. The shoulder buttons are drawn too (`Glyph::LeftBumper`.. `RightTrigger`: LB/RB/LT/RT, L1/R1/L2/R2, L/R/ZL/ZR,
or the bound keys on keycaps). The 3D prompts that float in the world (`models\ui\buttonprompt\xenon\a`, `b`, `x`,
`y`, `mash_*`, `b_hold`, `lt`, `rt`, `rs_*`) are **meshes** coloured by 4x4 colour maps (`ColorMap_Yellow_Y`...): the
letters are geometry, not a texture, and stay the 360's [V]. One real texture, `juicestandard_turbo_a_10` (128x128, a
green A of the Turbo race HUD, hash `0758AAD79A30A2C8`), is in `GlyphTextures` unseen in game [?]. **Words** (2026-10-28): every in-text button is a font character
of the font's alternate list (`ʘ` A, `^` B, `~` X, `` ` `` Y, `[ ]` LB RB, `< >` LT RT, `{ }` LS RS) drawn from those images,
except the title's "PRESS START" / "APPUYER SUR START" (font proximanova-info), and the font has no Start. Now **START is a
picture** in every style [V] (shots: ENTER keycap, OPTIONS, START, +, Menu, the 360's ▶; live on a device switch):
`engines/alchemy/prompt_text.cpp` (shared with SuperChargers and Imaginators since 2026-11-06) interns the text with its last word START replaced by U+0001 (`String_Intern`
`sub_823EC0D8`), and answers the font's character lookup (`Font_CharMetrics` `sub_82553E48`: binary search of the main list,
then the alternate one; nothing cached) for U+0001 in the info font with metrics of ours: a copy of the A's moved to an
empty part of the button image (x 199-245, y 0-55), where the adapter's added Start cell is drawn in every style (the
Xbox 360 style too, since the game has none). The log: `START shown as the Start button's picture`, `the info font's Start
character made at <address>`.
Inventory: [art.md](../../../games/swapforce/docs/art.md), "Button pictures".

**Audit, Giants against Swap Force and Trap Team** (2026-10-31; "follows" = the style's picture or the bound keycap; [V] seen in a capture, [?] not seen on a screen):

| Where | Giants | Swap Force | Trap Team |
|---|---|---|---|
| Title ("Press A", "PRESS START") | follows [V] | START picture follows [V] | the title's A follows [V] |
| Menu hints ("Select" / "Back", "Accept" / "Back") | follows [V] | follows [V], and the rebound `key_a=G`, `key_b=H` [V] | follows [V], and the rebound G, H [V]; PS5 cross and circle after a pad switch [V] |
| In-text A X Y B | font cells [V] | font images [V] | atlas cells redrawn; a sentence of the tutorial not reached [?] |
| LB, RB | the game has none | images redrawn (keycaps of `key_lb`/`key_rb`) [?] | the game has none: no picture, no text |
| LT, RT | an RT keycap in the robot HUD [?] | images redrawn [?] | atlas cells redrawn [?]; the world RT [V] |
| Sticks, Start, Back in text | cells [?] | sticks: images [?]; Start: picture [V]; no Back | cells [?] |
| World prompts in a level | gold Y and A coins follow [V] | meshes, the letters are geometry: stay the 360's [V] | the RT card follows (a `Model`) [V]; mash flipbooks, LT, coins listed [?] |
| Controller diagrams (`controls_*.png`) | stay the 360's | stay the 360's | stay the 360's |
| Skip prompt on movies | not looked for [?] | not looked for [?] | none in the title and tutorial text |

Swap Force's and Trap Team's default keycaps are the runtime's own table (`g_keyActions`: A Space, B X, X C, Y V, Start Return,
Back Backspace, LB Q, RB E, LT 1, RT 3) and the launcher's table for both (`game_keys.cpp`, one table for the two games) is the
same. Trap Team's real button roles are not checked against it: the tutorial's text says `[` is "jump", `]` "Primary attack".

**Finding a new game's glyphs** (about 30 minutes): run the game with `SKY_TEXTURE_DUMP=1 SKY_TEXTURE_DUMP_LOG=1`
through the title and the menus. Each dumped file is logged with its guest hash: `[texpack] dumped <file>: guest hash
<hash>, WxH` (the dump's file name is a hash of the decoded pixels, not the one glyphs.cpp matches). Look at the small
textures and the font atlases (a contact sheet of the `.dds` files), note the button ones, and for an atlas measure
each button's bounding box from the alpha channel (connected components below the last row of letters). Check the
orientation on a shot: Giants' A is stored upside down.

**When the dump has no atlas** (Swap Force): the game composes its strings on the CPU. Look in the font files (`igBitmapFont`:
one `igImage2` for the letters and, here, one named "button"), decode them (tiled, big-endian), note each button's box,
find the image in guest memory (hash of its first 4096 bytes; a probe of 64 of its bytes over the heaps helped: the
physical heap is reached through `PhysicalToGuest`) and redraw it in place (`MemoryImage`). The hash of a texture found
in an archive is the XXH3 of its **storage** (a tiled 64x64 DXT5 is 16 KB, not 4 KB), as the renderer reads it.

**Tests**: `SKY_GLYPHS_SCRIPT` (test aid, [testing.md](../../getting-started/testing.md)) plugs virtual SDL gamepads
(`ps5`, `ps4`, `ps3`, `switch`, `xboxone`, `xbox360`) and sends key presses at given times, so `auto` switches through the
real event path. The log says `[glyphs] <style>` at each switch and `[glyphs] <texture> drawn as <style>` (the first
64 draws). `tools/test_all.sh` plugs a PS5 pad into its Giants run and checks those lines, into a Giants tutorial run that walks to the rock (`y_coin 64 drawn as playstation`), plugs a Switch,
an Xbox One, a PS3 and an Xbox 360 pad by the rock (`drawn as nintendo`, `xboxone`, `playstation3`), then sends a key press and lifts the
rock (`y_coin 128 drawn as keyboard`: the "Mash!" prompt) with Giants' own Y key (`keycaps: ... y=G`, a `[giants.input]`
binding the SSA run must not see: its `y=V`), and into its Swap Force run (`(guest memory) drawn as playstation`; the title's
`START shown as the Start button's picture`).
