# Giants boot: what broke on the way to the title and to a level

> **Purpose:** what stood between Giants' disc and a playable level, the causes and the fixes · **Status:** current (2026-10-26; written 2026-10-25 as the boot went) · **Tags:** [Xbox] [XDK] [Alchemy] [Giants]

What the boot work learned about *Skylanders: Giants* (`default.xex` sha256 `12d1e29e...fc09e`, title
`415608DA`). Tags: **[Giants]** this game only; **[Alchemy]** the engine Giants and later games share; **[XDK]** the
libraries of XDK 21076; **[Xbox]** every game. Addresses are Giants'.

## What broke on the way to the title

### The engine is Alchemy, not Lux [Alchemy] [Giants]
- No `.lsa`/`.str`/`.lxb`: the disc holds `.arc` (295), `.bld` (596), `.mov` (Bink 2, 61) and IGA v8 archives / IGZ
  v6 scenes (tsapZ/GiantsRecomp, MIT). So **none of SSA's engine hooks apply** (NGL scene/list guards, Lux heap free,
  language tables, zlib `inflate`, LightTile classify, portal log, `Movie_Update`). That is why sigmatch found
  171/293 but only the CRT/XDK ones with confidence: the engine anchors are not "missing", they do not exist.
- No C++ exception handling in the binary (no FuncInfo magic `0x1993052x`): `invalid_instructions` only needs the
  padding and the `__C_specific_handler` thunk `0x8261B870` (48 SEH functions).

### XenonRecomp [Xbox]
- Instructions SSA never used and Giants does: `vsrah vslh vsrh vspltish vandc vslo/vsro(128) vsubsbs vminsh vmaxsh
  vcmpgtsb vcmpgtsh lhbrx ldux`, and `vpkd3d128` FLOAT16_4 with pack 3 (placed like pack 2 for shift 0-2, Xenia's
  permute table). All in `patches/xenonrecomp.patch`.
- The disassembler's `mulchw.` (a PPC405/440 instruction) shadowed `vsldoi128` (35 sites in the Bink 2 decoder):
  removed from the table. Any "mulchw" on Xenon is a decode bug.
- Four Bink 2 jump tables index with a mask and no bounds check (`0x821659A4`, `0x82166BE8`, `0x821693D0`,
  `0x8216AA04`): `find_switch_tables.py` cannot size them; taken from tsapZ/GiantsRecomp's config (MIT).
