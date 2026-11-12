# The in-game menu (F3), the problem report and where a crash happened

> **Purpose:** what the player gets on F3 (the side panel, the pages beside the rail: Settings, Achievements, Controls, the Toy Box; "Report a problem"), how those settings are saved and shown by Arkeyan Launcher, what is in the report file, and how a crash names the guest function · **Status:** current (2026-11-06, the 0.9.0 redesign) · **Tags:** [Xbox] [SSA] [Giants]

Code: `runtime/host/gamemenu.{h,cpp}` (what the rows change and save, the FPS text, the report file), `runtime/host/report.{h,cpp}`
(the report's text and its redaction), `runtime/host/toybox/menu.cpp` (the rows and their logic), `draw.cpp` (`DrawMenu`, `DrawSettings`,
`DrawAchievements`, `DrawCommands`, `DrawRail`, `DrawFps`), `toybox.cpp` (F3 and the pad chord), `runtime/thread.cpp` (`PrintGuestLocation`), `runtime/platform/platform_posix.cpp`
and `platform_win32.cpp` (the fault handlers), `runtime/host/commands.{h,cpp}` (the Commands page on launcher.ini) and each game's
rows in `games/<id>/adapter.cpp` (`game::Adapter::inputRows`). Arkeyan Launcher's side: its `docs/support.md`, `launcher/game_keys.cpp`.
Check: `tools/regress/f3menu.sh`, `tools/regress/f3commands.sh`, `ArkeyanCommandsTest`.

## The menu

