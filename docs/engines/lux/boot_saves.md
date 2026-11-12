# Boot, the startup state machine, intro movies, saves

> **Purpose:** how SSA boots (intro movies, the startup state machine) and how it saves · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Class **[Lux]** structure, **[SSA]** addresses. Detail: [engine_notes.md](engine_notes.md) §3.2, §3.11, §3.15.

## Boot

Entry `0x822DF400` -> XAPI startup -> game. `GameMaster_IntroMovies` (`0x827F5E80`) is a **main-thread loop over the four intro movies**: `movies\<lang>\legal.bik` (localized, **4.95 s**), `atvilogo.bik` (**10.38 s**),
`xpeclogo.bik` (**8.51 s**), `tfb.bik` (**10.0 s**) [Bink headers]. `Movie_Update` (`0x82852D90`): `BinkWait`/`BinkDoFrame`/`BinkNextFrame`, done when `FrameNum == Frames`. **A/START skips all but the legal screen** (title at ~20 s).
Before vblank pacing the movies ran at 6-7 game frames/s and the title came at ~65 s; paced, the title comes at ~36 s. After START: autosave notice -> "CHOOSE PLAY" menu -> Skyland Adventures -> Start a New Game -> slot -> story movie
`movies\sl_sc01.bik` (62.3 s, skippable with B only) -> first level **`level_027`** (`levels\level_027\mission.str`) -> in-level cutscene (B skips) -> Hugo's lines (~40 s, not skippable) -> "This Skylander is visiting from another
adventure" dialog (a figure first used on another console: owner id) -> play.
Scripted run: `SKY_PRESS="start:40,a:45,a:57,a:62,a:67,a:72,b:150,b:160"` (old, 240-300 s) or the fast recipe in [../testing.md](../../getting-started/testing.md) (~75 s). The test aid `SKY_SKIP_MOVIES=1` hooks `Movie_Update`: it lets one frame play then clears the "playing" flag
(`movie+0x80`) and returns 0, which both callers (the intro loop and the story movie state) treat as finished.

## The startup state machine (object `0x82C91EE8`, constructor `sub_82800048`) [V]

Table of **16-byte entries `{handler, 0, id, name}`**, current state at **`+0x1250`**, previous at `+0x125C`. Observed order:
`IDLE(0)` -> `WAIT_FOR_START_BUTTON(5)` -> START -> `GAME_STARTING(7)` -> `INIT_PROFILE(0xE)` -> `CHECK_FOR_STORAGE(0xF, XamShowDeviceSelectorUI)` -> `LOAD_GLOBAL_GAME_DATA_PROGRESS(0x10)` -> `GAME_INIT_SYSTEMS(0x13)` ->
**`GAME_FINALCHECKS(0x14)`** -> `GAME_INITIALIZED(0xC)` -> (A) -> `GAME_SAVE(8)` -> story movie.
**`STATE_GAME_FINALCHECKS` lasted ~22 s** with stubbed imports; it disappeared after the console-configuration + offline-network commit (XNetGetTitleXnAddr had stayed "pending" forever, XConfig/`XamGetSystemVersion` outputs were unwritten): if a
startup state hangs in another game suspect an async XAM/XNet call left pending or a stub with unwritten outputs first ([../xbox360/xam.md](../../runtime/xbox360/xam.md)). The startup state machine covers only the frontend (it is *not* a hook point for pipeline waits).

## Saves [Lux]

- Save manager singleton **`sub_8215B030()`**: `+0` state (1 = device selector pending), `+8` user, `+0xC` open-content hash, `+0x14` signed in, `+0x18` XCONTENT_DATA (device id), `+0x158` root `"lux"`, `+0x35C` XOVERLAPPED, `+0x378` file handle.
- `sub_82853428` shows the device selector; `sub_82853530` polls `XGetOverlappedResult` (`sub_822DE570`: returns `overlapped->InternalLow`; **0x3E5 = pending**); `sub_828536C0` opens content; `SaveSlot` `sub_828538E8`, `LoadSlot` `sub_828539F8`;
  Global exists/save/load `sub_82853BC0` / `CD0` / `DE8`; task switch `sub_8264D488`.
- Content names: "Skylanders - Global Data", "Skylanders - Slot 1..4" (formatted with `vsprintf`). Files: `GLOBAL.DAT` (196 bytes), `SLOT.DAT`, `SLOT.NFO`. Flags seen: `0x13` = OpenExisting probe of a slot that doesn't exist (-> 3, normal), `0x12` create, then `0x13` reopen.
- XAPI file calls go through a table at `*(0x82A1E308)`. `XamContentCreateEx`'s 9th argument and `NtCreateFile`'s options are both at **`r1+0x54`** (D12). Saves are time-stamped: XAPI `GetTimeZoneInformation` (`sub_822E4A18`) reads XConfig 3/1-3/7 and 3/12; `FileTimeToSystemTime` calls `RtlTimeToTimeFields`.

## Online, achievements, profile, region [Lux] [SSA]

Session code `sub_8254A4F8` treats `XNET_GET_XNADDR_NONE` as "no network". Achievements: unlock = `XUserWriteAchievements` = `XMsgStartIORequest(0xFB, 0xB0008)`, then the title enumerates its **38** achievements to check whether all but one are unlocked (the list and the unlocks are real now: [xam.md](../../runtime/xbox360/xam.md#achievements))
(`sub_8264ACF8`). One offline profile XUID `0xE000000000000001` on controller 0, all privileges allowed. `sub_822DF218` warns about the video output unless the AV pack is 3, 4, 6 or 8 and tests the AV region for PAL; country codes 5..109 map to territories.

## Crash guards in the boot path

- `sub_82631098` (`nglSceneTraverse`) and `sub_822895E0` (`nglRenderListSubmit`) crash on frames with nothing to draw (seen when the first movie starts): `patches.cpp` skips them ([ngl_renderer.md](ngl_renderer.md)).
- A rare heap corruption at frontend load (after `\ui\eng\hudmenu.str`): `sub_8267A880` builds a HUD object (skeleton `sub_828764B8`, shared handle `sub_82145F50`), the count reaches 0, `sub_82876460` -> `sub_82875D18` frees the matrix arrays, and the main
  heap's free `sub_828A25F8` merges with a neighbour marked free whose list links are 0 (**write to address 0**). Not reproduced in ~300 runs; the free guard skips it ([../gotchas.md](../../porting/gotchas.md), D1 was the original cause class).
