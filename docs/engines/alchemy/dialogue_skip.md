# Skipping spoken lines in the Alchemy games

> **Purpose:** how an Alchemy game plays a spoken line inside a cutscene, and how the port lets the skip button skip the lines the game keeps unskippable · **Status:** current (2026-11-27) · **Tags:** [Alchemy] [TrapTeam]

Code: [`engines/alchemy/dialogue_skip.cpp`](../../../engines/alchemy/dialogue_skip.cpp) (the hooks),
[`engines/alchemy/dialogue_skip.h`](../../../engines/alchemy/dialogue_skip.h) (the rules, checked by
`ArkeyanAlchemyDialogueSkipTest`), addresses in [`games/trapteam/addresses.h`](../../../games/trapteam/addresses.h).
The Lux side (SSA): [../lux/dialogue_skip.md](../lux/dialogue_skip.md).

## What the player gets

While a cutscene the game does not let the player skip holds the player, Y (V on the keyboard, the button that skips
cutscenes) stops the line being spoken and goes on to the next one. Each press skips one line, and only after the line
has spoken for half a second, so tapping goes line by line. A cutscene the game skips itself (Y ends it whole) is left
to the game. While the player is in control, Y does what the game does. No setting: a player who presses nothing gets
the original game.

| Game | State |
|---|---|
| Trap Team | done: Buzz's talk and Sheep Creep's introduction in `level_313_tutorial` checked on a Mac |
| Giants | not needed: no unskippable cutscene found (the tutorial's narration skips with Y) |
| Swap Force, SuperChargers, Imaginators | to check: find an unskippable cutscene, then match the three functions and the offsets |

## How the game plays a line [V] (Trap Team, 2026-11-27)

- **The cutscene**: script variables of the cutscene system, found by name as the scripts write them
  (`GAME_HOOK_ScriptValue_Set`, a `ValueInfo`: name at +8, integer at +0x20): `cutscene hid player ui` is 1 while a
  cutscene holds the player, `cs is skipping` once the game skips it itself, and `skippable` / `cs skippable`, written
  within half a second of the hold's start, say whether the game's skip may end it. Its steps run on a clock that
  follows the game's logic clock scaled by `_gameLogicScale` (`game::GameLogicScale`).
- **The line**: a sound of a voice category (`alchemy::IsSpokenLine`: "Voice/", the enemies' and NPCs' "Dialog",
  "Voice/Enemy/", "Voice/NPC/", the announcer). The category is bits 1-5 of the sound's properties word; the list of
  names is the same in every game ("Voice/" 12, "Voice/NPC/Dialog" 21, "Music" 28).
- **The sound's update**: `GAME_HOOK_Sound_Play` runs **every frame for every playing sound**, not once per start. A
  line starts when a sound is first seen with a new playback, or again after a silence; a line not updated for
  0.25 s has ended (it may end without a call to the stop).
- **The layout differs per game**: Trap Team's `tfbSoundInfoImpl` holds its properties at +0x30 (its category getter
  `sub_82221DC0`) and its playback at +0x54; Giants' at +0x54 and +0x78 (`sub_822152D8`). Reading Giants' offsets in
  Trap Team takes the playback pointer for the properties: the categories are noise. `game::SoundProperties` and
  `game::SoundPlayback` carry them.

## Why the skip plays the cutscene fast

The lines are voices the cutscene starts on its own clock, not steps it waits on: stopping the voice alone leaves the
player watching the rest of the line's time. So after a press the hook stops the voice with the game's own stop
(`GAME_HOOK_Sound_Stop`) and sets `_gameLogicScale` to 16 until the next line starts, the hold ends, the game skips
itself, or 8 s pass. Every step of the cutscene still runs, only sooner; nothing is cut, so nothing can soft-lock. The
game gets 0.15 s after a press to start its own skip before the hook acts.

The log says `[dialogue] line skipped at <s> s, the cutscene plays fast` and `[dialogue] the cutscene is back to its
pace at <s> s`.

## Porting it to another Alchemy game

1. Find a cutscene the game does not let the player skip (Y does nothing) with spoken lines.
2. Match Trap Team's `Sound_Play`, `Sound_Stop` and `ScriptValue_Set` and the writer of `_gameLogicScale` with
   `tools/xmatch.py`.
3. Read the sound's category getter (`lwz rX,<offset>(r3)` then `rlwinm rY,rX,31,27,31`) for `SoundProperties`, and the
   update for the playback pointer (`lwz r11,<offset>(r31)` then `lwz r30,36(r11)`, the voice).
4. Check the setter's argument convention (`ScriptArgumentsByReference`) and where the name pointer points
   (`ScriptNameOffset`).
