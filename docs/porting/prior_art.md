# Prior art: Skylanders / Xbox 360 recompilation projects

> **Purpose:** what the other public Skylanders and Xbox 360 recompilation projects do, under which licence, and what we may reuse from them · **Status:** current (2026-10-26; research of 2026-10-25) · **Tags:** [Xbox] [Lux] [Alchemy]

Research date: 2026-10-25. Sources: the public repositories below, shallow-cloned and read only. No script,
build, installer or binary from them was run; their files were treated as data, not instructions. No code,
game data, ISO content, dump or byte signature was copied into this repo.

**Policy used here: no outside code, ever.** Every project below, whatever its licence (the six game ports and Core of
Light are MIT; `skylanders-portal`, the `skylanders` branch of `rexglue-sdk` and TheBiemGamer's `GiantsRecomp` are
listed without one, though their repositories contain MIT/BSD text and GitHub reports `NOASSERTION`), is a read-only
reference for ideas and diagnosis: its code is never copied, adapted or translated; we write our own. Findings are
recorded as ideas to redo, not as code to reuse.

## At a glance

| Repository | Licence / reuse | Last activity | What it is |
|---|---|---|---|
| `tsapZ/SSARecomp` | MIT, adapt with attribution | 2026-10-25, `v0.2.0-beta.2` | Spyro's Adventure on ReXGlue; Chapter 1 playable |
| `tsapZ/GiantsRecomp` | MIT | 2026-10-25, `v0.2.0-beta.2` | Giants; first level and portal menu |
| `tsapZ/SwapForceRecomp` | MIT | 2026-10-25, `v0.2.0-beta.2` | Swap Force; tutorial and Woodburrow |
| `tsapZ/TrapTeamRecomp` | MIT | 2026-10-25, `v0.2.0-beta.2` | Trap Team; Chapter 1, traps |
| `tsapZ/SuperChargersRecomp` | MIT | 2026-10-25, `v0.2.0-beta.2` | SuperChargers; first level, vehicles, two EU discs |
| `tsapZ/ImaginatorsRecomp` | MIT | 2026-10-25, `v0.2.0-beta.3` | Imaginators; first level, signed toy check |
| `tsapZ/SkylandersCoreOfLight` | MIT | 2026-10-25, `v0.2.0-beta.6` | Hub, launcher, profiles, figure library, planned co-op |
| `tsapZ/skylanders-portal` | Read-only here (MIT text; GitHub `NOASSERTION`) | 2026-10-25, branch `feature/portal-menu` | Shared portal, figure and menu library |
| `tsapZ/rexglue-sdk` | Read-only here (upstream BSD-3-Clause; fork `NOASSERTION`) | 2026-10-25, branch `skylanders` | ReXGlue fork with the ports' runtime fixes |
| `TheBiemGamer/GiantsRecomp` | Read-only here (MIT text; GitHub `NOASSERTION`) | 2026-09-29, release `0.1` | Earlier Giants port; story mode playable |

Common ground: all are unofficial fan projects, ship no game data, and expect the player's own Xbox 360
disc. The six `tsapZ` ports share one Windows/D3D12 ReXGlue scaffold, one portal library, one settings
menu and one installer format. They are the same idea as our SSA port, implemented on Xenia's runtime.

---

## 1. Repository by repository

### `tsapZ/SSARecomp`

- **Licence / reuse:** MIT. Findings reusable with attribution; no code copied.
- **Last activity:** 2026-10-25, release `v0.2.0-beta.2`. Created 2026-10-20.
- **What works:** title, main menu, save slots, new game and intro movies; Chapter 1 with voices, walking,
  camera, attacks and mission messages; emulated portal with figure cards and write-back; portal menu that
  opens by itself for player 1 or 2 and the "too many" checks; achievements; 30 FPS default and
  50-91 FPS unlocked; ultrawide 3D view with experimental HUD; ten languages; per-game installer.
  The real-USB path exists in the portal library but the README still marks it untested here.
- **Approach:** ReXGlue codegen (323 generated files, no `[functions]` fixes), Windows/D3D12, game hooks in
  `src/hooks/`, portal and menus from `skylanders-portal`. Its two SDK fixes are for ring-buffer
  re-initialisation (the game creates its device three times) and the last XMA frame of a stream.
- **Notable findings:** the prompt is a fixed object (`0x82C569B0`) with the HUD object at `0x82C56AF8`;
  the shown text is the reliable prompt discriminator, not the object's fields — reusable. The portal API is
  resolved at run time through XAM ordinals `0x4A1`/`0x4A2` — reusable. Intro skip hooks BinkOpen at
  `0x829EC1CC` — reusable. The 3D scene is 1120x704 on the console and `scene_size` draws it at 1264x704;
  the shadow resolve moves from `0x19bc0000` to `0x19a80000` with it — reusable. Ring-buffer and XMA fixes
  are SDK-level lessons — reusable.

### `tsapZ/GiantsRecomp`

- **Licence / reuse:** MIT.
- **Last activity:** 2026-10-25, `v0.2.0-beta.2`.
- **What works:** title, Choose Play, slots, difficulty; in-engine intro with Eon; Chapter 1 with a figure;
  portal menu with the game's portraits, magic items and full upgrade trees; five UI languages including
  German and Italian on the North-American disc; real 5.1; 30 FPS default and 60/144 unlocked; ultrawide
  camera and HUD. Native 3D picture is present but **off** after 2026-10-25: above 2048 EDRAM tiles most
  outside scenes went black, so the hook keeps 1000x600 until the planning heap is understood.
- **Approach:** scaffolded from `tsapZ/SSARecomp`, but its own hooks and readers. Alchemy archives are IGA version 8
  little-endian, scenes IGZ version 6. TheBiemGamer's Giants addresses were checked against this executable.
