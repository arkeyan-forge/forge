# Languages in the Lux games

> **Purpose:** how SSA picks the text, voice and movie language, and how we override it · **Status:** current (2026-10-26) · **Tags:** [Lux] [SSA]

Code: [`language.cpp`](../../../games/ssa/language.cpp), [`kernel/xconfig.cpp`](../../../runtime/kernel/xconfig.cpp), [`host/settings.cpp`](../../../runtime/host/settings.cpp). Class **[Lux]** structure, **[SSA]** addresses.

## How the title picks a language

`sub_82808190` reads **`sku.lxb`** (`"languages"` bitmask `0x7CF4` = dut eng fre ger ita spa swe fin nor dan; `"default"` `0x10` = eng) and picks **one language for everything**:
1. **Country first** (via `sub_822DECD0`): XC_COUNTRY 74 -> dut, 90 -> swe, 75 -> nor, 25 -> dan, 32 -> fin.
2. Otherwise `XGetLanguage`: de -> ger, fr -> fre, es -> spa, it -> ita, anything else -> eng.

The index is stored at **`0x82AA7048`** (`+52` holds the same value; `+56/+60` the default); the **code table at `0x82AA5EBC`** (15 `char*`, the disc's folder names): **0 chs, 1 cht, 2 dut, 3 euk, 4 eng, 5 fre, 6 ger, 7 ita, 8 jap, 9 kor, 10 spa, 11 swe, 12 fin, 13 nor, 14 dan**.
**Everything reads `+48`**: text (`text/%s/*.lxb`, `UI\%s`), localized movies (`movies/%s/legal.bik`), and:
- **audio**: `sub_827FD4D8` -> **`sub_82864B90(code)`** sets Wwise's language folder (`\audio\<code>\voice.bnk`, `\audio\<code>\levels\*.bnk`) and, through `sub_82156A00`, the current language of every file package (`streaming.pck` holds 2055 voice streams per language keyed by the same codes);
- **cutscene voices**: **`sub_827F4400`** picks the Bink voice track (`sl_sc01.bik` has 15 tracks: 0 = English, 6 fre, 7 dan, 8 dut, 9 fin, 10 ger, 11 ita, 12 nor, 13 spa, 14 swe, 1-5 music/effects). Track numbers are **per-movie authoring**: re-check in other games.
The disc has **10** languages (dut eng fre ger ita spa swe fin nor dan); the code table has 15 (the other five have no text on disc: English shown).

## Our hooks and settings

- `language.cpp`: `SetVoiceLanguage(code)`; hooks **`sub_82864B90`** (replaces r3 by the voice language's code pointer from the table, logs `[language] text X, voices Y`) and **`sub_827F4400`** (after the original, if `(r4 & 0xFF) == 1`, i.e. the movie has localized voices, writes the voice language's Bink track into `*r3`).
  Table `VoiceLanguages`: en(4, track 0), fr(5, 6), de(6, 10), es(10, 13), it(7, 11), nl(2, 8), sv(11, 14), no(13, 12), da(14, 7), fi(12, 9).
- `kernel/xconfig.cpp`: `SetTextLanguage(code)` picks the **console language and country the title reads** (nl/sv/no/da/fi = English language with that country). Default **English** since 114fe5e ("French text and English voices" was the first default; reverted until translations are complete).
- Settings: `[ssa] language=` / `voice_language=` (or `SKY_LANGUAGE` / `SKY_VOICE_LANGUAGE`, `--language` / `--voice-language`), codes `en fr de es it nl sv no da fi`. UWP: kernel defaults.
- **Open [?]**: with French text some strings appeared in "random" languages (D29). No launcher UI at the time of the languages work (now in Arkeyan Launcher).
- Verification: default run log `[language] text eng, voices eng`; the game opens `\text\eng\global.lxb`, `\movies\eng\legal.bik`, `\ui\eng\uituning.lxb`, `\audio\eng\voice.bnk`.
- Next Lux game: find the `sku.lxb` reader and the language-code table; Nordic languages via country are likely unchanged.

## What the disc holds, per game (Arkeyan Launcher's Language tab) [Lux] [SSA] [Giants] [TrapTeam] [V]

`art::LanguagesOnDisc(game, files, archive)` ([`runtime/host/art/disc_languages.h`](../../../runtime/host/art/disc_languages.h)) gives the text and voice languages the disc holds; Arkeyan Launcher stores them when it checks the disc and lists only these. It reads **names** only: the disc's file names and, for Trap Team, the entry names of two or three archives (`archive` is the reader that gives them; without it Trap Team's lists are empty). Empty lists mean unknown (the launcher shows a short list: Swap Force English and French, Trap Team English, the others every language they can set). The codes it can name, in the order the lists keep: `en fr de es es-419 it nl pt sv no da fi pl ru cs hu tr el ja ko zh`.
- **SSA**: text = a folder `text/<eng|fre|ger|spa|ita|dut|swe|nor|dan|fin>/` with files; voices = `audio/<folder>/voice.bnk` (decided apart, though the disc has the same ten of both). The code table also names `chs cht euk jap kor` but the title picks only the ten above (`sub_82808190`), so a folder of another name is not a language it can show and is ignored.
- **Swap Force**: `archives/<name>_<code>.pak`, text and voices alike: `en fr` on the USA/Australia disc, `es it` on the European one. Any code of the list above counts (the engine's `IG_CORE_LANGUAGE` has the same languages as Trap Team's); a suffix that is not one (`_staging.pak`) is ignored. [?] no disc with `pl`, `ru` or `ja` was seen, and the runtime sets the text through the console language only (`--language`), so Dutch, Swedish, Norwegian, Danish and Finnish would need the hook Trap Team has.
- **Giants**: no file name marks a language (the packs are inside each `.bld`), and its only known release holds all twelve: always the full list of twelve.
- **Trap Team**: every file name is the same on the English-German and the French-Dutch discs (2133 files, same names, only the sizes differ), so the names inside the archives say it [V] (both discs, 2026-10-31): the **text** is the `<LANGUAGE>.pak` entries of `character/Init_Setup.bld` (also in every character `.bld`, 653 of each; `permanent/global.bld` when Init_Setup has none): `ENGLISH` and `GERMAN` on one, `FRENCH` and `DUTCH` on the other. The **voices** are the language folders of the sound entries of the character archives (`.../whirlwind/german/vo_whirlwind_victory_01.wav/0x....wav.hz.wav.enc`: `german`, `french`, `dutch`): read from the first six `character/*.arc` in path order, the first holding a language folder. English has no folder, its lines are the unmarked ones: it is held when the text has it and the archive has voice lines (the French-Dutch disc keeps 30 unmarked lines in `000_airdragon.arc` that need no translation, and no English). The names are the enum's (`IG_CORE_LANGUAGE_<NAME>`, lower case, `SPANISH_MEXICAN` for `es-419`); an unknown name is ignored. [?] Spanish, Italian and the other releases' languages are named by the same rule and were not seen.
- Test: `TestDiscLanguages` in `host/art/art_test.cpp` (hand-typed file lists and entry lists; the launcher's `ArkeyanLauncherTest` reads the two Trap Team discs when `SKY_LAUNCHER_TEST_TRAPTEAM_ISO` and `SKY_LAUNCHER_TEST_TRAPTEAM_LANGUAGES` are set). Copied to Arkeyan Launcher by `tools/sync_skylauncher.sh`.
