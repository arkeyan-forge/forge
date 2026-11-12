# SuperChargers' pictures and menu theme from the disc: logo, background, portraits, music

> **Purpose:** where SuperChargers keeps its logo, background, toy portraits and menu theme, and what the launcher reads from the player's disc · **Status:** current (2026-11-04) · **Tags:** [Alchemy] [SuperChargers] [V] [?]

The provider and the rule are the other games' ([game_art.md](../../../docs/runtime/host/game_art.md), [game_music.md](../../../docs/runtime/host/game_music.md));
this page is what is SuperChargers'. Read from the player's own disc and cached in `<data>/SuperChargers/`; nothing is shipped.
Manifest: [art.json](../art.json). The archives are IGA v11 big-endian, their entries IGZ v9 ([shaders.md](shaders.md)); the entry names are
build paths (`Temporary/BuildServer/xenon/Output/textures/<shader>,textures@!ui!<folder>!<name>\`tga,<flags>.igz`, `/` separators).

## Where they are [V]
| What | Archive | Entry | Result |
|---|---|---|---|
| Logo | `archives/legal.pak` | `textures@!ui!do_not_compress!Skylanders!ToyFair_Title_NOTFINAL` | 688x316, A8R8G8B8: the full logo (exhausts, the element medallions, ™); `LevelLoad.pak`'s `Logo_Loading` (672x336) is a plainer copy, `ProtoTitle.pak`'s `levelTitle` (860x484) an earlier logo on clouds |
| Background | `archives/levelload.pak` | `textures@!ui!Loading!BG_Full` | 1024x724: the blueprint paper of the loading screens. The main menu is a 3D scene (`MainMenuBackground.pak`: the start screen's logo models and a skybox); no 2D menu painting was found on the disc [?] |
| Portraits | `archives/toycollectionmaterials.pak` | `materialInstances/ToyCollection/Collection_<Toy>_<Variant>.igz`, each naming its texture (`textures@!ui!Collections!ToyInventoryIcons!<year>!<Toy>`) | **469**, 88x88, round badges: every toy of the five games (year folders 2011 to 2015: characters, traps, vehicles, the race packs), e.g. `collection_souljam_normal` (Fiesta), `collection_vehicleundeadcar_normal` (Crypt Crusher) |
| Menu theme | `archives/inter_streamed_bank.pak` | `INTER_Main_Theme.wav,0,48000,kDefaultFormat,40,1,0,1` | FMOD FSB5/XMA, 48 kHz stereo, 80.6 s; music throughout (RMS 2500 to 8400 per 10 s, 0.011 % of samples clipped) |

- The theme: the streamed bank's events (`permanent.pak`, `sounds/banks/inter_streamed_bank.igz`) have `INTER_Menu_Screen` without a sample of
  its own, as in Swap Force, next to `INTER_Main_Theme` (and `INTER_Main_Theme_Fin2`); the only theme sample is `INTER_Main_Theme.wav`. The link from the
  menu event to that sub-sound was not followed object by object as for Swap Force [?]; the other candidate, `INTER_ActionPack_Academy_Menu_01`,
  is the race packs' menu.
- Proof: `ArkeyanArtTest`'s disc extraction run once on this disc with a temporary call (logo 1, background 1, title icon 1, 469 portraits, 0.85 s,
  0.00 s cached); `Arkeyan-Builder --iso <disc> --menu-music <file.wav> --kit <scratch kit>` (`@done`). Shots in `private/superchargers/art/`.

## For the launcher (not done here)
- The catalogue's SuperChargers toys need portrait keys (the badge names above, `collection_<internal name>_<variant>`) and SuperChargers in
  `ArkeyanArtTest`'s disc list (`TestDisc`, with the drawn-icon count it then has); the internal names are the game's (Fiesta is `SoulJam`, Crypt
  Crusher `VehicleUndeadCar`).
- `tools/sync_skylauncher.sh` carries the reader (IGA v11 big-endian, IGZ v9, the separators) and the manifest.

