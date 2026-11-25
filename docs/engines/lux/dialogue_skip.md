# Skipping spoken lines in the Lux games

> **Purpose:** how SSA plays a spoken line inside a cinematic, and how the port lets B skip the lines the game keeps unskippable · **Status:** current (2026-11-25) · **Tags:** [Lux] [SSA]

Code: [`engines/lux/dialogue_skip.cpp`](../../../engines/lux/dialogue_skip.cpp) (the hooks),
[`engines/lux/dialogue_skip.h`](../../../engines/lux/dialogue_skip.h) (the rules, checked by `ArkeyanDialogueSkipTest`),
addresses in [`games/ssa/addresses.h`](../../../games/ssa/addresses.h). Check: `SSA_ISO=<disc>
tools/regress/ssa_dialogue_skip.sh <name>`. Class **[Lux]** structure, **[SSA]** addresses.

## What the player gets

While a cinematic the game does not let the player skip shows a spoken line, or while a line freezes the hero
(Blobbers' lines in level_027, with no cinematic), B (the button that skips cutscenes) closes the line and stops its
voice. The cinematic or the level's script then goes on to its next line or its end. Each press closes one line,
and only after the line has spoken for half a second, so tapping B goes line by line. A cinematic that the game skips
itself (B ends it whole) and a line the game lets the player close with A are left as they are. While the player is in
control, or while the game is paused, B does what the game does. No setting: a player who presses nothing gets the original game.

## How the game plays a line [V] (level_027, 2026-11-24)

- **The box**: `lux::UITextDialog` (RTTI). Its open (`sub_826C10B0`) starts the voice through the sound manager
  (`sub_827FDE48`, a Wwise event posted with the end-of-event and marker callbacks), its update (`sub_826C1930`, hooked
  as `TextDialog_Update`) types the text in time with the voice's markers and turns its pages. Fields: +12 done (the
  box closes, its voice stops), +63 the hand advance (A shows the whole text, then closes the box; 0 in every line of
  level_027: there A held only types faster), +84 the voice (its +8 is the playing id, cleared by the end callback
  `sub_828657E0`).
- **The cinematic**: `lux::CinemaTrigger`. Its update (`sub_827C79A0`, `Cinematic_Update`, f1 its time step in
  seconds) ends it when a press of the skip mask (`game::SkipMask`, B) finds its own skip on (+498, or +496 without
  +497), or when one of its tracks has ended and no line holds it. Its end (`sub_827C3338`, `Cinematic_End`) runs in
  both cases. The tracks are a list at +84 (node +20 the track); each track is an animation stepped by
  `sub_8289BE28`: time (+240, frames) += rate (+40, 1 a 60th of a second) * the step, clamped at the end (+248), where
  +272 counts the ends. Its step is 0 while it waits for a line (the cinematic's clock stops until the box closes).
- **The hero's lock**: the word at `game::ControlLocks` holds one bit per reason the hero cannot move (0x10000 the level
  script's `game_lock_control`, 0x100000 a box whose byte +61 freezes the hero, set by the box's open through
  `sub_826C0B10`), and the byte at `game::ControlLockCount` counts other locks; the script command's own query
  (`sub_829382A8`) reads both. A line shown with the hero locked and no cinematic (Blobbers' "Hey! You're a
  Skylander", 16 s) waits for its box: closing the box goes on as its end would.
- level_027: the first cinematic (the level's opening) has its own skip on; the second (the end of the cutscene and
  Hugo's speech, frames 0 to 1672, ~32 s) has it off, and Hugo's 20 s line plays while its clock runs to the end.

## Why the skip plays the cinematic fast

Ending such a cinematic through its own skip path soft-locks level_027: the save and the visiting-Skylander dialog that
follow its natural end never come (tested at 44 s and at 58 s, 3.5 s before the end). Closing the box alone stops the
voice but leaves the player waiting for the clock. So after a skipped line whose cinematic's clock runs, every track
plays 16 times faster (`lux::FastTime`, never past one step before its end, so the game's own step reaches the end and
reports it) until the next line starts or the cinematic ends: every step of the cinematic still runs, only sooner. A
line the cinematic waits for needs nothing more: once the box closes the clock goes on by itself.

Measured (`ssa_dialogue_skip.sh`): B at 45 s closes Hugo's line, the level's save follows at 45.8 s instead of ~62 s.

## Not done yet

- The Alchemy games (Giants to Imaginators, e.g. Trap Team's Sheep Creep introduction): their dialogue code is not
  mapped yet.
- Lines while the player is in control stay as the game plays them (B attacks there).