Since 0.9.0 (2026-11-06, the owner's redesign "in the launcher's look"; the look: [ingame_ui.md](ingame_ui.md)) F3 opens a
**side panel** on the right of the screen, the game visible on its left, and the panel opens **full screens beside a rail**:
Toy Box, Achievements, Settings, Controls (the Commands page) and Performance (the debug screen over the game). Every
behaviour of the old single menu is kept: the same settings, keys, saved values, log lines and report.

| Open | Close |
|---|---|
| **F3** (released alone) | F3, Esc or B on the panel; F3 on any of its pages |
| **View + Start** on a pad (the chord does not clash: View alone is the wheel, View + Y the Toy Box; Start is kept from the game while View is down) | View + Start again, or B |

Inside the Toy Box F3 stays its "View" key (the search); the menu opens from the game, not from the Toy Box's screens. While
the menu is open no input reaches the game from that pad (as for the Toy Box and the wheel; the game keeps running behind it:
the runtime has no pause, and the Toy Box does not pause either).

**The panel** (`Screen::Menu`, `DrawMenu`, `Model::MainRows`): the game's name (Bungee, in its accent) with "Arkeyan ·
<version>" under it and the player's profile (`SKY_PLAYER_NAME` and its picture, else the initials; [profiles.md](profiles.md))
at the top, then:

| Row | What it does |
|---|---|
| Volume | the slider: Left and Right by 10, a click on the track sets it (`SetVolumeAt`); `[<game>.audio] volume` |
| Resume | closes the menu (the panel opens on it) |
| Toy Box | its page (the value: how many figures are on the portal) |
| Achievements | its page (the value: "6 / 38") |
| Settings | its page |
| Performance | the debug screen's level, Off, Compact, Full: Left / Right / A step it, round the ends; `debug_screen` |
| Report a problem | writes the file, below |
| Quit to the launcher | asks first ("What the game has not saved is lost."); Quit ends the game as closing its window does (an `SDL_EVENT_QUIT`: the play time is written, the launcher comes back) |

Up and Down move (round the ends), A or Enter does the row, B or Esc closes; the mouse: the pointer moving onto a row focuses
it, a click does it. Toasts show on the left of the panel, over the game.

**The pages beside the rail** (`Model::OpenPage`, `look::Page`): the rail on the left lists Toy Box, Achievements, Settings,
Controls and Performance (the page shown is lit in the accent; Performance has a dot while the debug screen is on), the
profile at its bottom; a click on a row opens it. **B** (Esc) on a page goes back to the panel when the panel opened it, else
closes (the Toy Box opened with F6 or View + Y closes, as before). **LB / RB** (Page Up / Page Down) go to the page before or
after among Achievements, Settings and Controls (`look::PageStep`), except where they already mean something: the Toy Box's
seats, Achievements' tabs. Performance turns the debug screen on (Compact) and goes back to the game to show it; on again, it
turns it off.

**Settings** (`Screen::Settings`, `DrawSettings`, `Model::SettingsRows`): one list in sections (Game, Video,
Audio, Debug; French Jeu, Vidéo, Audio, Débogage) as headings; the focused row shows its value as a stepper (the value between
two chevrons, its place among the values as small steps under it; the mouse steps with the chevrons' halves, a click elsewhere
does the row as A does). The right panel says what the focused setting does in one line, its cost to the graphics card (0 to 5
steps, for the rows where it matters: resolution scale, texture filtering, edge smoothing, frame rate) and "Applies at the next
start" for the starred ones. Up and Down move (round the ends; the list scrolls, `look::ListScroll`), Left and Right change, A
flips or moves on. "Saved as you change them": every change writes at once, as before.

| Section | Row | Applies | Saved in launcher.ini |
|---|---|---|---|
| Game | Commands | opens the Controls page, below: **at once** | `[<game>.input]` `key_<action>`, `pad_<button>` |
| Game | Skip the start-up logos * | at the next start | `skip_intro_movies` of `[<game>.video]` |
| Video | Fullscreen | at once (`SDL_SetWindowFullscreen`) | `[<game>.video] fullscreen` |
| Video | Frame rate (30, 60; 0.9.4: no limit above 60, an older one shows as 60, `gamemenu::NormalFrameRate`) | at once (`SetFrameRateLimit`) | `[<game>.video] fps_limit` |
| Video | Drop to 30 FPS when needed (only at 60) | at once (`SetFpsAutoFallback`) | `[<game>.video] fps_auto_fallback` |
| Video | Resolution scale *, Texture filtering * | at the next start | `resolution_scale`, `anisotropy` of `[<game>.video]` |
| Video | Upscaling (Off, FSR 3), FSR quality (Quality, Balanced, Performance, Ultra performance; shown while on) | **at once** (`upscale::Request`: the scales change at the next swap, the render targets are made again); Upscaling at the next start in a game whose scene was full size at the start ([upscaling.md](../gpu/upscaling.md)); with FSR on, Resolution scale reads "x1, FSR 3 to x2" | `upscaler`, `fsr_quality` of `[<game>.video]` |
| Video | Edge smoothing (Game, Gentle, None; only a game with a known FXAA pass) | **at once**; until one is chosen it shows what auto uses: Gentle in a level from scale 2, Game at scale 1 and on the title and menus | `edge_smoothing` of `[<game>.video]` |
| Video | Graphics API * (Direct3D 12, Vulkan; desktop Windows only), with `(now <the one running>)` when it differs from the choice (a change, or the fallback) | at the next start | `gpu_backend` of `[<game>.video]` |
| Video | VSync * | at the next start | `vsync` of `[<game>.video]` |
| Audio | Volume (0-100, steps of 10) | at once: a gain on every audio frame (`HostAudioSetVolume`) | `[<game>.audio] volume` |
| Audio | Portal sound (only a game whose portal has a speaker: Trap Team) | at once (`PortalSetSound`): the traps' voices, on the Traptanium portal or the computer ([portal.md](portal.md), "The speaker") | `[<game>.audio] portal_sound` (on by default) |
| Audio | Speakers (Auto, Stereo, 5.1; Auto shows the layout of this run, "Auto (5.1)") * | at the next start: the device is opened and the title told once ([audio_xma_xaudio.md](../xbox360/audio_xma_xaudio.md#51-the-speakers-setting)) | `[<game>.audio] speakers` (auto by default) |
| Debug | FPS counter | at once: a small readout, top right of the TV safe area (frames presented over the last half second, `gpu::FramesPresented`) | `[<game>.video] show_fps` |
| Debug | Debug screen (Off, Compact, Full) | at once, below; any level hides the separate FPS counter | `[<game>.video] debug_screen` (0, 1, 2; off by default) |
| Debug | Record a performance log | at once, below; not saved (this run only) | |

"*" rows apply at the next start (the right panel says so). **F3 opens the menu when it is released alone**; held with G, T,
C or R it is the debug screen's key instead (below); Shift+F3 cycles the debug screen Off, Compact, Full in play.

**Achievements** (`Screen::Achievements`, `DrawAchievements`): a ring with the share unlocked, "6 of 38 · 60 / 1000 G", the
tabs All / Unlocked / Locked (LB / RB, or a click), and the title's achievements as cards of four columns (its medal, its
points, its name and description in the game's text language, "Unlocked Oct 20" / "Débloqué le 20 oct." or "Locked"; a hidden
one still locked is "Secret achievement"). The list is the one the runtime answers the game with (`kernel/xam_achievements.cpp`
through `ToyboxSetAchievementList`: the title's XDBF list from the player's own default.xex and the unlocks in the game's
`achievements.txt`, [settings.md](../../getting-started/settings.md)); nothing is stored for the page. The arrows move the focus
in the grid (it scrolls by rows, `look::GridFirstRow`), the pointer moving onto a card focuses it.

- **Measured** (2026-11-02, every row on the four games, offscreen, through `SKY_TOYBOX_SCRIPT`): volume 50 halves the output
  (Giants' title, RMS 0.072 to 0.037 in the disk audio file); the debug screen draws its panels; fullscreen takes the display's size
  at once (1024x768 offscreen); 30 to 60 at once (`[perf]` 30.0 to 60.0); Edge smoothing at once (Trap Team's title, scale 1:
  Gentle +24 %, None +36 % of detail); every starred row read back at the next start in the `[settings]` line, with
  `[gpu] resolution scale 2`, `[movie] skipped <logo>` and `[gpu] sampler <n>: anisotropy 16`; every value in `[<game>.video]` /
  `[<game>.audio]`. Fixed then: Trap Team's "Skip the start-up logos" did nothing (its adapter had no logo list).
- **Master volume only.** The runtime mixes the game's audio into one stream before the host (`kernel/audio.cpp` →
  `HostAudioSubmit`); music and effects cannot be told apart, so there is no separate slider.
- English and French by the game's text language (`toybox::L`).
- The game's version (`games/<id>/game.txt`, given to `gamemenu::Init`) is under the game's name at the panel's top, "Arkeyan · 0.9.5".
- Every change is a log line (`[gamemenu] volume = 80`), so a report says what the player did. The settings file is read and
  written whole (`LoadLauncherConfig` / `SaveLauncherConfig`): every key is kept, comments are not (the file already is the
  launcher's, which does the same).
- **The launcher shows the same values.** The runtime reads `[<game>.video]` after `[video]` and lets it win
  ([settings.md](../../getting-started/settings.md)); the launcher's game page edits the same copy and has the **FPS counter**
  (`show_fps`, the eleventh per-game video key), **Volume** and, for Trap Team, **Portal sound** rows. Arkeyan Launcher no longer copies `[video]` back over
  `[<game>.video]` when the game ends (it would have undone what the menu saved).
- Xbox (UWP): `host/uwp/gamemenu_uwp.cpp` only keeps the values for the run (no settings file, no report); that target is paused
  and this stub is not built or run [?].

## Commands

One row per action of the game, in Arkeyan Launcher's order and words (its Keyboard tab, `launcher/game_keys.cpp`; the French from its
`lang/fr.txt`): each game lists its rows in `games/<id>/adapter.cpp` (`game::Adapter::inputRows`: the action, the English and French
label, the pad button whose job it is). Columns (`menu.cpp` `Column`, `Model::CommandHeaders`; `DrawCommands` lays out as many as
there are): the action, the **keyboard** key (the letter its place has on the keyboard's layout, the named keys in French when the
game's text is), the **pad** button (Xbox 360 names; the button or buttons that do the row's job now), the **mouse** button
("Left click", "Wheel up"...; "-": none). Below the actions: the **Mouse** switch (On / Off), the Toy Box's three keys (**Open the
Toy Box**, **Add a figure**, **Clear the figures**: keyboard only, every game), **Reset to defaults** and **Back** (Escape or B goes
back to the panel).

It is the **Controls** page beside the rail (`Screen::Commands`), opened from Settings' Commands row or the rail; LB / RB go to
the next page. Rows are 44 canvas pixels apart (`look::Commands`: 11 a page); the chevrons at the top right change pages with
the mouse, and keyboard/pad navigation reveals the selected row. A notice reserves space above the hints.

| Cell | Accept (Enter, A, a click) | Written |
|---|---|---|
| Key | waits for the next key ("Press a key"; Escape cancels; F3 is refused, and Tab while it opens the Toy Box) | `key_<action>=<SDL scancode name>` (the key's place, as Arkeyan Launcher stores it) |
| Pad | waits for the next pad button, read as it is (no remapping, no keyboard: `HostInput::raw`; the first button pressed after all were released; Escape cancels) | `pad_<button>=<the row's button>` (the pressed button does that job) |
| Pad of a stick direction (the moves, Spyro's Adventure's right stick) | nothing to choose: it says the stick stays | |
| Mouse (only with the switch on, else it says so) | waits for the next mouse button or wheel tick ("Click or scroll"; Escape cancels); that click does nothing else (`ToyboxWaitsForMouseButton`) | `mouse_<action>=<mouse name>` (`host/mouse.h`); the action that had the button gets `none` |
| Mouse row | flips the switch | `mouse=1` / `0` |
| A Toy Box key | waits for the next key | `[input] key_toybox`, `key_toybox_add`, `key_toybox_clear` (F6, F7, F8 when not set; Tab opens the Toy Box too while `key_toybox` is not set, and is listed beside F6) |

- **Same file and rules as Arkeyan Launcher** (`host/commands.cpp`). The first change seeds the game's copy from the shared `[input]` keys
  and writes `seeded=1` (Arkeyan Launcher's `SeedGameInput`); every change then puts the game's keys in `[input]` as Arkeyan Launcher's PLAY
  does (`ApplyGameInput`), so a start without the launcher has them too. A key another row of the game or a Toy Box key has: the two
  **swap** keys. A pad button: it takes the row's job and the button that did that job takes its old one, so **no job is lost**
  (pressing the row's own button gives every button its own job back). A mouse button: one action per button, the one that had it
  (by its line or the game's default, `game::Adapter::mouseDefaults`) gets `none`. **Reset to defaults** removes the game's `key_`,
  `pad_` and `mouse_` lines and the Toy Box's keys (`seeded` and the mouse switch stay) and `[input]` gets the game's default keys.
- **At once**: the page hands the bindings it saved to the main loop (`commands::Publish` / `Take`, read as `ParseSettings` reads them:
  `[input]`, then `[<game>.input]`: keys, pad, mouse, Toy Box keys), which rebuilds its input the same frame:
  `[input] bindings in use now: key_b=K, ...` in the log; the Toy Box's keys apply at once too (`ToyboxSetKeys`, which also gives
  the portal its add and clear keys, `PortalSetHotkeys`). Every change is a line: `[gamemenu] key_b = K (swapped: key_right_down = X)`,
  `[gamemenu] pad_a = b (swapped: pad_b = a)`, `[gamemenu] mouse = 1`, `[gamemenu] mouse_b = Mouse Right (mouse_x = none)`.
- A binding just taken: the pad does nothing on the page until every button is released (a key that is also a pad button through the
  keyboard bindings would otherwise act again).

## The mouse in the menu and the Toy Box

Every row, button, stepper, cell and card of the F3 menu and the Toy Box is a click target (`AddHit` in `draw.cpp`); the one under the
pointer gets a soft outline, the pointer moving onto a menu row, a card or a button moves the focus ring there (never a
still pointer; the Commands page's cells keep the focus to the keys), and a click focuses it and does it
([toybox_ux.md](toybox_ux.md), "Navigation"). The
wheel moves the panel's, Settings', Achievements' and Controls' focus, scrolls the Toy Box's lists, and steps Edit stats. The
rail's rows and Achievements' tabs are click targets too. Keyboard and pad work as before.
The pointer is shown and free while one of them is open (`ToyboxOverlayOpen`; the game holds it only in play: `host/mouse.h`).

## Report a problem

One **text file**, `<game id>-report-<yyyymmdd-hhmmss>.txt` (for example `ssa-report-20261028-165644.txt`), on the **Desktop**; the
logs folder (`<data root>/logs`) when the Desktop cannot be written. The folder is shown in the file manager (`SDL_OpenURL`)
and the menu says where the file is. Nothing is uploaded. `SKY_REPORT_DIR=<folder>` writes there alone and opens nothing
(tests); `SKY_LAUNCHER_LOG_DIR` replaces the logs folder as in Arkeyan Launcher.

The format is Arkeyan Launcher's ([launcher integration](../../release/launcher/README.md)): a header, then each part under
`===== name =====`:

1. **Header**: game, version and build, the OS, the CPU, memory, the GPU adapter, backend and window, the settings line, and
   whether there was a crash. (The log's own `[version]`, `[system]`, `[gpu]` and `[settings]` lines, so what the report says is what the game logged.)
2. **last crash**: the last `Fault (` block of the log, from this run's log or else the previous run's (a crash ends the run,
   the next start moves its log to `Arkeyan.prev.log`): the fault, the `[crash]` lines, the host stack.
3. **launcher.ini** (the settings the game started with, and the ones the menu saved).
4. **Arkeyan.log** and **Arkeyan.prev.log** (the last 600 KB of each).
5. **skylauncher-game.log**: the launcher's capture of the game's output, when the game was started from it.

**Redaction** (`report::Redact`, unit-tested by the harness check): the home folder becomes `~` (every spelling of its path, and
`/Users/<name>`, `/home/<name>`, `C:\Users\<name>`), the account name `<user>` and the machine's name `<computer>`, whole words,
case-insensitive; everything else is untouched. `tools/regress/f3menu.sh` fails if the report holds the real home path, the account
name or the host name.

Arkeyan Launcher writes the same file from Settings > Support > Report a problem (**Save to Desktop**) and from the "The game stopped
unexpectedly" dialog (**Report**: the file is written, shown in the file manager, and the GitHub issue page opens to attach it).
It has the same sections (header with CPU, **last crash**, files) and the same `~`.

## Where a crash happened

When the runtime stops on a fault or on one of its own fatal errors, the log says where in the game:

```
Fault (signal 11 segv) at host address 0x0
[crash] where: guest thread 7, lr 0x8262D6D4, r1 0x700B7D30, r3 0x40070600
thread "", sp 0x175276cc0, fp 0x175276d00
pc 0x102720fa0 _Z11WaitForFlipj + 164 (...)
#0 0x1025c3690 _Z12sub_822A95C0R10PPCContextPh + 196 (...)
#1 0x10405b0dc __imp__sub_8262D690 + 280 (...)
...
[crash] guest function: sub_822A95C0, called from sub_8262D690, sub_826320C8, sub_82635CF8
```

- **`[crash] where:`** (`PrintGuestLocation`, `thread.cpp`): the guest thread's id and the guest registers last stored in its
  `PPCContext`: `lr` (the last call that went through the context), `r1`, `r3`. A register the recompiled code kept
  in a host register is not there, so `lr` names the last call, not necessarily the faulting instruction [V: the test
  below]. `ctr` and `r31` are no longer printed since every game's translation keeps them as the functions' locals
  (2026-11-04, [xenonrecomp.md](../../tools/toolchain/xenonrecomp.md#registers-as-locals)); a translation without
  those options still prints them. On a host thread (video, audio, the main loop) it says so.
- **`[crash] guest function:`** (posix): the recompiled functions on the host stack, innermost first, from the symbol names
  (`sub_XXXXXXXX`, `__imp__sub_XXXXXXXX`, or the C++ mangling of one): `sub_822A95C0` is the guest function at 0x822A95C0 (look it
  up in `games/<id>/` and the dump). "none named on the host stack" means the fault is in host code. The host stack is the
  platform's own (`atos -o <executable> -l <base>` for raw addresses).
- **Fatal errors**: `SIGABRT` is handled too, so `abort()` (a call into unrecompiled guest code, a full system heap) gives the same
  block. Windows: the same `Fault (` block and `[crash] where:` from the unhandled-exception filter and `SIGABRT`; its frames are
  offsets in the executable, not symbol names, so there is no `guest function` line there (not run on Windows) [?].
- **Test aid** `SKY_CRASH_TEST=fault|abort`: after the 400th flip the guest's render thread faults on a null pointer (or aborts).
  `tools/regress/f3menu.sh` runs it, checks the three lines in the log, then checks that the next start's report carries them.

## Tests and shots

`tools/regress/f3menu.sh <ssa|giants|swapforce|superchargers|imaginators> <disc>` (part of `tools/test_all.sh`, once per game; ~1.5 min): run A faults on a
guest thread (the `[crash]` lines are checked); run B (A's log is now the previous log) opens the panel with F3, lowers the volume,
opens Settings and turns the counter on (and steps Edge smoothing where there is one), comes back with Escape, writes the report
and closes with F3, then turns the debug screen to Full (Shift+F3 twice), records ten
seconds with F3+R, copies the screen with F3+C, hides and shows the graph with F3+G, and records with Settings' Record row
until it is turned off, through `SKY_TOYBOX_SCRIPT` (`down=F3`, `C`, `up=F3` hold F3 for a combination). It also checks the
debug screen's lines in the log, `debug_screen=2` in the settings file and the two performance logs (header, 40 frames or more). It checks: opened and closed, the
volume change logged, the counter shown (`[gamemenu] fps counter shows`), `[<game>.audio] volume=80` and `[<game>.video] show_fps=1` in the
settings file, the report's parts, the previous run's crash in it, no home path, account name or host name in it, and a `~`.

**Run C, by pad (0.9.2; SuperChargers and Imaginators, whose runs A and B are not run; `PAD=1` adds it to the others; `tools/test_all.sh`
runs it after their hub test).** The panel and every rail page walked with pad presses only, through `SKY_TOYBOX_SCRIPT` (`pad=back*0.6`,
`pad=start` 0.1 s later: Start inside View's hold, before the wheel's 0.25 s). `SKY_PRESS` cannot do it: it is added to the pad the
*game* reads (`GetGamepad`), after the menu's filter (`ToyboxFilterGamepad`) has seen the host pad, so the menu never sees it. The walk:
the Toy Box, Achievements and Settings rows opened and left with B; on Settings' page RB (Controls), LB (Settings), LB (Achievements; there
LB / RB are the tabs' keys and step no page); Performance stepped Compact, Full, Off; "Report a problem"; B closing the panel. Checked in
the log: the `[gamemenu] opened`, `achievements opened`, `settings opened`, `commands opened` lines, `debug_screen = 1, 2, 0`, `[report]
written`, `[toybox] closed`. The script starts at 100 s (`PAD_START`): the game's own vehicle prompt opens the Toy Box around 70 s on a
cold start (no shader cache), and the panel does not open over it. SuperChargers and Imaginators: PASS (2026-11-06, the 0.9.0 builds).

The Commands page: `ArkeyanCommandsTest` (in `sky_add_tests`; a scratch `launcher.ini` in the temporary folder or `argv[1]`): the seed once,
the key written, the swap, the pad jobs swapped and given back, a `none` line, Reset, `[input]` as PLAY leaves it, `commands::Read`
equal to `ParseSettings`, and `Take` once. `tools/regress/f3commands.sh <Spyro's Adventure disc>` (~1 min; `SKY_LANGUAGE=fr` for the
French page): F3, Settings, Commands (the Controls page), B's key to K (it swaps with the right stick down's), the pad's A to B's job, Escape, F3; it checks the log
lines, `[ssa.input]` `key_b=K`, `key_right_down=X`, `pad_a=b`, `pad_b=a`, `seeded=1`, `[input] key_b=K`, and, through the script's
`hold=K` (a key the game's input reads as held, `ToyboxKeyboardState`), that K gave the game the right stick down before and gives it
B (`0x2000`) after. Shots: `private/runs/commands/ssa/c_menu.png`, `c_page.png`, `c_waitkey.png`, `c_key.png`, `c_waitpad.png`, `c_pad.png`.
`tools/regress/f3mouse.sh <Spyro's Adventure disc>` (~1 min, 24 scratch figures): through `move=`, `click=`, `rclick=` and `wheel=`
(each, and each scripted key, logs the focus: `[toybox] script: focus: ...`), the pointer moving onto the panel's Settings row focuses
it and a click opens the page; wheel ticks move the focus down the list (it scrolls), the pointer moving onto VSync and the FPS
counter focuses each, a click turns the counter on, a tick up moves the focus; a click on the rail's Controls, its next page, the
Mouse row (on), the first page, B's mouse cell, a right click gives B the right button (X loses it); in the Toy Box the pointer moving onto the fourth card focuses it, a
click keeps it and three wheel ticks move down the list. It checks the log, `[ssa.input]` `mouse=1`,
`mouse_b=Mouse Right`, `mouse_x=none`, and the running game taking them. Shots: `private/runs/commands/mouse/m_*.png` (`m_hover`,
`m_settings`, `m_settings_fps`, `m_page`, `m_waitmouse`, `m_mouse`, `m_card_hover`, `m_card`, `m_scrolled`). `ArkeyanCommandsTest` also covers the mouse lines, the
switch, the Toy Box keys and their swaps, and Reset.
Shots (offscreen, `SKY_TOYBOX_SHOTS`): `private/regress/f3menu-<game>/b_open.png`, `b_report.png`, `b_closed.png`; the English and French ones are in the wave's report.

The frame rate row offers the launcher's list (`gamemenu::FrameRates`). A value read from `launcher.ini` is shown as it is, and `NormalFrameRate` keeps
any rate of the list unchanged (another number goes down to the rate below it), so opening the menu or moving through it lowers nothing; only moving the
frame rate row writes `fps_limit`. Checked: `ArkeyanSettingsTest` (every rate stands for itself) and `tools/regress/f3menu.sh` (a `fps_limit=144` in the game's
section is still there after the menu was used).

## Debug screen

Code: `runtime/host/gamemenu_debug.cpp` (collection, layout, performance log), `runtime/host/debug_screen.h` (the pure,
allocation-free logic: levels, pacing, hitch causes, the limit, the CSV line, the accent), `runtime/gpu/frame_stats.{h,cpp}`
(the per-frame measurements), `runtime/host/machine_state.{h,cpp}` (process, thread and system CPU, RAM, thermal state,
power, the runtime's own counts). The F3 row **Debug screen** / **Écran de débogage** chooses Off, Compact or Full, saved as
`[<game>.video] debug_screen` 0, 1 or 2; **Shift+F3** cycles it in play. It is drawn on the Toy Box's overlay (the same
software rasterizer and texture on Metal, Vulkan and D3D12), beneath the F3 menu and the wheel, hidden on the Toy Box's
full pages, and refreshed twice a second.

**The look** (approved by the owner, 2026-11-06, after Minecraft's F3 screen and Arkeyan Launcher's style): one line per stat on
its own translucent strip (`15171C`), the key in grey Outfit, the value in white monospace (ImGui's own ProggyClean), grouped
under headings in Bungee coloured with the running game's glow (Arkeyan Launcher's `StyleOf(game).glow`, `game::Accent()` in each
`games/<id>/adapter.cpp`, lightened to a 4.5:1 contrast as the launcher does for Trap Team's red); the bottom boxes on
`1E1E24`, bar tracks `26262C`, rules `3A3A42`. Left column: FRAME, GUEST, RENDER, AUDIO AND INPUT; right column, right-aligned:
MACHINE, SESSION. Compact keeps fps, frame time, cap, output, level and game, and OS, CPU, GPU, GPU memory, RAM.

| Line (Full) | What it shows, and where it comes from |
|---|---|
| fps | the presented frame rate (half a second), 1% low, the slowest and fastest frame of the perf window as FPS |
| cap | the frame limit, VSync, the 30 FPS drop |
| frame | the mean frame time; **cpu** the game's render thread's CPU time a frame (its thread clock read at each `gpu::Present`), **gpu** the GPU busy time a frame, **wait** the emulated vblank wait plus the present call |
| limited by | the frame limit when the frames last what it asks; else the GPU when it is busy 85 % of a frame; else a CPU thread (the game's, or the GPU thread's CPU a frame) busy 85 %; else "nothing clear" (`debugscreen::Decide`) |
| pacing | present-to-present mean and p99 (the perf window), frames **missed in the last 60 s** (longer than 1.5 frames of the limit, or 50 ms without one) |
| latency | **pad>game**: the newest SDL key, button or stick event before the game's `XamInputGetState`, to that read; **game>present**: that read to the present call returning; their sum, `~`: an estimate without the display's own delay |
| hitches 50 ms+ | how many among the recent frames, the newest with its cause: pipeline built at draw time, GPU read-back wait (a producer's `RunAndWait`), GPU work (over half the frame), loading (`NtReadFile` in the frame), texture uploads (16 MB+), else unknown |
| GUEST cpu, threads, heap, level, portal | game-thread CPU against the renderer's host calls (the d3d hooks, `gpu::FrameTime::Renderer`); guest threads running; the physical heap allocated and its size, write-watch faults a frame; the map's name (the zone path's last part), frames presented, "loading" while the game read files in the last second; each portal seat's toy type (block 1, once the title read it) |
| RENDER | scale, scene (1120x704 x scale) and output; draws and resolves of the guest frame, render-target changes; shaders cached and missing, pipelines ready and built at draw time in the last 60 s; host textures alive, uploads a frame and their MB, texture-pack files; video memory and its budget |
| AUDIO AND INPUT | 48 kHz, the game's audio clients playing, the device queue in ms, underruns (an output that found the queue empty); the pad's type (never its name) or the keyboard, the button pictures |
| MACHINE | OS and architecture, CPU model and logical cores, process and system CPU %, GPU and its busy % (GPU ms over frame ms), GPU memory, game and system RAM, display mode and window, power and thermal state (macOS thermal pressure; Linux the hottest thermal zone; Windows n/a) |
| SESSION | time played, game and version, build |

**Full** adds, bottom left, the **frame-time graph**: the last 240 frames, game-thread CPU (blue) under GPU (orange), 16.7 and
33.3 ms lines, the worst frame and how long ago; bottom right, **per-thread CPU** bars (% of one core): game, render (the
GPU thread), host main, texture workers, shader build, audio; and the key hint line at the bottom centre. **F3 held** with
**G** hides or shows the graph, **T** the thread bars, **C** copies every line to the clipboard as text, **R** records ten
seconds of the performance log; the game does not see those four keys while F3 is down.

**Not measured, shown as n/a**: guest threads waiting, write-watch pages watched, MRT aliases, an upscaler (the runtime has
none: no CAS or FSR), texture-pack replacements in use (the files indexed are shown), the video-memory split by kind, a
streaming thread, the disc check, the profile, the log's warning count; the driver version on Metal; a pad>game latency
without an input event (the scripted runs press through the guest, not SDL).

**GPU time** (`gpu/gpu.cpp`, only while measuring): Metal reads each command buffer's own `GPUStartTime`/`GPUEndTime` (the
buffer kept alive until its fence is waited for); Vulkan and D3D12 write a timestamp at each end of a command list (plume's
query pools). A frame's GPU time is the union of its command lists' spans; known one or two frames late, so a sample is
finished once its present pass is read back or four frames old.

**Cost.** Nothing is measured while the screen is Off or Compact and no log records: every call site tests one relaxed
atomic (`framestats::Enabled`). Full logs its own cost every ten seconds, `[debugscreen] full: X ms a frame (measurement Y,
panels and overlay Z)`. Measured (2026-11-06, macOS, M1 Pro, 30 FPS, offscreen, a quiet machine): SSA `level_027`
0.083-0.100 ms, Swap Force's tutorial 0.077-0.100 ms, Imaginators' title 0.079-0.111 ms a frame (measurement about 0.004);
Swap Force at scale 3, 42 FPS GPU-bound: 0.046-0.060 ms. Two fixes got it there: the panels drawn as plain rectangles (a
rounded one is a fan of long triangles the software rasterizer tested over its whole box, about 10 ms a refresh) and the
renderer's snapshot taken with `try_lock` (a GPU-bound GPU thread, or the offscreen output pacing itself, holds the lock
while it waits; the refresh tries again at the next update, and waits for it only after a quarter of a second without it).

**Checked against the game's own counters** (the performance log over the same stretch as the `[render]`
(`SKY_RENDER_STATS=1`) and `[perf]` lines; `private/runs/f3debug/`): SSA `level_027` draws 1884 against 1885, resolves 42.0
and 42.0, 29.85 FPS and 29.85, renderer 3.99 ms against `[perf]` 4.0; Swap Force 453 against 455 draws, 46.0 resolves, 30.00
FPS, renderer 1.15 against 1.1; Imaginators 144 against 146 draws, 54.0 resolves, 30.00 FPS. GPU: Swap Force's tutorial at
scale 3 with the drop to 30 off ran 42.0 FPS, frame 23.8 ms, **GPU busy 23.7 ms** while `[perf]` showed the GPU thread
waiting 21-22 ms a frame for the GPU, and the screen said "limited by GPU"; Metal System Trace measured 20.5 ms of
fragment, vertex and blit work there ([renderer_threads.md](../gpu/renderer_threads.md), "The GPU at scale 2"), the
command buffers' spans also count their gaps.

**Performance log.** The Debug row **Record a performance log** (or F3+R for ten seconds) writes
`<logs folder>/<game>-perf-<yyyymmdd-hhmmss>.csv` (`SKY_LAUNCHER_LOG_DIR` replaces the folder, as for the report): a header,
then one line per presented frame, `time_s,frame_ms,renderer_ms,gpu_ms,latency_ms,draws,resolves,uploads,upload_mb,write_faults,pipelines_built,file_reads,hitch`
(an unknown value is empty), at most 32 MB, then it stops by itself (`[gamemenu] performance log full`); a toast names the
file when it ends. Lines are written by the main thread from the measurement's ring (1024 frames), never by the frame loop.

**Privacy.** Every value goes through `debugscreen::Label`, which refuses a path or an address (a slash outside "n/a", a
backslash, an `@`); the CPU brand stops at its `@`; the pad shows its type, not its name; figures show their toy type, not a
nickname; the log and user folders are never read.

**Tests.** `ArkeyanDebugScreenTest` (synthetic measurements through the real `frame_stats` ring, the real fonts and rasterizer):
Compact and Full in English and French, every Full line's value, the busy renderer, the graph and thread bars, the
clipboard text, the graph and thread toggles, the clear gameplay area, no C++ allocation while collecting or logging, the
log's lines, its ten-second end and its 32 MB stop. `ArkeyanSettingsTest`: `debug_screen` 0/1/2 and its strictness, pacing,
hitch causes, the limit, the CSV line, the accent's contrast, `Label`. `tools/regress/f3menu.sh` runs Shift+F3 to Full,
F3+R, F3+C, F3+G and the Record row in each game. Shots: `private/runs/f3debug/<game>-<en|fr>/shot_*.png`
(`.scratch/f3_run.sh` in the lane).