## Button pictures (glyphs, 2026-11-04) [Alchemy] [SuperChargers] [V]
Swap Force's way ([input.md](../../../docs/runtime/xbox360/input.md), "Glyphs"): the game composes its strings on the CPU, so the
buttons inside every text (the pause menu's "Select", "Back", "Quests" [V]; the tunnel ride's "Hold RT to Accelerate!", not run with a
style yet [?]) come from two pictures named in the fonts, which
`glyphs::MemoryImage` (adapter `GlyphImages`) redraws in guest memory for the device in use.

| Picture | Where | Status |
|---|---|---|
| Info font's buttons, 256x168, 32-bit tiled ARGB | `fonts/ProximaNova-Info.igz` (`languagestartup.pak`), `igImage2` 1 | **Swap Force's own** (the same XXH3 of the first 4096 bytes, `7360B650D459A892`; components measured again: the same boxes): Swap Force's cells, with its added Start cell (202,7-244,49: "Press Start", below) [V] |
| Body font's buttons, **128x209**, 32-bit tiled ARGB | `fonts/ProximaNova-Body.igz` (`essentialui.pak`), `igImage2` 1, `402F2F6915FA96F8` | new layout, two buttons a row: A 1,2-38,39 · B 44,1-80,39 · Y 85,1-123,39 · X 2,42-39,80 · LB 44,49-105,76 · LT 4,83-40,121 · RT 49,83-86,121 · RB 3,131-65,158 · LS 73,125-110,161 · RS 5,166-42,202; a D-pad at 47,164-88,202 has no glyph and stays [V] |
| `Turbo_A`, `_X`, `_Y` (88x88 DXT5) | `permanent_xenon.pak`, `textures@!ui!ButtonPrompt!XENON!turbo!` | not listed: the renderer hashes a tiled level padded to 32x32 blocks (16 KB) and the file stores 11 KB, so the hash cannot be taken from the disc; no run reached a turbo prompt [?] |
| World prompts `models/UI/ButtonPrompt/XENON/{A,B,X,Y,LT,RB,Mash_*,RS_*,...}` | `permanent_xenon.pak` | two meshes, a face and the letters, coloured by a flat 4x4 map and a tint: **RB replaced** (below, 2026-11-05) [V]; the others are listed, not replaced |
| Controller pictures (`ui!Controllers!Xbox_Controller`...) | | not replaced |

How (about 1 h): the two archives extracted from the disc (`tools/shaders/xdvdfs.py`) into `private/superchargers/disc/`, their pictures
listed and their stored pixels written by a scratch tool on `runtime/host/art/alchemy_archive.cpp` (`private/superchargers/scratch/fonts/`),
untiled, the opaque components' boxes printed and a picture of the body image looked at to name them.
Proof (run `scU2`, a continue into the Academy, 2560x1080, `SKY_GLYPHS_SCRIPT=ps5:122,switch:130,key:138,xbox360:146`, Start at 118 s):
`body font buttons 128x209 found in guest memory at AC6D6000`, `info font buttons 256x168 found ... at AC57A000`, each `drawn as`
keyboard, playstation, nintendo, keyboard, xbox360; the pause menu's hints in the shots: cross, circle, triangle; B, A, X (Nintendo, by
position); Space, X, V keycaps; the 360's A, B, Y.

