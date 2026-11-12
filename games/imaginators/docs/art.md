# Imaginators: the launcher's pictures and menu theme on the disc

> **Purpose:** where Imaginators keeps the logo, background, portraits and menu theme the launcher reads from the player's disc · **Status:** current (2026-11-04) · **Tags:** [Xbox] [Alchemy] [Imaginators] [V]

The manifest is `games/imaginators/art.json` (names only; the reader is SuperChargers': IGA v11 big-endian, IGZ v9).

## Where they are [V]
| What | Archive | Entry | Result |
|---|---|---|---|
| Logo | `nxeart` (the dashboard's STFS package) | `nxeslot.jpg`, `compose: feather` | the logo on its night sky, faded at its edges; the launcher shows a downloaded clean logo instead (`logo_download`, Wikimedia Commons, CC BY-SA 4.0) |
| Background | `nxeart` | `nxebg.jpg` | 1920x1080, the dashboard's night sky with light streaks; also the only scene |
| Portraits | `archives/toycollectionmaterials.pak` | `materialInstances/ToyCollection/Collection_<Toy>_<Variant>.igz` | **565** |
| Menu theme | `archives/inter_streamed_bank.pak` | `mu_main_menu_2016_apm_heroes_all.wav,0,48000,kDefaultFormat,40,1,0,1` | 43.9 s, 48 kHz stereo, mean -15 dB, peak -0.2 dB |

Proof: `ArkeyanArtTest`'s disc extraction run once with a temporary `IMAGINATORS_ISO` call (logo 1, background 1, title icon 1, 565
portraits, 2.0 s); `Arkeyan-Builder --iso <disc> --menu-music <file.wav> --kit <scratch kit>` (`@done`). The disc test is not
permanent: `TestDisc`'s later checks (upgrades, drawn icons) are the older games'.

## Open
- No clean transparent 2D logo on the disc (scan of 4995 archives, 2026-11-05): the title screen's logo is a 3D model
  (`actors/UI_3D/imaginatorslogo.igz`), `legal.pak`'s `TitleLogo_2016_001` is the logo in a burst of light on black
  (`compose: light_on_black` reads it), `prototitle.pak`'s `TitleLogo_2016` is the prototype "Skylanders 2016" one.
- `imaginators_TM` and about 13,600 other `,111.igz` entries store their chunks as raw deflate (mode word top nibble 1:
  `[u16 n][n bytes]`, 0x8000 bytes out each); `IgaArchive::Read` decodes LZMA only and reads them as stored [V: the scan's
  patched reader]. A deflate branch would open them (none is needed for the launcher today).

## Button pictures (glyphs, 2026-11-05) [Alchemy] [Imaginators] [V]
SuperChargers' way ([its art.md](../../superchargers/docs/art.md), "Button pictures"): the game composes its strings on the CPU, so
the buttons inside every text come from two pictures named in the fonts, which `glyphs::MemoryImage` (adapter `GlyphImages`) redraws
in guest memory for the device in use.

| Picture | Where | Cells |
|---|---|---|
| Info font's buttons, 256x168, 32-bit tiled ARGB | `fonts/ProximaNova-Info.igz` (`languagestartup.pak`), `igImage2` 1 | **SuperChargers' and Swap Force's own** (the same XXH3 of the first 4096 bytes, `7360B650D459A892`): SuperChargers' cells, plus Swap Force's added Start cell (202,7-244,49, below) [V] |
| Body font's buttons, **128x250**, 32-bit tiled ARGB | `fonts/ProximaNova-Body.igz` (`essentialui.pak`), `igImage2` 1, `3B9E95737FB43E23` | SuperChargers' 128x209 layout plus one row: A 1,1-38,39 · B 44,1-80,39 · Y 85,1-123,39 · X 2,42-39,80 · LB 43,49-106,75 · LT 4,83-40,121 · RT 49,83-86,121 · RB 3,131-66,157 · LS 73,125-110,161 · RS 5,166-42,202 · **Back** 47,172-83,196 and 89,172-125,196; the D-pad at 2,205-43,243 has no glyph and stays |

The two "Back" pills are the same picture (0 pixels differ). The font's glyph records (48 bytes after its letters: `8, 1, code << 16`,
the advance, the size, then u0, v0, u1, v1 over 128x250) give each cell its character: A `0x298`, B `0x5E`, Y `0x60`, X `0x7E`, LB
`0x5B`, RB `0x5D`, LT `0x3C`, RT `0x3E`, LS `0x7B`, RS `0x7D`, the D-pad `0xA4`, the pills `0x86` and `0x2020`: the dagger "†" in
Windows-1252 and in Unicode, one button under two encodings, so both are Back (the 360's Back shows that left arrow).
Proof (run `imgB1`, `SKY_LEVEL=hub_2016`, 2560x1080, `SKY_GLYPHS_SCRIPT=ps5:120,switch:132,key:144,xbox360:156`, Start at the hub):
`body font buttons 128x250 found in guest memory`, `info font buttons 256x168 found ...`, each `drawn as` playstation, nintendo,
keyboard, xbox360; the pause menu's hints in the shots: cross and circle; B and A (Nintendo, by position); Space and X keycaps; the
360's A and B. The movie's skip prompt shows the Y as the keyboard's V (run `imgD1`). The Back pills were not met in a run [?].
How (30 min): the two archives extracted from the disc (`tools/shaders/xdvdfs.py`) into `private/imaginators/disc/`, SuperChargers'
scratch tools (picture list and hash, untile and boxes) run on them, the body picture looked at; the records read in the entry.

**"Appuie sur Start" shows the Start button's picture** (2026-11-06) [V]. The title's prompt is
`CGlobalPlatformStrings::_titleScreenPressStartString` of `strings/xenonPlatformStrings_<fr|nl>.lng` (`permanent_xenon.pak`):
"Appuie sur Start" / "Druk op Start"; the same files hold "Appuie sur START", "DRUK OP START", "Druk op START" (the HUD's
`_hudPressStartString`, the online invitation's `ui/OnlineInvitation_2015_<lang>.lng`). `ui/StartScreen_2016_<lang>.lng`'s
"APPUIE SUR START POUR COMMENCER" is not what the title draws. The words mix case, so Swap Force's rule (upper case, last word
START) became a list of press-Start phrases per disc language (`engines/alchemy/prompt_text.h`; script names that end with
"Start", such as "On Start" or "Fog Start", stay as they are; `ArkeyanPromptTextTest`), and Swap Force's hook moved to
`engines/alchemy/prompt_text.cpp`, shared with SuperChargers. The two functions (`addresses.h`): the intern `sub_82485F28`,
found through its caller, the IGZ loader's TSTR helper `sub_824E3180` (Swap Force's `sub_82433D90`, `tools/xmatch.py` shape
1.00; the intern itself was rewritten, shape 0.19, with the same arguments and result), and the font's character lookup
`sub_826AE8E0` (Swap Force's `sub_82553E48`, shape 0.82; the same lists at +0x40 and +0x44). The prompt is drawn in the info
font (`[glyphs] the info font's Start character made`), whose image has Swap Force's empty corner: the adapter adds the same
Start cell. Proof (runs `img-fr1`, `img-nl1`, `SKY_LANGUAGE=fr|nl`, `SKY_GLYPHS_SCRIPT=ps5:100,key:125,xbox360:150`; frames in
`private/start-glyph/shots/`): "Appuie sur" / "Druk op" followed by the ENTER keycap, the PlayStation OPTIONS button and the
360's ▶; the log says `text "Appuie sur Start": START shown as the Start button's picture`.

## World prompts (2026-11-05) [Alchemy] [Imaginators] [V]
`permanent_xenon.pak` holds SuperChargers' 3D button prompts (`models/UI/ButtonPrompt/XENON`: A, B, X, Y, LT, **RT**, Mash_*, RS_*; RT
instead of its RB) and the meshes are the same: a face and the letters of the button on it ([SuperChargers' page](../../superchargers/docs/art.md#world-prompts-2026-11-05)
has the format and the method; the signature of Imaginators' B, X, Mash and RS meshes differs a little, A, Y, LT and RB are byte for byte
SuperChargers'). **RB is replaced** like SuperChargers' (adapter `GlyphPrompts`): the same face (816 indices, `21E1098D0C5EEC4F`) and letters
(2706, `7F26CABF048F8787`), the same orientation and the same stretch of u (2.55) [V]. The draw order differs from SuperChargers': the colour pass draws the
letters before the face (the depth pass, the face first), so both orders are paired.

**Where one shows.** The hub (`SKY_LEVEL=hub_2016` on a continue; the plaza's fountain): the RB pill under the hologram bust of Flynn, the interaction
prompt, once Eruptor has walked to the fountain (`lup:5`, the bust's lines skipped with A, `lup:6`). The first level (`level_508`) showed none in a short walk.

| Style (`--glyphs`) | The RB prompt in the hub (`private/prompts3d/proof/`) |
|---|---|
| `playstation` | a dark pill, "R1" |
| `keyboard` | a white keycap pill (the RB key) |
| `xbox360` | the game's white pill with "RB", unchanged |

Run: `private/runs/p3d-imgF-{playstation,keyboard,xbox360}`, a continue on the lobby test's save of `tools/regress/imaginators_hub.sh` (Start to the licence, Y, A).
**Left:** RT (333 and 1362
indices; B's meshes differ from SuperChargers': 441 and 1059), A, B, X, Y, LT, the Mash and B_Hold prompts were not seen in a run (the hub's plaza as far as the
fountain's stairs and the first level's first path), so no row is listed ([SuperChargers' page](../../superchargers/docs/art.md#world-prompts-2026-11-05)
says how a row is made; A and Y, B and the Mash prompts share a face and are told by their letters).