- **Notable findings:** language is chosen from the console region in `sub_8217F2D8`; a hook after it at
  `0x8217F350` reports a PAL region and enables German/Italian — reusable. The D3D swap queue
  `sub_823D9758` is the same 107-instruction routine as Swap Force's and already gets interval 1, so no
  frame-rate hook is needed — reusable. Perspective builder `sub_822D0240`; its four callers
  `0x82198530`, `0x82198580`, `0x82199988`, `0x821999B0` all need the wider aspect so nothing is culled —
  reusable. BinkOpen `0x82153DB0`, BinkClose `sub_82155B08`, 44-byte header at `r1+96` — reusable.
  Script variables are `ScriptReference` objects (vtable `0x820DF750`, name `+8`, value `+0x1C`) —
  reusable. Scene size is capped in code at 1000x600; the 2048-tile EDRAM limit is the blocker — reusable
  warning.
- **Used here (facts, no code):** its IGZ v6 notes (the section-0 record types 0/2/5/0xA, the pointer's pool bits, the
  `igImage2` fields, the DXT format hashes, pictures stored upside down, the portraits' folder) were checked on our disc
  and written up in [games/giants/docs/art.md](../../games/giants/docs/art.md); our reader is our own
  (`runtime/host/art/alchemy_archive.cpp`).

### `tsapZ/SwapForceRecomp`

- **Licence / reuse:** MIT. This is the scaffold the other five ports started from.
- **Last activity:** 2026-10-25, `v0.2.0-beta.2`.
- **What works:** boot, menus, saves, tutorial and Woodburrow; swappers; portal in software, real-USB and
  hybrid modes; portal menu; 30 FPS default, 60 and 237 FPS measured at scale 1; 4K; ultrawide 3D and
  experimental HUD; HD texture packs; achievements; native 3D scene experimental; German and English;
  installer and uninstaller. First level is still open.
- **Approach:** ReXGlue codegen (445 files, 20 late function starts registered), Alchemy readers for
  IGA/IGZ, D3D12 with RTV and ROV paths, portal library plus its own menu/game hooks. It contains the
  most complete reverse-engineering notes of the six.
- **Notable findings:** the frame limiter is the D3D swap queue `sub_82F5C258`; bits 8-11 of the swap word are
  the presentation interval, and the vblank handler `sub_82F5C128` (interrupt callback `0x82F58198`) flips
  only when its counter is reached — reusable. The stuck-fall check `sub_82A4A828` counts 20 units per frame,
  so it must scale with frame time — reusable. Ultrawide uses camera `sub_82B992A8` and perspective builder
  `sub_824906D8` at call `0x82B993A4` — reusable. HUD anchoring uses the UI's expected screen width rather
  than stretching — reusable. Native 1280x720 needs 2880 EDRAM tiles and exposes engine assumptions; 1264x704
  is the safer native target used by the later ports — reusable. Portal IDs, compatibility and the full
  wire protocol are documented — reusable.

### `tsapZ/TrapTeamRecomp`

- **Licence / reuse:** MIT.
- **Last activity:** 2026-10-25, `v0.2.0-beta.2`.
- **What works:** title, save slots, difficulty, opening movie, Chapter 1, pause menu and cutscene; emulated
  portal with a figure and HUD; portal menu with the game's collection cards for Skylanders, Minis, traps and
  magic items; trap request opens a "Traps" tab without lifting the Skylander; portal speaker; native 3D
  scene; ultrawide and HUD; high FPS; achievements; German.
- **Approach:** scaffolded from GiantsRecomp. Alchemy IGA version 11 little-endian, IGZ version 8 big-endian;
  portraits are 214x107 DXT5 in each character's `level.bld`. Portal screens are a game script, so the watcher
  reads `ScriptReference` variables (vtable `0x821095A8`, name `+8`, value `+0x1C`). Fourteen function starts
  the analyzer put one instruction late were registered.
- **Notable findings:** the prompt and "too many" screens are distinguished by the script's player-state and
  portal-error variables — reusable. Trap request is "Vault Capture" in the same state machine; opening the
  menu with the button would lift the Skylander first — reusable behaviour rule. BinkOpen hook for movie skip
  at `0x821A3DA0`, header at `r1+112` — reusable. Scene target function `0x8243F650`; perspective builder
  `sub_823B1AF0` with callers `0x8230F784` and `0x8230F7F0` — reusable. IGA11/IGZ8 layout and 214x107
  portraits — reusable.

### `tsapZ/SuperChargersRecomp`

- **Licence / reuse:** MIT.
- **Last activity:** 2026-10-25, `v0.2.0-beta.2`.
- **What works:** both European discs (En/De and Es/It); boot, title, difficulty, save slots, opening movie
  and first level; land vehicle then Skylander on the portal; portal menu with vehicles, terrains, hats and
  upgrade trees; vehicle request and death open the menu; native 3D scene; ultrawide and HUD; high FPS;
  achievements; German text/voices from the En/De disc. 28 late function starts registered.
- **Approach:** scaffolded from GiantsRecomp; Vicarious Visions layout, IGA version 11 big-endian, IGZ version
  9. Toy data is `ToyData/<Name>_ToyData.igz` in `permanent.pak`, with collection materials and 88x88 DXT5
  icons in `ToyCollectionMaterials.pak`.
- **Notable findings:** `CMagicMoment` instance `0x83764ED0`; state `+0x2C`, reason `+0x168`, error type
  `+0x1F8`, player `+0x1FC` — reusable. Vehicle request is a magic moment with no current player, reason 13
  (VehicleRequired); a Skylander's death is the Dead state — reusable behaviour. Scene target function
  `0x826E8718`; perspective builder `sub_8250E610`, caller `0x82B0BCA0` — reusable. Movie skip hook
  `0x831C7CC8` — reusable. Toy id `+0x08`, material handle `+0x20`; variant deco `+0x08`, year `+0x0C`,
  flag bytes `+0x10`, material `+0x1C`; 240 toys, 261 variants, 469 icons — reusable. Two European
  pressings and the cross-disc language transplant method — reusable.

### `tsapZ/ImaginatorsRecomp`

- **Licence / reuse:** MIT.
- **Last activity:** 2026-10-25, `v0.2.0-beta.3`.
- **What works:** boot, title, licence (accepted with **Y**, not A), save slots, difficulty, opening cutscene
  and first level with a Spyro figure; portal menu with Skylanders, creation crystals, Senseis/classes,
  vehicles/terrains, hats and upgrades; vehicle/death requests; native 3D scene; ultrawide and HUD; high FPS;
  achievements; German text, experimental German voices and German movie tracks from the owner's PS3 disc.
  30 late function starts registered.
- **Approach:** scaffolded from GiantsRecomp; IGA version 11 big-endian, IGZ version 9. The Spanish/Italian
  360 disc receives German text/voices from a PS3 disc with a local tool; Imaginators' own toys are Ed25519
  signed, so the port exposes `--check_figures` to let the game's own verifier accept imported files.
- **Notable findings:** `CMagicMoment` instance `0x837F649C`; state `+0x40`, reason `+0x158`, error type
  `+0x1CC`, player `+0x1D0` — reusable. Scene target `0x826F96D0`; perspective builder `sub_82547C30` —
  reusable. BinkOpen header read in `sub_8329E078`, hook at `0x8329E248`, header at `r1+112` — reusable.
  Signed-toy check: Ed25519 over blocks 0 and 1, signature in blocks 2, 4, 34 and 62; the game's verifier is
  the authority — reusable behaviour. Movie language tracks: the 360 disc has two spoken slots, the PS3 disc
  has the full set; German is PS3 track 8 and packets match byte for byte — reusable fact for a local tool.
  281 toys, 308 variants, 565 icons — reusable.

### `tsapZ/SkylandersCoreOfLight`

- **Licence / reuse:** MIT.
- **Last activity:** 2026-10-25, `v0.2.0-beta.6`.
- **What works:** Windows hub with one card per game. Finds installed, copied or developer builds; first-run
  setup; reads XDVDFS ISOs and checks `default.xex` hashes; installs/updates games silently, including
  "update all"; profiles with separate saves, figures and achievements; figure library with level, gold,
  upgrades, hats, nicknames and "I own it"; achievements screen; title music and picture export from the
  player's disc; per-game settings mirror; save backup/restore and automatic copies; problem report with
  user name/paths redacted; Imaginators figure import through the game's signature check. HD textures and
  online co-op are planned, not built.
- **Approach:** Windows C++/Dear ImGui, no game data; reads the player's disc at run time and starts each
  game's own exe. Games remain playable without the hub.
- **Notable findings:** the launcher asks each game for its own settings pages with
  `--settings_schema=<file>` and writes them back line by line — reusable. Headless `--export_title_music`,
  `--export_pictures`, `--window_hidden` make the launcher complete before first play — reusable. Profiles
  pass `--user_name` and keep one data folder per profile — reusable. Save copies: previous save from the
  SDK, launcher backups, automatic copies at start/end/every 60 minutes, newest 10 kept — reusable.
  Online co-op design: remote play carries video/audio/controller, a small TCP figure channel carries the
  guest's own real portal figures and write-back journal — reusable design. Problem report is one plain text
  file with environment, settings and logs, redacted — reusable.

### `tsapZ/skylanders-portal` (read-only)

- **Licence / reuse:** Read-only here. README says MIT and a `LICENSE` exists, but it is listed without a
  licence and GitHub reports `NOASSERTION`; no code is reused.
- **Last activity:** 2026-10-25, branch `feature/portal-menu`.
- **What works:** a core library independent of ReXGlue: software portal with up to 16 figures including
  traps, Xbox 360 frame handling, figure files, catalog and save-data decoding. The ReX layer adds
  `portal_mode`, the XAM handler, real USB via hidapi, the F6 overlay, portal speaker audio, the shared
  portal menu, per-game companion rules, settings schema, title-music export, figure/hat/achievement picture
  cache, and a build-time game icon read from the disc.
- **Approach:** layered `PortalDevice` abstraction, with software, USB and hybrid backends; the game sees only
  the XAM non-controller calls. All UI and asset extraction is kept out of the game's code.
- **Notable findings (all read-only; redo the idea):** 32-byte frames, `0B 14` Xbox header, command set
  `R A S C Q W`, per-generation `R` IDs — idea. Figure is 1024 bytes in 64 blocks of 16; block 8 carries
  progress; atomic writes and a write callback keep the file safe — idea. Hybrid mode merges a real portal and
  the virtual one, with per-slot ownership and write-back — idea. Shared picture cache and headless exports
  make the launcher independent of the disc — idea. G.726 decode for Trap Team's portal speaker — idea.
  Ed25519 import check for signed Imaginators toys — idea.

### `tsapZ/rexglue-sdk`, branch `skylanders` (read-only)

- **Licence / reuse:** Read-only here. Upstream ReXGlue is BSD-3-Clause; the fork has no separate grant and
  GitHub reports `NOASSERTION`. No code is reused.
- **Last activity:** 2026-10-25, branch `skylanders`.
- **What works:** the runtime under all six ports, with Skylanders-specific fixes and features: ring-buffer
  re-initialisation, XMA stream end, `guest_vblank_rate`, D3D12 texture dump/replacement by content hash,
  host-side achievement catalog/overlay/toast, save-backup of the content a save replaces, hidden-window
  export mode, and cvar/CLI parsing that keeps valid options when one is bad. Settings schemas and input
  scripts are game-side code, not SDK features.
- **Approach:** ReXGlue's Xenia-derived runtime. Game-facing work lands on the `skylanders` branch; general
  fixes stay on their own short branches (`fix/*`, `feat/*`) and are merged. Game-specific code stays in the
  game repos; only generally useful runtime changes go here.
- **Notable findings (all read-only; redo the idea):** separate render rate from guest vblank so a game that
  paces on real time is not forced to 30 FPS — idea. Texture replacement keyed by content hash, accepting any
  size/mip count, is enough for HD packs — idea. Achievement metadata as editable TOML and a runtime event API
  keeps achievements out of game code — idea. Save the previous version before every save and restore it if a
  save was interrupted — idea. `--window_hidden` makes headless asset export invisible — idea. Keeping valid
  CLI options when one is malformed avoids losing the whole command line — idea.

### `TheBiemGamer/GiantsRecomp` (read-only)

- **Licence / reuse:** Read-only here. README and `LICENSE` say MIT, but it is listed without a licence
  and GitHub reports `NOASSERTION`; no code is reused.
- **Last activity:** 2026-09-29, release `0.1`.
- **What works:** Giants story mode from start to playable levels on Windows; software portal with up to 16
  figures, real USB portal, figure dumping to file, keyboard/mouse controls, ultrawide camera and HUD, logo
  skip, F4 settings, achievements overlay, per-game installer. Linux builds and runs but is experimental with
  a red tint; macOS presets exist but are untested.
- **Approach:** ReXGlue with a portal submodule; game hooks for ultrawide; Ghidra headless workflow for naming
  functions and keeping `config/default.toml` in sync. It is the project the `tsapZ` ports started from.
- **Notable findings (all read-only; redo the idea):** portal read/write wrappers `sub_82403BB8` /
  `sub_82403C28` reached through XAM ordinals 1185/1186; the game does not import a portal function — idea.
  Portal wire protocol and figure format are documented — idea. Jump tables in the Bink decoder needed real
  sizes; the same class of codegen-analysis fix as the later ports' late function starts — idea. HUD
  ultrawide works by hooking the UI camera's orthographic projection and widening the screen object, so the
  game's own layout anchors to the real edges — idea. A headless Ghidra rename workflow that writes names
  into the codegen config is a better reverse-engineering loop — idea.

---

## 2. Ideas worth taking

Rows are ordered by value ÷ effort (high=3, medium=2, low=1), highest first, with ties broken by higher
value. Every source is read-only: its code is never copied, adapted or translated; the implementation column is our
own design.

| # | Idea (what, where) | Licence | Games | Value | Effort | How we would do it in our runtime |
|---|---|---|---|---|---|---|
| 1 | **Portal prompt watcher → auto-open the portal UI** for the player the game is asking, plus companion requests. Sources: `SSARecomp/docs/portal.md`, `GiantsRecomp/docs/portal.md`, `TrapTeamRecomp/docs/portal.md`, `SwapForceRecomp/docs/portal.md`. | MIT; adapt with attribution | SSA, Giants, later | High | Low | Add a per-game prompt adapter to our runtime: SSA fixed object/text, Giants/Trap Team script variables, Swap Force/SuperChargers/Imaginators magic-moment fields. Map the state to a player and open the Toy Box portal UI for that player; add trap/vehicle tabs only where the game asks. |
| 2 | **Headless asset exports for Arkeyan Launcher** (`--export_title_music`, `--export_pictures`, achievements, icon). Sources: `skylanders-portal` `src/portal_rex/title_music.*`, `settings_schema.*`; `SkylandersCoreOfLight/docs/launcher.md`. | Portal read-only; Core of Light MIT | All | High | Low | Add flags to our runtime that decode title music, figure/hat/upgrade/achievement art and the game icon from the player's disc, write them to the shared cache and exit before a window. Arkeyan Launcher calls them hidden and muted. |
| 3 | **Save backup, rollback and crash recovery.** Sources: `SkylandersCoreOfLight/docs/launcher.md` (saves); `rexglue-sdk` `src/system/xam/content_manager.cpp`; `SSARecomp/docs/STATUS.md`, `SwapForceRecomp/docs/STATUS.md` (interrupted save). | Hub MIT; SDK read-only | All | High | Low | In our save layer keep the previous file, write atomically, and add periodic snapshots while playing. Arkeyan Launcher shows the copies with timestamps and restores one after backing up the current save. |
| 4 | **Deterministic input script and diagnostics** (`tap`, `hold`, `save`, `dump`, `stacks`). Sources: `SSARecomp/README.md`, `SSARecomp/docs/STATUS.md`, `SwapForceRecomp/src/dev/input_script.cpp`. | MIT | All | High | Low | Put a scriptable input device and memory-save/dump commands in our test harness, plus a thread stack walk symbolized by guest function. Use it for boot, portal and high-FPS regression runs. |
| 5 | **Movie skip and 16:9 pillarbox** via the Bink header. Sources: each game's `docs/STATUS.md` (Giants, Swap Force, Trap Team, SuperChargers, Imaginators). | MIT | All | High | Low | Hook BinkOpen after the 44-byte header read, set logo/opening movie frame counts to 1; tell our presenter to letterbox while a movie plays. Keep both as separate settings. |
| 6 | **Headless Ghidra rename → symbol/config sync.** Source: `TheBiemGamer/GiantsRecomp/tools/ghidra_rename_and_export.py`, `docs/reverse-engineering.md`. | Read-only; redo idea | All | High | Low | Build our own PyGhidra wrapper that dumps a function, renames it in the project and updates our symbol/config file in one step; require a clean diff and a boot test afterwards. |
| 7 | **Late function-start detection.** Sources: `TrapTeamRecomp/docs/STATUS.md`, `SuperChargersRecomp/docs/STATUS.md`, `ImaginatorsRecomp/docs/STATUS.md`, `config/default.toml`. | MIT | Later games | High | Low | Add an analysis pass for a start inside the previous function, wrong jump-table sizes and the `blr`-followed-by-unknown-start shape; emit candidate entries with evidence, then boot-test each. |
| 8 | **Per-game portal identity and compatibility** in the emulated portal. Source: `SwapForceRecomp/docs/portal-compatibility.md`. | MIT | All | High | Low | Let the portal backend answer `R` with the ID the running game expects, expose the detected model to Arkeyan Launcher, and warn on a real portal whose ID the game rejects instead of showing a generic failure. |
| 9 | **High-FPS validation recipe** (swap interval, fixed-distance walk, jump/fall checks). Source: `SwapForceRecomp/docs/high-framerate.md`. | MIT | All | High | Low | Keep a frame-rate setting with 30/60/display/manual; validate each game by fixed-time movement, cutscene pace and scripted falls. Scale frame-count thresholds by the game's own delta time. |
| 10 | **Dump a real figure to the collection.** Sources: `TheBiemGamer/GiantsRecomp/README.md`, portal `src/portal/figure_file.*`. | Read-only; redo idea | All | High | Low | Read the 1024-byte figure from the real portal, save an atomic copy in the shared collection and never write to the source unless the game writes it. Show the copy in the Toy Box immediately. |
| 11 | **Console-region override for languages.** Source: `GiantsRecomp/docs/game/giants.md`, `docs/STATUS.md` (`0x8217F2D8`, hook `0x8217F350`). | MIT | Giants, later | High | Low | Add a region setting and set it before the game's language setup; keep "auto" as the default. Test each European language pack that exists on the player's own disc. |
| 12 | **Cross-game instruction-window signatures** for hooks. Sources: `GiantsRecomp/src/hooks/ultrawide_hook.cpp` (42 of 48 instructions), `SwapForceRecomp/docs/high-framerate.md` (swap queue shape). | MIT | All | High | Low | Extend our sigmatch database with short instruction windows per anchor (swap queue, perspective builder, BinkOpen, prompt object), match them across XEX builds, and require a manual review of each hit. |
| 13 | **Portal overview and selective removal.** Sources: all six `docs/STATUS.md` (portal overview entry, 2026-10-25). | MIT | All | Medium | Low | Add a portal screen that lists every occupied slot with its owner and lets the player remove one item/figure without lifting the Skylander. |
| 14 | **Problem report with redaction.** Source: `SkylandersCoreOfLight/docs/launcher.md`. | MIT | All | Medium | Low | Have Arkeyan Launcher collect the newest logs, settings and environment into one plain-text file, replace user name, computer name and home paths, and show it before upload. |
| 15 | **Controller remap and per-player vibration.** Source: portal `src/portal_rex/portal_menu_input.cpp`; `SwapForceRecomp/docs/STATUS.md`. | Portal read-only; docs MIT | All | Medium | Low | Put button mapping and rumble strength in our input settings, apply per player, and mirror the page in Arkeyan Launcher. |
| 16 | **Portal menu drawn with the game's own fonts, portraits, hats and upgrade trees.** Sources: all six game repos; portal `src/portal_rex/portal_menu.cpp` and `game_font.h`; `SwapForceRecomp/docs/portal.md`. | MIT (portal read-only) | All | High | Medium | Build per-engine asset readers (see #23), cache decoded art and fonts, and render cards in our UI with the game's element colours, variants, companion rules and details view. Fall back to our own art when a game lacks something. |
| 17 | **Figure workshop** (create, edit variants, swappers, traps, vehicles, crystals; signed import). Sources: `GiantsRecomp/docs/figures.md`; portal `src/portal/figure_file.*`, `figure_catalog.*`; `SkylandersCoreOfLight/docs/launcher.md` (Imaginators import). | Read-only idea + MIT docs | All | High | Medium | Implement our own 1024-byte figure builder with checksums, variant catalog, top/bottom swapper builder and per-game companion groups. For signed Imaginators toys, add a local check through the game's own verifier equivalent; never bundle dumps. |
| 18 | **Achievements as a runtime system** (catalog, toast, overlay, profile persistence). Sources: portal `src/portal_rex/achievements_ui.cpp`, SDK `src/system/achievement_manager.cpp`, `SkylandersCoreOfLight/docs/launcher.md`. | Portal/SDK read-only; hub MIT | All | High | Medium | Register a TOML catalog per game, expose an unlock API to hooks, draw a native toast/overlay, persist per profile. Keep the unlocked list in `<profile>/<game>/achievements/<title id>.toml`. |
| 19 | **HD texture dump/replace by content hash.** Sources: SDK `src/graphics/d3d12/texture_cache.cpp`, `SwapForceRecomp/docs/texture-packs.md`. | SDK read-only; doc MIT | All | High | Medium | Add a backend-neutral texture layer in our plume path: dump each top mip as DDS, load `<hash>.dds` replacements of any size/mip count, keep BC1/BC3 and alpha-test rules. Store packs per game under the shared data folder. |
| 20 | **Ultrawide done properly**: Hor+ camera, HUD anchored to the window, movies pillarboxed. Sources: `GiantsRecomp/docs/STATUS.md`, `SwapForceRecomp/docs/STATUS.md`, TheBiemGamer HUD hook. | MIT + read-only idea | All | High | Medium | Widen aspect and FOV at every camera/perspective caller so culling follows; give the UI its real expected width instead of stretching; force 16:9 bars only while a movie plays. |
| 21 | **Settings schema export so the launcher mirrors the game.** Sources: portal `src/portal_rex/settings_schema.cpp`, `SkylandersCoreOfLight/docs/launcher.md`. | Portal read-only; hub MIT | All | High | Medium | Have our runtime emit a JSON schema of its settings (type, range, default, `enabled_by`, start-up-only) and let Arkeyan Launcher render/edit it. Pass a "launched by launcher" flag to hide start-up-only rows in-game. |
| 22 | **Profiles with separate saves, figures and achievements.** Source: `SkylandersCoreOfLight/docs/launcher.md`. | MIT | All | High | Medium | Make profiles first-class in Arkeyan Launcher and pass a profile id/name to the runtime. Keep each profile's saves, figure collection and achievement state in its own folder; migrate existing data once. |
| 23 | **IGA/IGZ readers for Giants and later games** (portraits, fonts, upgrades, hats, toy data). Sources: `GiantsRecomp/docs/game/giants.md`, `TrapTeamRecomp/docs/STATUS.md`, `SuperChargersRecomp/docs/STATUS.md`, `ImaginatorsRecomp/docs/STATUS.md`. | MIT | Giants, later | High | Medium | Implement versioned readers in our runtime/tooling: IGA 8/10/11, IGZ 6/7/8/9, little/big-endian headers, image and material records. Unit-test with synthetic fixtures, never with shipped game data. |
| 24 | **Hybrid real + virtual portal with write-back.** Source: portal `src/portal/hybrid/real_portal_bridge.cpp`; `SwapForceRecomp/docs/hybrid-portal.md`. | Read-only; redo idea | All | High | Medium | Build a portal source multiplexer: virtual slots for the Toy Box, real slots for USB, per-slot ownership, and writes routed to the originating figure file. Define what happens on conflict and on unplug. |
| 25 | **Cross-disc language text/voice/movie tool** for the player's own discs. Sources: `SuperChargersRecomp/tools/german_text.py`, `ImaginatorsRecomp/docs/STATUS.md`. | MIT | Later games | High | High | Build a local-only tool that reads two discs the player owns, matches language entries and voice/movie tracks, writes a merged copy with checksums, and supports restore. Never distribute the result. |
| 26 | **Online co-op with each player's own portal.** Source: `SkylandersCoreOfLight/docs/online-coop.md`. | MIT | All | High | High | Use an existing remote-play transport for video/audio/controller; add our own small authenticated figure channel. Host exposes a network portal source; guest runs a portal client; sequence-numbered write-back and a journal protect figure progress. |
| 27 | **Native scene at output resolution** instead of the console's small target. Sources: `SwapForceRecomp/docs/native-renderer.md`, game STATUS. | MIT docs | SSA, later | High | High | Use a counting build to map passes, then draw the scene directly at the output size through plume, with our own resolve/copy and no EDRAM tile limit. Keep the current path as an A/B fallback until parity is proven. |
| 28 | **Trap Team portal speaker** (G.726 stream, volume, speaker indicator). Source: `TrapTeamRecomp/docs/portal.md`, portal `src/portal/g726.cpp`. | MIT + read-only idea | Trap Team | Medium | Medium | Decode the portal's speaker stream into our audio mixer, expose a volume setting, and show who is talking in the portal overlay. |

Other useful but smaller items: per-game companion slot rules (traps/vehicles/crystals), disc-icon extraction,
mouse-wheel navigation, and "Nightmare from the start" style difficulty unlocks. They are cheap polish once
the larger items exist.

---

## 3. What we already do better

- **Our runtime is ours.** No Xenia and no ReXGlue dependency: we own the recompiled-code/runtime boundary
  and the GPU path through plume. Their ports inherit Xenia's runtime and its D3D12 assumptions.
- **Three graphics APIs, three operating systems.** Metal, Vulkan and D3D12; macOS, Linux and Windows. Their
  six ports are Windows/D3D12 only; TheBiemGamer's Linux build is experimental and its macOS presets untested.
- **The player kit builds from the player's own ISO on the player's machine.** Arkeyan Launcher downloads the kit,
  `Arkeyan-Builder` does recompilation, shaders and linking locally. Their installers ship prebuilt recompiled
  executables and copy the player's disc files, with no compilation on the player's machine.
- **Xbox Series X package.** We already boot our SSA build on a Series X in Dev Mode; none of these projects
  has a console target.
- **Real portal coverage.** We already handle the wired `1430:0150` portal and the Xbox One GIP portal. Their
  shared library supports `1430:0150` and `1430:1F17` and explicitly does not support Xbox One portals;
  ArkeyanRuntime's real-USB path is still marked untested.
- **A Toy Box and a shared collection as product features**, not only a library screen: figures, portraits,
  ownership marks and per-game presentation live together.
- **One cross-platform launcher and one data folder** for the game, saves, figures, pictures and cache.
  Core of Light is a strong Windows launcher, but it is Windows-only and each game still has its own
  installer.
- **Our own hook/symbol inventory and sigmatch.** 293/293 anchors found on SSA gives us a tested base for
  porting the same hooks to Giants and later games instead of rebuilding the list per title.
- **No game data in the product.** Both sides respect this; ours also builds the executable locally rather
  than distributing a recompiled game binary.

---

## 4. Facts useful for Giants and the later games

### 4.1 Portal wire protocol (shared by all six)

- Frames are **32 bytes**. On Xbox 360 a frame is a `0B 14` header plus a 30-byte payload; on the raw
  USB side it is a 32-byte payload with no header. The payload's first byte is the command.
- Commands: `R` ready poll; `A <0|1>` activate/deactivate; `S` status; `C R G B` LED; `Q <slot> <block>`
  read one 16-byte block; `W <slot> <block> <16 bytes>` write one block.
- Status frames carry 2 bits of state per slot (empty/present/removing/added), a counter and an active flag.
- A figure is **1024 bytes = 64 blocks × 16 bytes**. The game validates block 0 and shows a problem-toy
  message for invalid data; progress is written back to block 8 and others while the figure is on the portal.
- The game resolves the portal API at run time as XAM ordinals **1185/1186** (`0x4A1`/`0x4A2`); it does not
  import a portal function. Giants' wrappers are `sub_82403BB8` (read) and `sub_82403C28` (write).
- Non-Xbox portals use USB `1430:0150`; Xbox 360 portals use `1430:1F17`; Xbox One portals use GIP and are not
  supported by their library.
- `R` reply IDs: Spyro's Adventure `01 29` (Xbox 360 `01 28`), Giants `01 3D`, Swap Force `02 03`,
  Traptanium `02 18`/`02 1B`, SuperChargers `02 0A 03 02`, Imaginators `02 0A 05 08`. Trap Team rejects
  non-Traptanium portals; Swap Force warns on older ones and rejects some IDs. The launcher can name the
  exact model from this.
- Sources: [portal protocol](https://github.com/TheBiemGamer/GiantsRecomp/blob/main/docs/portal-protocol.md),
  [compatibility table](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal-compatibility.md),
  [portal menu and protocol](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/portal.md).

### 4.2 How each engine asks for a figure

| Game | Prompt anchor | What to read |
|---|---|---|
| Spyro's Adventure | prompt object `0x82C569B0`; HUD object `0x82C56AF8` | shown text hash (`HUD +3260 -> +376 -> +432`) distinguishes P1/P2 and "too many"; object fields alone do not |
| Giants | script state machine | `ScriptReference` vtable `0x820DF750`, name `+8`, value `+0x1C`; "current state name" and "portal error string" |
| Trap Team | script state machine | `ScriptReference` vtable `0x821095A8`, name `+8`, value `+0x1C`; player-state and portal/tag-state variables |
| Swap Force | `CMagicMoment` instance `0x83131BE0` | state `+0x20`, player `+0x148` |
| SuperChargers | `CMagicMoment` instance `0x83764ED0` | state `+0x2C`, reason `+0x168`, error type `+0x1F8`, player `+0x1FC` |
| Imaginators | `CMagicMoment` instance `0x837F649C` | state `+0x40`, reason `+0x158`, error type `+0x1CC`, player `+0x1D0` |

Offsets are executable/version specific. The reliable rule is: read the game's own prompt/error state, not
just "a figure is missing"; the "too many" screens can look like a player request.

### 4.3 Frame rate and rendering anchors

| Game | Anchor | Finding |
|---|---|---|
| Spyro's Adventure | swap queue `sub_822A8ED8`, vblank `sub_822A8DB8`, callback `0x822A4000` | interval is already 1; 30 FPS comes from the emulated vblank running at 30 Hz without a limit; raising the guest vblank gives 50-91 FPS in Chapter 1 without changing game speed |
| Giants | swap queue `sub_823D9758` | same 107-instruction routine as Swap Force; already interval 1, so no frame-rate hook; 60/144 works via the guest vblank |
| Swap Force | queue `sub_82F5C258`, vblank `sub_82F5C128`, callback `0x82F58198` | interval lives in bits 8-11 of the swap word; hook the interval, then raise the limit; stuck-fall `sub_82A4A828` must scale with `dt` |
| Swap Force | camera `sub_82B992A8`, perspective `sub_824906D8`, call `0x82B993A4` | Hor+ ultrawide; HUD needs its own expected-width fix |
| Trap Team | scene target `0x8243F650`, perspective `sub_823B1AF0`, callers `0x8230F784`/`0x8230F7F0` | native scene 1264x704; widen all camera/culling callers |
| SuperChargers | scene `0x826E8718`, perspective `sub_8250E610`, caller `0x82B0BCA0` | native scene 1264x704; same pattern |
| Imaginators | scene `0x826F96D0`, perspective `sub_82547C30` | native scene 1264x704; same pattern |
| Giants | scene target cap `sub_821939E0`, helpers `sub_8218ED90`/`sub_8218EDD8` | code caps the scene at 1000x600; above 2048 EDRAM tiles the 360's planning heap breaks, so native 3D is off until that is moved |
| Swap Force | scene buffer 1024x624, final 1280x720 | 1280x720 native needs 2880 EDRAM tiles and exposes engine assumptions; 1264x704 is the safer target |

Movie skip: BinkOpen reads a 44-byte header (`r1+96` in Giants, `r1+112` in Trap Team/SuperChargers/
Imaginators); setting a movie's frame count to 1 removes it. The opening movies are `FMV01.mov` (Giants),
`FMV1.mov` (Trap Team), `FMV01.bk2` (SuperChargers), `FMV01_Cinematic.bk2` (Imaginators). Sources:
[Giants STATUS](https://github.com/tsapZ/GiantsRecomp/blob/main/docs/STATUS.md),
[Swap Force high FPS](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/high-framerate.md),
[Trap Team STATUS](https://github.com/tsapZ/TrapTeamRecomp/blob/main/docs/STATUS.md),
[SuperChargers STATUS](https://github.com/tsapZ/SuperChargersRecomp/blob/main/docs/STATUS.md),
[Imaginators STATUS](https://github.com/tsapZ/ImaginatorsRecomp/blob/main/docs/STATUS.md).

### 4.4 Discs, archives and engine formats

| Game | Title ID | Archives / scenes | Notes |
|---|---|---|---|
| Spyro's Adventure | `415608AB` | own formats: `.lxb`, `.str`, `.lsa`, `.bnk/.pck`, Bink 1 | 10 languages; 1120x704 scene; no Alchemy `.pak` |
| Giants | `415608DA` | IGA v8 little-endian; IGZ v6 | 12 language packs; Bink 2 `.mov`; region hook |
| Swap Force | `415608E9` | IGA v10 big-endian; IGZ v7 | 2 UI languages; swappers; HD texture pack method |
| Trap Team | `41560911` | IGA v11 little-endian; IGZ v8 big-endian | 214x107 DXT5 portraits; traps; portal speaker |
| SuperChargers | two EU pressings (En/De and Es/It) | IGA v11 big-endian; IGZ v9 | toy data in `permanent.pak`; 88x88 DXT5 icons; vehicles |
| Imaginators | `41560930` | IGA v11 big-endian; IGZ v9 | signed toys; PS3 disc has all languages; German movie tracks |

Each game's `docs/game/` folder records the exact disc revisions and their hash checks, e.g.
[Giants](https://github.com/tsapZ/GiantsRecomp/blob/main/docs/game/giants.md),
[Swap Force](https://github.com/tsapZ/SwapForceRecomp/blob/main/docs/game/swap-force.md),
[Trap Team](https://github.com/tsapZ/TrapTeamRecomp/blob/main/docs/game/trapteam.md),
[SuperChargers](https://github.com/tsapZ/SuperChargersRecomp/blob/main/docs/game/superchargers.md),
[Imaginators](https://github.com/tsapZ/ImaginatorsRecomp/blob/main/docs/game/imaginators.md),
[Spyro's Adventure](https://github.com/tsapZ/SSARecomp/blob/main/docs/game/spyros-adventure.md).

Useful engine facts:

- Giants' collection portraits are 256x256 DXT5 in `character/NNN_<model>[_<deco>].bld`; Trap Team's are
  214x107 DXT5 in `level.bld`; SuperChargers/Imaginators put toys in `permanent.pak` and icons in
  `ToyCollectionMaterials.pak`.
- Giants/Trap Team use a script-driven "Portal Control" state machine; Swap Force and later use
  `CMagicMoment`. Spyro's Adventure is the outlier with a fixed prompt object.
- Giants' language set comes from console region: `XGetGameRegion` `0x201` or `0x2FE/0x2FF` selects the
  European set; anything else selects North America. A hook after `sub_8217F2D8` at `0x8217F350` changes it
  without touching the disc.
- SuperChargers has two European pressings; the En/De disc carries German text and voices. Imaginators'
  360 disc is Spanish/Italian, while the PS3 disc has all languages; both ports use a local cross-disc tool.
- Imaginators' signed toys are Ed25519: signature blocks 2, 4, 34 and 62 over blocks 0 and 1. Import must go
  through the game's own verifier (the port's `--check_figures` pattern), not a homemade acceptance rule.
- Figure progress is written back to the originating 1 KB file. Keep atomic writes and a previous-save copy;
  Swap Force recorded an empty save after a crash during save load, and the SDK now keeps the previous save
  to recover it.

### 4.5 Launcher and packaging facts

- Games can be found from an uninstall entry, a games folder, `coreoflight.ini`, next to the launcher, or a
  developer build; copied installs are found by their `rom\` and can be updated in place.
- `default.xex` SHA-256 is the cheap, reliable disc identity. Store one hash per supported pressing and show
  a clear "different disc version" state instead of a generic boot failure.
- A game can expose its settings pages headlessly (`--settings_schema`) and its art/music
  (`--export_pictures`, `--export_title_music`) in about a second; the launcher can be complete before the
  first play.
- `--window_hidden` plus a hidden, muted start makes those exports invisible. A bad CLI option should not
  discard the valid ones.
- Saves: previous copy by the runtime, launcher backups, automatic copies at start/end/every 60 minutes,
  newest 10 kept. Restore backs up the current save first.
- Problem reports are one plain-text file with environment, settings and recent logs; redact user name,
  computer name and home paths.
- Online co-op design: remote play for video/audio/controller; a separate small TCP channel for the guest's
  real portal figures, with pairing, per-player slots, sequence-numbered write-back and a journal that
  refuses to overwrite newer progress.

---

## 5. Other projects they credit, and what to read there

| Project | Why it matters | Reuse |
|---|---|---|
| [Cemu graphic packs for Skylanders](https://github.com/cemu-project/cemu_graphic_packs) | Per-game resolution-dependent buffers, high-FPS bugs and HUD/FOV floats (Giants, Trap Team, Swap Force, SuperChargers, Imaginators). | Cemu is GPL; read the pack docs/config only, do not copy code. |
| [DimensionsRecomp](https://github.com/NeverCookFirst/DimensionsRecomp) | A ReXGlue port with a virtual toy pad and real-portal support; useful UX and portal-source patterns. | Read-only; licence separately. |
| [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp) | Native renderer, per-element HUD anchoring, high-FPS clamping, resolution presets. | GPLv3; read public docs, do not copy code. |
| [Skate 3 Recomp 2.0](https://github.com/mchughalex/skate3recomp) | Native scene renderer fed from the game's own structures; useful feasibility reference for later titles. | Read-only. |
| [SkyReader](https://github.com/reedstrm/SkyReader) / [Marijn Kneppers' write-up](https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/) | Figure layout, save data and checksums. | Read facts; do not copy code or dumps. |
| [Skylanders Ultimate NFC Pack](https://skylandersnfc.github.io/Skylanders-Ultimate-NFC-Pack/) | Public id/name/variant catalog used by the portals. | Use names/ids only; do not ingest or redistribute dumps. |
| [Xenia](https://xenia.jp) / [ReXGlue](https://github.com/rexglue/rexglue-sdk) | Runtime and GPU behaviour reference. | Read-only; keep their licences. |

The recurring lesson is that the six ports solved the **game-facing** problems (portal protocol, prompt
detection, native UI, high FPS, ultrawide, saves, packaging) with relatively little title-specific code.
Our advantage is the runtime and platform layer underneath; the work worth taking is the game knowledge,
not the ReXGlue plumbing.