**"Press Start" shows the Start button's picture** (2026-11-06) [V]. Swap Force's way, with its hook now shared
(`engines/alchemy/prompt_text.cpp`; Imaginators' [art.md](../../imaginators/docs/art.md), "Button pictures", has the details):
the title's `CGlobalPlatformStrings::_titleScreenPressStartString` (`strings/xenonPlatformStrings_<en|de>.lng`,
`permanent_xenon.pak`) is "Press Start" / "Drücke Start", mixed case; the same files and the online invitation's
`ui/OnlineInvitation_2015_<lang>.lng` hold "PRESS START", "Press START", "DRÜCKE START", "Drücke START" (all matched by the
phrase list of `prompt_text.h`). `ui/StartScreen_2015_<lang>.lng` ("Press Start To Begin", "Zum Beginnen Start drücken") is not
what the title draws, and is left. The intern is `sub_82455A88` (called by the TSTR helper `sub_824AD660`, Swap Force's
`sub_82433D90` at shape 1.00; Imaginators' intern instruction for instruction), the font's character lookup `sub_826A4548`
(Swap Force's `sub_82553E48`, shape 0.82). The prompt is drawn in the info font, whose image has the empty corner of Swap
Force's: the adapter adds the same Start cell. Proof (runs `sc-en1`, `sc-de1`, `SKY_LANGUAGE=en|de`,
`SKY_GLYPHS_SCRIPT=ps5:110,key:135,xbox360:160`; frames in `private/start-glyph/shots/`): "Press" / "Drücke" followed by the
ENTER keycap, the PlayStation OPTIONS button and the 360's ▶.

## World prompts (2026-11-05) [Alchemy] [SuperChargers] [V]
**What they are.** `permanent_xenon.pak` holds 19 prompt models (`models/UI/ButtonPrompt/XENON`: A, B, X, Y, LT, RB, B_Hold, Mash_A/B/X/Y,
LS_Waggle, RS_*) with their vfx and five flat 4x4 maps (`ColorMap...Yellow_Y`, `Green_A`, `Red_B`, `Blue_X`, `Blue_Letters`). Each model is
two indexed meshes (A: 87 and 633 indices; RB: 816 and 2706): a flat face and the letters of the button on it. Positions are SHORT4N,
texture coordinates FLOAT16_2, stride 20 or 24. The face's pixel shader samples a flat map (the RB pill samples the white default map; its
colour is a tint), so no texture holds the button. The index arrays of the disc (the `igIndexBuffer` of the model, found in the
decompressed entry by its shape, a scratch tool on `runtime/host/art/alchemy_archive.cpp`) hash, as the renderer hashes them, to
the draw's: RB's face `21E1098D0C5EEC4F` (816) and letters `7F26CABF048F8787` (2706) are the ones the game draws [V].

**Where one shows.** The Academy (a continue from the lobby test's save, `tools/regress/superchargers_lobby.sh`): Fiesta walking up
from the start platform (`lup` held 9 s at 165 s), an RB pill floats by the platform's NPC (white, "RB" in cyan: the interaction prompt). The tutorial level (`intro1`) showed none in a walk to its first fight; A, B, X, Y, the triggers and the mash prompts were not met in a run [?].

**How it is replaced** ([input.md](../../../docs/runtime/xbox360/input.md), "World prompts"; adapter `GlyphPrompts`, `GlyphTextures`): the
letters' draw is skipped, the face gets the style's glyph on a dark plate as its texture (320x128, the pill's shape), with texture
coordinates spread over its bounding box (`uAxis` 0, `vAxis` 1, v flipped). The game's shader stretches u about its middle by 2.55
(measured with a gradient picture, the pill spans u 0 to 1 at 0.31 to 0.70 of its length), so `uScale` 2.55 writes it divided.

| Style (`--glyphs`) | The RB prompt in the Academy (`private/prompts3d/proof/`) |
|---|---|
| `playstation` | a dark pill, "R1" |
| `keyboard` | a white keycap pill, "E" (the default RB key) |
| `xbox360` | the game's white pill with "RB" in cyan, unchanged |

Run: `private/runs/p3d-scF-{playstation,keyboard,xbox360}`, the lobby test's save, `SKY_PRESS` A to the menus, `lup:165:9`, `SKY_GPU_SNAPSHOT_SECONDS=190`.

**Left.** A and Y share one face (87 indices), B, B_Hold and Mash_A/B/Y another (609): the renderer now tells them by their letters,
which the game draws right after the face (it holds the face for one draw, [input.md](../../../docs/runtime/xbox360/input.md), "World prompts"; unit-tested,
and run with RB). What is missing is a prompt to measure: A, B, X, Y, LT, the Mash and B_Hold prompts were **not met in a run** (the Academy's
start platform, the walk to the tutorial's first fight, the tunnel ride), so their `uAxis`/`vAxis`/flips and `uScale` (2.55 for the pill: the
shader's stretch of u, which a disc may not have) are not measured and no row is listed; the meshes' signatures come from the disc
(`permanent_xenon.pak`, the decompressed entries scanned for their index arrays, A: 87 and 633 indices, X: 609 and 624, LT: 114 and 2148,
the same method as RB's) and a row is one line per prompt once one is seen. The effects (`vfx/UI/ButtonPrompt`: rings, sparks) and `Turbo_A/X/Y` (the
vehicle's HUD) stay the 360's.
