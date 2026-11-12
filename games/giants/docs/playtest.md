# Giants: the playtest checklist (for human testers)

> **Purpose:** what a person checks when playing Giants on our port, and how to report it · **Status:** current (2026-10-26) · **Tags:** [Giants]

The goal: play Giants from start to end on our port and report every problem. Automated runs reach Chapter 1 and load
every level for 80 s (the tour); playing through needs a person.

## Before
- Use the latest build: Arkeyan Launcher with Giants (kit 0.5.0), or a build of this repo (`cmake -S . -B build-giants -G Ninja -DARKEYAN_GAME=giants`).
- Note your machine: OS, CPU, GPU, screen size and scale, gamepad or keyboard, real portal (which one) or the emulated
  portal (Toy Box).
- Known open issues, so you need not report them: [status.md](status.md) ("Open problems"). Do report a movie whose
  sound and lips drift apart.

## Checklist
Tick each line, or write what went wrong.

### Every chapter, in order
- [ ] Each chapter loads, plays to its end and saves (the progress screen and the save message).
- [ ] Cutscenes and dialogues: the text and voices are right, nothing hangs.
- [ ] Every boss fight to the end.
- [ ] The ending and the credits.
- [ ] After the ending: the game returns to the menu, and the save holds the completed story.

### Modes and menus
- [ ] Arena challenges (several, including a long one).
- [ ] Battle Mode (two players: the second on a gamepad or keyboard).
- [ ] Every menu: options, the collection screens, upgrades, hats, the portal prompts.
- [ ] Changing the language and the voices; the subtitles follow.

### Figures and portal
- [ ] Place, swap and remove figures during play: in the Toy Box (emulated), and on a real portal if you have one.
- [ ] Player 2 drops in and out.
- [ ] A figure keeps its level, gold and upgrades after quitting and coming back.
- [ ] Giants figures, LightCore (they light up), sidekicks, magic items, adventure packs.

### Quality
- [ ] No crash or freeze over one hour of play.
- [ ] Frame rate: smooth in menus and levels. Note any level where it drops, and any stutter on the first visit.
- [ ] Sound: music, effects and voices clean (no crackle, no repeat, no silence).
- [ ] Rendering: nothing missing, stretched, flickering, too dark or too bright. Compare with your memory of the
  original, or a video of it.
- [ ] Saves: quit in the middle of a level, start again, and continue.

## Report
1. Note the time, the chapter or menu, and what you did just before.
2. In Arkeyan Launcher: Support > **Report a problem**. It gathers the logs with personal paths removed. Save the file and
   attach it.
3. One problem per report. A screenshot of the **game window only** helps (never a full screen).

Each report becomes an issue (`Arkeyan-Launcher`, label `giants`) with its log. Fixed issues are listed in
[status.md](status.md), and this checklist is reset at each release.