- `find_switch_tables.py` pass 2 (with pass 1's mapping) alone drops 32 functions pass 1 found: merge both lists.
  Result: 9711 explicit functions, 253 tables, 23631 functions, no unknown instruction.

### Imports [Xbox]
33 imports SSA lacks (Io*/Stfs*/Ex pool/APC/XeCryptSha/XamLoader launch data/NetDll listen...): implemented in
`runtime/kernel/imports_giants.cpp` and the matching files. Giants imports 220 in all; `runtime/kernel/import_list.inc`
has 251 (SSA's 218 and Giants' 33: [kernel_exports.md](../../../docs/runtime/xbox360/kernel_exports.md)).

### Direct3D [XDK]
- XDK libraries 2.0.**21076** (SSA 2.0.20500). The D3D code is nearly the same, with the device grown by 0x80.
- Found with `tools/xmatch.py` (reusable for every later game; reference = a tree with SSA's files):
  `callgraph` from the `VdSwap`/`VdInitializeRingBuffer` callers (callee lists aligned; `--find` ranks by matched
  callees and size), `shape` (instruction-shape similarity), `offsets` (moved structure offsets):

| Function | SSA | Giants | How |
|---|---|---|---|
| D3DDevice_Swap | 822A95C0 | 823DA640 | only caller of VdSwap |
| D3DDevice_InitializeRingBuffer | 822A4210 | 823D2298 | only caller of VdInitializeRingBuffer |
| Direct3D_CreateDevice | 822A09F0 | 823DB340 | call graph |
| D3DDevice_KickOff | 822A3BB0 | 823D1C10 | call graph |
| D3DDevice_BlockOnFence | 822A3CB8 | 823D1D10 | call graph |
| D3DDevice_InsertCallback | 822A3DB0 | 823D1E08 | call graph |
| D3DDevice_NextRingSegment | 822A2D78 | 823D0DB8 | mapped callees |
| D3DDevice_DrawVertices | 8229AD58 | 823E03E0 | mapped callees, same size 0x418 |
| D3DDevice_DrawIndexedVertices | 8229B170 | 823E07F8 | same size 0x478 |
| D3DDevice_BeginVertices | 8229A850 | not found | 823DFF48 (shape 0.77) is an internal fixed-stride draw: wrong |
| D3DDevice_Resolve | 822A1560 | 823DED00 | mapped callees 8/8 |
| ClearF worker | 822ADDE0 | 823DDF38 | same size 0x680 |
| D3DVertexShader_Init | 82290410 | 823DC7E8 | shape similarity 1.00 |
| D3DPixelShader_Init | 82290128 | 823DC5F0 | shape similarity 1.00 |
| Begin/End/RunCommandBuffer | 822A6BC8.. | none | Alchemy does not record command buffers (no match) |

- **Shaders:** Alchemy never calls `D3DDevice_Create*Shader`: it builds the shader object itself (IGZ data) and calls
  `D3DVertexShader_Init`/`D3DPixelShader_Init` (`sub_822FCF40`, r3 object, r4 microcode in physical memory): our
  shader hook is on the two Init functions. The shaders are inside the compressed archives; Arkeyan-Builder now reads them
  from `/permanent/global.bld` ([shaders.md](shaders.md); the early "378 raw containers" were chance matches).
- **Device layout:** aligning matched functions instruction by instruction, every device offset from ~0x2F58 up is
  **+0x80** in Giants (0x2F80→0x3000, 0x318C→0x320C, 0x3190→0x3210, 0x31A0→0x3220, 0x31E8→0x3268, 0x3260→0x32E0,
  0x327C→0x32FC, 0x3290→0x3310, 0x35E0→0x3660, 0x3630→0x36B0, 0x3B08→0x3B88, 0x5594→0x5614); 0x480-0x27A0 (constants),
  the register shadows 0x2880-0x2A6F and the fence words 0x2B10/0x2B14/0x2B1C are unchanged. The vertex declaration is the exception: 0x2F58 -> **0x2FD0** (+0x78), found at run time by
  scanning the device for a pointer to a type-5 (declaration) resource.
- **Device pointer:** no game global like SSA's `0x82BB4F94`: Alchemy keeps it in an object (`sub_823001C8` stores
  CreateDevice's result at `obj+0xC`). Take it from the hooks' r3 instead.

### Prior art used (facts only)
tsapZ/GiantsRecomp (MIT): same `default.xex` hash; Alchemy formats; BinkOpen `0x82153DB0`, BinkClose `0x82155B08`,
region hook `0x8217F350`, swap queue `sub_823D9758`, scene cap `sub_821939E0`.

### XMA [XDK]
Same two engines as SSA (shape similarity 1.00): engine 1 init `82436480` (reads XMA register `0x7FEA1800`), voice
clear `824364E0`, kick `82436E78`, lock-all `82436D08`, critical section `0x82627B90`; engine 2 voice clear `823CD048`,
lock `823CD1F0`, kick `823CD4A0`. Engine 2 has no separate init: its read of `0x7FEA1800` is inline at `0x823561E8`.
Hooked since the second boot pass (`games/giants/addresses.h`: `Xma1_*`, `Xma2_*`), which fixed the sound and a
deadlock (below).

### First boot to the title screen (2026-10-25) [Giants] (unless marked)

Run: `private/giants/scripts/run.sh` style (the test isolation variables (docs/getting-started/testing.md), `SKY_SKIP_MOVIES=1`). Result: the title
screen "Press A to begin" with its 3D scene and logo at 60 fps (78-96 draws, 12 resolves per frame, frame check: all
three thirds of the 1280x720 image filled, ~7800 colours), then A gives the autosave notice and a save dialog. In order,
what stood in the way:

1. **Indirect call to 0x82397068** (crash): 68 jump tables sit right after their `bctr` with the bounds check far
   before it (Alchemy render code, the Bink 2 ones too). `find_switch_tables.py` now finds them (`find_inline_tables`).
   [Xbox] (any game)
2. **`NtQueryDirectoryFile`** (crash after `[stub]`): Alchemy lists `\permanent\` at start. Implemented over the disc
   and host folders (`runtime/kernel/files.cpp`, `DiscFileSystem::List`). [Xbox]
3. **Shaders:** the hooks on `D3DVertexShader_Init`/`D3DPixelShader_Init` dump 4338 containers (`SKY_DUMP_SHADERS`);
   Alchemy strips their constant tables, so XenosRecomp now makes one up (float4 array `g_C`, samplers and booleans
   0-15): 4337 of 4338 translate (1 uses a 1D texture Metal has no heap for). SSA: 1118. The cache was then built
   from that dump (`SKY_GAME=giants tools/shaders/build_cache.sh <disc> <dump dir>`, ~15 min on an M1 Pro); since
   2026-10-25 it comes from the disc ([shaders.md](shaders.md)). [Alchemy]
4. **0 draws**: the draws ran but `PrepareVertices` found no vertex declaration (offset 0x2FD0, above). [Giants] [XDK]
5. **3 indices per draw**: `DrawIndexedVertices` lost its base-vertex argument in XDK 21076:
   `(device, primitive, start, count)` (`game::DrawIndexedBaseVertex`). [XDK] (21076)
6. **Right third black**: the 1280x720 MSAA frontend pass uses predicated tiling (two 864-pixel tiles: count at
   device+0x3344, rects at +0x3348); we see each draw once, so host targets of such a pass now cover all the tiles
   (`game::ExpandTiledTargets`, Giants only; SSA renders as before). [Xbox] (enabled per game)
7. Movies: Bink 2 logos (`atvinewlogo_640.mov`, `tfb_logo_640x456.mov`) were not shown then (their 50 run-time shaders
   were missing); `SKY_SKIP_MOVIES=1` makes them one frame long through a `BinkOpen` hook (`sub_821550A0`). [Giants]
   **Since 2026-10-26 they are drawn** (the shaders are recovered at install): [bink.md](../../../docs/engines/lux/bink.md), "Bink 2 in Giants".

## What broke on the way to a level

### From the title to Chapter 1 (2026-10-25, second pass)

Title -> autosave notice -> Choose Play -> slot -> new game -> opening movie (skipped) -> the in-engine intro with
Eon's narration -> "Player 1: please put a Skylander on the Portal of Power" -> Chapter 1 played with Spyro (SSA toy,
emulated portal), music and voices playing, 60 fps. In order:

1. **"Save content is damaged"** [Xbox]: the game asks `XamContentGetCreator` before opening a save; ours always said
   "you made it", so the game opened a missing save, failed and gave up. A missing package now gives
   ERROR_PATH_NOT_FOUND (as Xenia); `XamContentClose` of a root that is not open gives ERROR_FILE_NOT_FOUND. Found by
   reading the save state machine: `sub_8226C800` (open request), `sub_8226C9E8` (completion: an error is state 8,
   "create", only when the request may create; else 0x11, "damaged"), `sub_8226D498` (Win32 error -> state).
   Saves go to the game's own data folder (`ARKEYAN_GAME_ID`).
2. **Deadlock after the opening movie** [Xbox]: with no XMA hooks, engine 2's voice code (`sub_823CD288`) waited
   forever for a context lock inside a critical section the main thread then waited for. The XMA hooks above fixed it
   and the sound with it. Diagnosed with `sample <pid>` (host stacks show the `__imp__sub_` frames).
3. **Washed-out menus** [XDK] (21076): `D3DDevice_Resolve` lost its DestLevel argument: slice r8, clear color r9, clear
   stencil at +0x54 (`game::ResolveHasLevel`). The old layout took a garbage clear color for resolve-and-clear
   (flags 0x100), painting the tiled menu pass white.
4. **Skinned meshes** [Xbox]: Giants gives blend indices as FLOAT4; the shaders declare them uint4. Float formats now
   go in as their bits with the `BlendIndicesFloat` specialization constant (`tfetchBlendIndices`); UBYTE4 (SSA) as
   before. 0 rejected pipelines (7 before).
5. **Pipeline baseline** [Giants]: `games/giants/pipelines/baseline.zst` (459 pipelines, two runs); draw-time builds on
   the same path 433 -> 5. Since 2026-10-26 the 91-level tour gives **1575** pipelines, waited for at every level
   load ([history.md](history.md), "Level tour"); since 2026-10-27 tours with other figures make it **2363**
   ([pipeline_cache.md](../../../docs/runtime/gpu/pipeline_cache.md), "Giants").

### Choppy, metallic sound (2026-10-25) [Xbox] (any title; seen in Giants)

Giants' sound engine (Alchemy, not Wwise) mixes everything itself into **one XAudio2 source voice that loops a
1024-sample 5.1 float buffer**, and takes `SamplesPlayed mod 1024` (`sub_8235C518`, voice at +664, loop length at
+708) as its play cursor; the voice render (`sub_823AADF8`) mixes 256 samples per tick ahead of it. Our render driver
refilled the host queue in bursts (the device takes 1024 samples at a time, so 4 frames back to back): SamplesPlayed
jumped by 1024, the cursor stayed put, the title stopped mixing and XAudio2 replayed the stale loop (75.8% of the
output were exact 1024-sample repeats: a 47 Hz buzz, "metallic and choppy"). The driver now calls the clients one frame
every 5.33 ms like the hardware, the device queue only trims the period (`runtime/kernel/audio.cpp`).
The XMA decode was checked and is not at fault: per-stream dumps match FFmpeg's own xma2 decode at 66-85 dB.
Numbers (2 min 45 s, title to Chapter 1): repeated blocks 75.8% -> 0.0%, discontinuities 717/min -> 0/min, 47 Hz line
+10.6 dB -> +0.1 dB, clipping 0% before and after. SSA's output is unchanged (RMS -24.5/-24.4 dBFS, same spectrum).

### Portal prompt (the smart portal) [Alchemy] [Giants] [V] (2026-10-26)
Giants has no prompt object: its portal screens are run by a **game script**, the "Portal Control" state machine, and two
of the script's variables say what is on screen. Public facts: `tsapZ/GiantsRecomp` (its portal page,
`src/game/portal_prompt_watch.cpp`, MIT; THIRD_PARTY_NOTICES). Checked here:
- Script variables are `ScriptReference` objects (vtable `0x820DF750`): `+8` a pointer to the name, `+0x1C` the value.
  `"portal error string"` holds 0 or a `StringInfo` (vtable `0x820DD548`, `+8` a pointer to its key:
  `203_PortalError_TagTypeNotOnPortal` = "Please put a Skylander on the Portal of Power."); `"current state name"` holds a state
  (vtable `0x820DFE40`, `+8` a pointer to its name): `Portal Control - In Game - 0 - Idle` in play, `... Portal Init`,
  `... Portal - 0 - Idle P1 Only` / `P2 Only` at the prompts, `... Magic Moment - 2 / 3 / 4 - ... P1` while a figure comes in,
  `... World Entry - Init`. Several state machines have a "current state name": the one whose state starts with
  `Portal Control - ` is ours.
- The objects live in the physical heap (the `0xA0000000` window; in these runs the message variable at `0xA2F050E4` and the
  state at `0xA2F19458`, not the same addresses as in tsapZ's runtime, so they are **found**, not hard-coded): a thread scans
  `g_physicalHeap`'s allocations for the vtable every 2 s until both are found (a few tens of ms), checks them again on every
  poll (a script loaded anew is found anew), reads them every 30 ms.
- Classification (`ClassifyGiants`, unit-tested): `203_`, `301_ToyMissing` and `304_NeedsRemoval_Minion` ask for a figure (player 2
  when the state says ` P2`); `101_ 102_ 103_ 105_ 106_ 111_ 112_ 305_` are "too many / data" messages, not requests; `306_ConfirmPVP`
  is a question the game's buttons answer.
- Run (`SKY_PORTAL=emulated`, `SKY_PORTAL_PLACE=` empty, a figure in the collection, the presses `a:20,a:30,...,a:130`,
  `SKY_TOYBOX_SCRIPT="Return:100,..."`, `SKY_PORTAL_LOG=1`): `prompt for player 1`, `open`, Enter: `placed`, `P1: Jet-Vac`,
  `the prompt is gone`, `closed`, `the game took Jet-Vac (slot 0, 0.1 s after the change)`.
- [?] the player 2 prompt, "too many toys" opening nothing, and a pause-menu false positive were not run; the states for
  them are in tsapZ's measurements.

### Text language [Alchemy] [Giants] [V] (2026-10-27)
`--language` / `language` / `SKY_LANGUAGE` picks the text (and, with no `voice_language`, the voices and the movie tracks) in any of
the disc's twelve languages: `en fr de es es-419 it nl pt sv no da fi` (`games/giants/language.cpp`, `giants::SetTextLanguage`).

**Why only some worked** [V]: `sub_8217F2D8` (the app's set-up) takes the console language (`sub_8223C248` calls `sub_82297910`,
which maps XC_LANGUAGE 1-12 to an `IG_CORE_LANGUAGE`) and then **keeps it only if the region allows it**: a bitmask over the
`IG_CORE_LANGUAGE` values, one for the PAL AV region (`this+52 == 1`: Danish, Dutch, English, Finnish, French, German, Italian,
Norwegian, Spanish, Swedish) and one for the others (English, French, Hispanic, Portuguese), else English; and outside PAL
Spanish becomes Hispanic. The runtime reports the North American AV region (the title only tests it for PAL), so German, Italian,
Dutch, Swedish, Norwegian, Danish and Finnish fell back to English whatever the setting, and Spanish was Latin-American. The console
languages the title has no number for (Dutch, Swedish, Norwegian, Danish, Finnish: the setting only changed the country) and
`es-419` could never be asked for.

**The hook** [V]: `Language_Region` (`sub_8217F2D8`) runs the original, then writes the chosen language's `IG_CORE_LANGUAGE` value
into the app's `_appLanguage` (`+56`); the packs, the movies' tracks and the scripts read it from there. The console language is
set too where there is one (`es-419` is Spanish to it). Log: `[language] text nl: app language 5`.

**Proof** (2026-10-27, isolated runs to the title, the "Press ... to begin" line, a GPU snapshot): `fr` "Appuie sur ... pour
commencer", `nl` "Druk op ... om te beginnen", `sv` "Tryck på ... för att börja", `de` "Drücke ... zum Starten", `es-419` and `es`
"Pulsa ... para comenzar", `pt` "Pressione ... para começar"; before the hook `nl`, `sv`, `no`, `da`, `fi` and `de` showed English.
The packs hold those lines (`permanent/global.bld`: "Druk", "Tryck", "Trykk", "Paina", "Pressione", "Pulsa" x2, "Premi").
`no`, `da`, `fi`, `it` were run (the hook logs their values 17, 4, 7, 14) and not looked at on screen.

### Voice language [Alchemy] [Giants] [V] (2026-10-26)
The setting `voice_language` (`--voice-language`, `SKY_VOICE_LANGUAGE`; unset = the text's language, as the game does)
plays the voices in another language than the text, like SSA's (`games/giants/language.cpp`).

**What the disc holds** [V]: twelve voice languages, the same as its text languages: English, French, German, Spanish,
Latin-American Spanish (`HISPANIC`), Italian, Dutch, Portuguese, Swedish, Norwegian, Danish, Finnish. Codes:
`en fr de es es-419 it nl pt sv no da fi`.
- Each archive `<name>.arc` holds every language's voice lines as FMOD sample banks (`FSB5`, XMA), named
  `levels/npc_audio/<speaker>/<language>/vo_<line>.wav/0x<hash>.wav.hz.wav.enc`, English without the `<language>/` folder
  (e.g. 12 copies of `vo_eon_lvl213_038` in `level_213_tutotial.arc`).
- Each `<name>.bld` holds one **language pack** per language, `<LANGUAGE>.pak` (`ENGLISH`, `FRENCH`, ... `HISPANIC`): an IGZ
  with one `LanguagePackInfo` (the archive's text) and the archive's localized `tfbSoundDataInfo` objects (named like the
  English ones, `Levels/npc_audio/eon/vo_eon_lvl213_038.wav`, each pointing at its language's stream with its own hash, and
  its lip-sync `tfbPhysicsAnimation`).
- The story movies `fmv01`-`fmv11.mov` (Bink 2) carry 13 audio tracks: ids 1, 5-10, 15, 18, 20, 22, 23 = language + 1
  (one per voice language), and 0 (music and effects).

**How the game picks** [V]: one language for everything, the app object's `_appLanguage` (`*0x826F4258 + 56`, an
`IG_CORE_LANGUAGE` value: 0 Hispanic, 4 Danish, 5 Dutch, 6 English, 7 Finnish, 8 French, 9 German, 14 Italian, 17 Norwegian,
19 Portuguese, 21 Spanish, 22 Swedish), set by `sub_8223C248` from the console language (`sub_82297910`). Its readers:
- `sub_8217A0C0`, the archive loader, for each archive: the enum's name (`IG_CORE_LANGUAGE_FRENCH` + 17 = `FRENCH`) makes
  the path `<archive>.bld:/FRENCH.pak`, loaded by the resource loader `sub_822A8590` before `<archive>.bld:/level.bld`.
  The pack decides both the text and the voices: with `--language fr`, the tutorial streams `npc_audio/eon/french/...`.
- `sub_8222C738`, a movie's set-up: track id = language + 1, given to `BinkSetSoundTrack` (`sub_82150BF0`) by the movie
  start `sub_823344B0` just before `BinkOpen`.
- `sub_821DE5A8`, the scripts' "Game Language" value (Hispanic read as Spanish), and `sub_821DA060` (font metrics for a
  few languages). Changing the scripts' value alone changes no voice (a run).

**The hook** [V]: when two packs are loaded into the same directory, the **first** pack's voice lines play and the **last**
pack's text shows (runs: French then English pack = French voices, English text; English then French = the reverse).
So `Language_LoadPack` (`sub_822A8590`) loads the voice language's pack just before each text-language pack (an archive
without it fails that load harmlessly, as `bootstrap.bld` does for every pack), and `Movie_Open` (`sub_823344B0`) puts the
voice language's track id in the movie's description (+40, when its mode +100 is 0 and the id is the text language's).
Log: `[language] voices fr: permanent/bootstrap.bld:/FRENCH.pak loaded before permanent/bootstrap.bld:/ENGLISH.pak` and
`[language] movie voice track 9 (fr)`. Cost: one more pack per archive (about 150 KB each).

**Proof** (2026-10-26, isolated runs from the title to the tutorial, `SKY_TRACE_FILES=1`, `SKY_AUDIO_DUMP`):
| Run | Subtitles | Streams read from `level_213_tutotial.arc` | Eon's line `vo_eon_lvl213_003` in the audio dump |
|---|---|---|---|
| `--voice-language fr` | English ("This was a dark time for...") | `npc_audio/eon/french/vo_eon_lvl213_002b, 003, 004, 017a, 038, ...` | French version r = 0.84 (51.8 s), English 0.08 |
| `--language fr --voice-language en` | French ("C'était une période sombre pour...") | `npc_audio/eon/vo_eon_lvl213_002a, 002b, 003, ...` | English version r = 0.90 (47.6 s), French 0.07 |

r: the best normalized cross-correlation of the line (the disc's FSB5 sample decoded by FFmpeg's XMA2 decoder, 48 kHz mono)
against the run's audio dump. Every run of both settings reads `npc_audio/arkeyanrobot/danish/vo_arkeyanrobot_lvl213_038`
whatever the language, with or without the hook (the Danish copy of that line; not looked into).
[?] Not run: the story movies with sound (the runs skip them; the track id is logged), the other ten languages
(same packs, same code path).

### Levels past Chapter 1
The level tour (2026-10-26, `tools/pipelines/tour_giants.sh`) loads every level of the disc: 78 of 91 play, 12 crash
(`Fault at guest address 0x78` in `sub_82246390`) and 1 stops: all challenge levels started without their base mesh and
challenge session by the redirect, not runtime bugs ([history.md](history.md), "Challenge levels in the tour").

## Still open
- Challenge levels through the game's own path (a save with the Heroic Challenges bought): the tour's redirect cannot
  start them properly ([history.md](history.md), "Challenge levels in the tour").
- `XMsgStartIORequest(0xFB, 0x000B0006)` (XUserSetContext in Xenia's XGI app) is refused, about 2 per second.
- The `SKY_D3D_CAPTURE` debug log still prints Resolve/DrawIndexed arguments with SSA's layout.
- The portal prompt for player 2, "too many toys" and a pause-menu false positive were not run [?].
- Fixed since: Bink 2 movies (drawn, 2026-10-26), Giants figures and LightCore (2026-10-26).
