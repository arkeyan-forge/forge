# SSA: the playtest checklist (for human testers)

> **Purpose:** what a person checks when playing SSA on our port, and how to report it · **Status:** current (2026-10-26) · **Tags:** [SSA]

The automated runs reach `level_027` ([testing.md](../../../docs/getting-started/testing.md)); everything else needs a
person.

## Before
- Use the latest build: Arkeyan Launcher (INSTALL from your disc), or a build of this repo (`cmake -S . -B build -G Ninja`).
- Note your machine: OS, CPU, GPU, screen size and scale, gamepad or keyboard, real portal (which one) or the emulated
  portal (Toy Box).
- Known open issues, so you need not report them: [status.md](status.md) ("Open problems").

## Checklist
Tick each line, or write what went wrong.
- [ ] **Title**: the intro movies play (or are skipped with the option), the title screen and its music.
- [ ] **Menus**: new game, the save slots, options; button pictures match your pad.
- [ ] **A level**: `level_027` and the levels after it load, play and save at the end; cutscenes, voices and text right.
- [ ] **Figures**: place one, swap it, remove it during play (the Toy Box, and a real portal if you have one); the
  portal prompt opens the Toy Box; Player 2 drops in and out.
- [ ] **Toy Box**: create a figure, edit it, put it on the portal; it keeps its level, gold and upgrades afterwards.
- [ ] **Save and reload**: quit in the middle of a level, start again, continue; the progress is there.
- [ ] **Settings**: language and voices, scale, widescreen and ultrawide, fps limit 30/60, HD textures, intro skip.
- [ ] **Quit**: from the game's menu and by closing the window; the next start is clean.
- [ ] **Quality**: no crash or freeze over one hour; smooth frame rate (note any level where it drops); clean sound;
  nothing missing, stretched or flickering.

## Report
1. Note the time, the level or menu, and what you did just before.
2. In Arkeyan Launcher: Support > **Report a problem**. It gathers the logs with personal paths removed. Save the file and
   attach it.
3. One problem per report. A screenshot of the **game window only** helps (never a full screen).

Each report becomes an issue on `Arkeyan-Launcher` with its log; fixed issues are listed in [status.md](status.md).
