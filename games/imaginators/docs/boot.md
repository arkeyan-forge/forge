# Imaginators: the first boots

> **Purpose:** how far each boot of Imaginators got, what stopped it, and the fix · **Status:** current (2026-11-04) · **Tags:** [Xbox] [Alchemy] [Imaginators] [V]

Runs: the Europe disc, offscreen, disk audio, scratch settings and saves (`private/imaginators/run.sh <name> <seconds>`), one at a
time. Presses come from `SKY_PRESS_FILE`, written by `private/imaginators/drive.sh` while the game runs (both local, not in git:
they hold the disc's path).

## 1. The shader cache
The first boot drew nothing but the clear colour: every draw was skipped (`no translated vertex/pixel shader`). A 3-minute run with
`SKY_DUMP_SHADERS` (the menus and the movies) gave 1190 containers; `SKY_GAME=imaginators tools/shaders/build_cache.sh <disc>
<dump>` merged them with the disc's (`shaders=igz:/archives/shaders_xenon.pak` in `game.txt`): **1190 of 1190 translate**, none
rejected [V]. Later runs in the menus, the opening and the first level log no missing shader [V].

## 2. Function entries the analyzer missed
Two aborts, `Indirect call to unrecompiled guest code`:
- `ctr=0x82D779B8` (2 min in, the menus): an 8-byte adapter after zero padding (`lwz r4,64(r4)`, then `b` into its neighbour).
  A scan of `.text` for every non-zero run after zero padding that the first mapping did not know found **437** of them (430 of 8
  bytes, the rest 12 to 164), added to `config/Imaginators.toml` [V].
- `ctr=0x8304AD98` (the opening's loading): a 16-byte flag setter after a `blr` (`li r11,1`, two `stb`, `b` back), reached only
  through a `lis`/`addi` address. A scan of every `lis`/`addi` constant into `.text` that is neither a known start, a branch target,
  a switch-table label nor an inline table of addresses (the jump tables built in the code) found **3**: the two setters and a catch
  funclet at `0x836C7FC0`, added too [V].
After both, the game runs 14 minutes from the boot into the first level without a fault [V].

## 3. The way in
The intro movies, the title ("Appuie sur Start"), then **Start** opens the software licence (`softwarelicense.pak`, 50 pages, French
on this disc): it is accepted with **Y**, not A (the screen's own glyphs say so). Then the save (`IMAGINATORS_OPTIONS`,
`IMAGINATORS_0`), the difficulty choice, the opening (`fmv01_cinematic.bk2`, `level_508.pak`, the `c508_*` cutscenes), the portal:
the emulated portal's figure (`SKY_PORTAL_PLACE=new:9`, Eruptor) is read and shown in its vortex, then the part sets screen, then
the first level, on foot with Eruptor [V]. 30 fps (the default limit), 1180 draws a frame taking 4.4 ms of our renderer [V].

## 4. SKY_LEVEL
`engines/alchemy/zone_request.cpp`, SuperChargers' zone request redirect, now shared: Imaginators' zone lookup is `sub_82D7B450`
(a strict shape match of SuperChargers' `sub_82DF1C58`), its zone enter `sub_82D82B50` (0.66; it reads the request global
`0x837E7A64` +44 and calls the lookup at `0x82D82C1C`, as SuperChargers' does), read in the generated code [V].

## 5. Text and voice language (2026-11-05) [Alchemy] [Imaginators] [V]
The disc has French and Dutch (text and voices: `*_fr.pak` / `*_nl.pak` sound packages, no English). With the settings' language
left to the game, it plays in French (our console answers English; the game falls back to the disc's first language) [V].
`[imaginators] language` (`--language`, `SKY_LANGUAGE`) and `[audio] voice_language` (`SKY_VOICE_LANGUAGE`) now choose either,
SuperChargers' scheme (`games/superchargers/language.cpp`) in `games/imaginators/language.cpp`:
- `Language_Get` `sub_8250C898`: `XGetLanguage` (`sub_8363D968`, `ExGetXConfigSetting(3, 9)`) through SuperChargers' jump table
  (read from the image: console 1 -> 6, 3 -> 9, **4 -> 8 French**, 13 -> 23, 15 -> 17, **16 -> 5 Dutch**). Our console has no
  language 16 (`runtime/kernel/xconfig.cpp` gives "nl" as English with the Netherlands for the older games), so for Dutch the hook
  answers 5 itself; for French the console's 4 is enough.
- `Language_SoundPak` `sub_8277EB80` and `Language_SoundPak2` `sub_8277F000`: the two users of `"%s/%s_%s.pak"` (`0x823CA3C0`); while
  one runs, `Language_Get` answers the voice language. Without a voice language the voices follow the text.
- `Movie_Open` `sub_8270DAE0`: BinkOpen's only caller; the language's Bink track id at description `+56`: **5 French, 6 Dutch**
  (runs `imgA1`, `imgB1`); a voice language puts its own there.

| Run | Settings | Result |
|---|---|---|
| `imgA1` | the game's | 7 sound packages `_fr`, movie track 5 |
| `imgB1` | text `nl`, voices `fr` | `[language] text: nl (5), the console's 6`; the pause menu "SPELVOORTGANG", "Selecteren", "Terug"; 8 packages `_fr`, none `_nl`; movie track 6 |
| `imgC1` | text `fr`, voices `nl` | `[language] sound packages: nl (5), the console's 8`; 8 packages `_nl`, none `_fr`; `movie voice track 6 (nl), the text's was 5` |

The install's `movies` step (`movie_shaders=8` in `game.txt`: the game's own six at start, Bink's two with the first logo, 5.4 s after the first in a loaded run) waits for all eight; without a target a slow start ended it at six (SuperChargers' install, same engine and shaders, did: black logos) and the movies are black. A fresh install with the target collected 8 and `movies.sh` passed (brightest frame 187) (2026-11-06, [render_bugs.md](../../../docs/runtime/gpu/render_bugs.md)) [V]. Check on a kit-installed game: `EXE=<game> tools/regress/movies.sh imaginators <disc>`.

## 6. Save tasks (2026-11-05) [V]
Swap Force's stop at the save slots (its task start cleared the done flag after starting the worker thread) **is not in this game**:
SuperChargers' start `sub_829AAB90` is `sub_829CE070` here (shape 1.00), which sets the worker, clears the done flag (`0x837E50D5`),
then starts the thread (`sub_824A80B8`); the poll `sub_829CE020` is SuperChargers' `sub_829AAB40` (1.00). No hook.

## 7. Movies: the skip (2026-11-05) [V]
The two start-up logos (`bumper_atvi`, `bumper_tfb`) are the adapter's `IntroMovies` (`skip_intro_movies`). The opening `fmv01_cinematic`
(5308 frames, 177 s) is skipped the game's way, unlike SuperChargers': a first press shows "OVERSLAAN: (Y)" ("PASSER" in French),
then **Y** skips. Run `imgD1` (a new game, `SKY_LANGUAGE=nl`, one button every 10 s from 11 s into the movie): X at 65.4 s brought the
prompt (shot at 75 s, its Y drawn as the keyboard's V), Y at 87.0 s, and `c508_intro.bk2` opened at 87.3 s, 33 s after `fmv01` (day 1's
runs, A only, played it to its end).
