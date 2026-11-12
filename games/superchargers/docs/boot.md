# SuperChargers boot: what broke on the way to the first level

> **Purpose:** what stood between SuperChargers' disc and its first level, the causes and the fixes · **Status:** current (2026-11-04) · **Tags:** [Xbox] [XDK] [Alchemy] [SuperChargers] [V] [?]

## Fixed on day 1 (2026-11-03) [V]
- **Two instructions XenonRecomp did not know**: `vpkuwum128` (17 sites; a modulo pack, the low halves of the words) and `vadduhs` (2;
  unsigned saturating halfword add). Added to `patches/xenonrecomp.patch`, with samples in `tools/xenonrecomp/sample_gen.cpp` checked by
  `ArkeyanXenonRecompTest`. No earlier game has them (their XenonRecomp logs are clean), so their generated code does not change.
- **The portal thread aborted at start** (`No code padding left for a host thunk`, signal 6 in `PortalThread`): the title asks for
  `XamInputNonControllerGetRaw` through `XexGetProcedureAddress`, and the runtime places a host thunk in a run of 16 zero words between
  code sections. SuperChargers' code is one section (`0x82450000` + `0x129CDBC`) with no such run: 55124 lone zero words between
  functions, 7 runs of 2 and 7 of 3. `CreateHostThunk` now takes a lone zero word where no function starts when no run of 16 exists
  (`runtime/thunk_slot.h`, `ArkeyanThunkSlotTest`); a thunk is only an entry of the function table, and such a word is never a call target.
  The other games still find their run of 16 first, as before.
- **A trap on a job worker at 13 s** (signal 5 in `sub_82691C70`): `vpkd3d128 v63,v62,3,1,3`, the FLOAT16_2 pack (4 sites), was emitted
  as `__builtin_debugtrap`. Written as the inverse of our `vupkd3d128` FLOAT16_2 (x to the high half of the word that `shift` names,
  y to the low half, the D3DCOLOR form's word convention; the float to half conversion of the FLOAT16_4 form), with a round-trip sample.
  The regenerated code differs only at the 4 sites; the 167 FLOAT16_4 sites are byte for byte the same.
- **Ten imports** (`games/superchargers/imports.cpp`): network and Live calls answer as offline, the audio ducker is off. Seen called
  so far: none of them logged a first call in the runs to 210 s [V].

## The black frame (fixed 2026-11-04) [V]
Every image presented was black (23 shots, menus, movies, intro) while the game drew 110 to 346 draws a frame, none skipped. Two
causes, one after the other, both in our renderer; neither is SuperChargers-specific.

1. **An empty scissor applied with the scissor test off.** The frame dump printed `sc 0 1440 0 1440` (left, top, right, bottom) for every
   draw into the 2560x1440 frame target and for 42 scene draws: an empty rectangle. The game leaves an empty rectangle in the device
   (`D3DDevice_SetScissorRect`, `sub_82871118`, stores it as given at `+0x32FC`) with `D3DRS_SCISSORTESTENABLE` off (`+0x3000`, the dump
   now prints it: `(test 0)`), and that function only folds the rectangle into the window scissor register when the test is on. The
   renderer applied the device's rectangle to every draw (the clear path already checked the flag). Fix: `d3d::DrawScissor`
   (`runtime/d3d/render_scale.h`, tested in `ArkeyanRenderScaleTest`): the whole target when the test is off. The movies, the title, the
   menus' text and the HUD appeared (run `sc7`).
2. **A SHORT4N position read as (y, x, w, z).** The menus' 3D background and the whole world stayed black, and moving HUD elements left
   trails: the frame target was never covered. Its first draw, the composite of the scene (`vs 263a001ff7d5a917`, `ps 72f716a3a7453b07`,
   an indexed quad, stride 20), feeds the position as SHORT4N (`R16G16B16A16_SNORM`). Vertex data is byte-swapped in 32-bit words, which
   swaps the halves of each 16-bit pair; only texture coordinates were swapped back (in the translated shader). The quad got w = z = 0
   and never rasterized. Fix: any other input in a 16-bit pair format has its stream copied on the CPU and its halves swapped back
   (`d3d::SwapHalfPairs`, `runtime/d3d/vertex_pairs.h`, `ArkeyanVertexPairsTest`). No earlier game fed a 16-bit position.

The other games with both fixes [V] (2026-11-04): `tools/test_all.sh` ALL PASS (400 checks; the frames of SSA's level, Giants' three runs and
Swap Force's tutorial within their PSNR bounds: 22.6, 31.5, 28.6, 24.9, 19.5 dB); Trap Team, whose harness section has no game run, to
its tutorial with Gusto (`new:450`) in an isolated run: the whole scene and the HUD drawn, no fault.

The leads of day 1, each checked [V]:
| Lead | Verdict | Evidence |
|---|---|---|
| Device offsets taken from Trap Team | not the cause | the two D3D functions that write the scissor field (`sub_8286E830`, `sub_82871118`) are Trap Team's at shape 1.00 with no displacement moved (`tools/xmatch.py offsets`); the picture is right with Trap Team's offsets |
| The `Resolve` hook (fuzzy match) | not the cause | the scene's resolves hold the picture in every dump (menu, tunnel); the frame is right with the same hook |
| The frame target's depth/stencil state | not the cause | the composite runs with z off (`depthctl 00708730`) and was still empty; the scissor and the position explain all of it |
| The frame target's format, EDRAM base, the gamma/present pass | not the cause | unchanged by the two fixes, which give the full picture (runs `sc7`, `sc12`) |

## To the first level with a figure and a vehicle (2026-11-04) [V]
Run `sc12`: the isolation block, `SKY_PORTAL=emulated SKY_PORTAL_PLACE=new:3400,new:3227` (Fiesta and Crypt Crusher, her own land vehicle,
both blank tags made by our generator), A every 4 s to the play style, Y to skip, A for the dialogues, then RT held (`rt:205:30`, the
tutorial says "Hold RT to Accelerate!") with some steering. 30 fps, no fault in 335 s.
- 14 s `bumper_vvisions.bk2`; the title ("press start"); 32 s `SUPERCHARGERS_OPTIONS`; the play style menu ("Choose your play style", the
  difficulty) over the 3D menu background; 40 s `SUPERCHARGERS_0`; 42 s `fmv01.bk2` and the intro (`intro.pak`): Master Eon's cutscene.
- 181 s the vehicle is read (`undeadcar.pak` and its sound banks), then Fiesta's (`souljam.pak`); without a vehicle on the portal the game
  asks "Please place a LAND VEHICLE on the Portal of Power" and offers an instant vehicle (A takes Hot Streak, `firecar.pak`; run `sc8`).
- From 205 s the rift tunnel ride is played: Crypt Crusher driven by Fiesta, the HUD, the gearbit counter rising (31 at 240 s, 52 at
  300 s), the tutorial's hints ("Use the stick to change direction").
- Portal traffic: `R`, `A`, `S`, `Q` (the figure reads), `W` (writes, answered), `J`, `L`, `V` (lights, ignored).

## SKY_LEVEL and the test (2026-11-04)
**`SKY_LEVEL=<map>`** (`engines/alchemy/zone_request.cpp`): a new game plays its intro and the tunnel ride, then enters `<map>/<map>`
(`academy`: the Academy) instead of `intro1/intro1`. How it was found and what did not work [V]:
- The zone lookup `sub_82DF1C58(list, name)` (it copies the name, turns `\` into `/`, hashes it, returns the zone) was found from the string
  `academy/academy` (its caller `sub_829C2078`). A trace of its calls in a whole run: `sub_82DF9528` enters the zone the game asked for
  next (returning to `0x82DF95E8`): `ui/mainmenubackground/mainmenubackground`, `intro/intro`, `intro1/intro1`; `sub_82A229F0`
  (`0x82A22CC0`) looks up the zone being left. Only the first is a level start (`[level] ... starts`).
- Swap Force's way (write the new name over the game's) does not fit: the names are interned strings sized to their text.
  Giving only the lookup another name returned the Academy's zone, but the loader still opened `intro1.pak`; adding a file
  redirect of `intro1.pak` to `academy.pak` then failed on the package, `packages\generated\maps\intro1\intro1_pkg.igz`, not in that
  archive (a bus fault at guest 0x10 in `sub_82B09528`): every name comes from the request.
- The request: `*(0x83764394) + 44`, a string object (one word: the text's pointer, interned in the engine's pool with a
  reference count before the text). The hook points it at our own text with the pointer's **bit 0 set**, the engine's mark of a
  borrowed text (`sub_82450088` says "not pooled": no count, no free; the copy `sub_824AD560` interns it). Pointing it at the
  bare text faulted at guest `0xFFFC` (the count read before our page).

**The test** `tools/regress/superchargers_lobby.sh` (`SUPERCHARGERS_ISO=<disc>`, ~11 min of game time, through `../gamelock.sh`):
a new game to the Academy (the presses of run `sc12`, RT held through the tunnel, `SKY_LEVEL=academy`), then a second run on a copy of
its saves that continues into the Academy with no `SKY_LEVEL`. Checks for each: `Academy.pak` opened and `[level] academy/academy
starts`, no fault (`analyze.py scan` of `game.log`, the stubs against `private/test-refs/superchargers/`), the fps floor (25) over the
last 20 s, the sound (`analyze.py audio`), the `SUPERCHARGERS_0` save, the last three shots lit. **It passed twice in a row** (runs `lobbyD`, `lobbyE`, 2026-11-04: 15 checks each,
30.0 fps, 20000+ non-silent sound blocks in the new game). An earlier version of its fault check read the console log, where a fault is not
written: it reads `game.log` now.

## Text and voice language (2026-11-04) [Alchemy] [SuperChargers] [V]
The disc has English and German (text and voices). The text follows the console language (`--language`, `[superchargers]`'s setting)
as before; **the voices can now follow another one** (`[audio] voice_language`, `SKY_VOICE_LANGUAGE`): Swap Force's scheme
(`games/swapforce/language.cpp`), re-addressed in `games/superchargers/language.cpp`:
- `Language_Get` `sub_824D7E10`: the console language through `XGetLanguage` to an IG_CORE_LANGUAGE value by a jump table (read from the
  image: English 6, German 9; Swap Force's code instruction for instruction). After the table, a byte global can make it answer a value
  kept elsewhere (`0x83736550`) [?].
- `Language_SoundPak` `sub_8276BA98`: the only user of `"%s/%s_%s.pak"`; while it runs, `Language_Get` answers the voice language.
- `Movie_Open` `sub_826FB690`: BinkOpen's only caller; the movie's description at `+32`, its language track id at `+56`: **5 English, 6
  German** (runs `scU2` and `scU1`); the voice language's id is put there before BinkOpen.
Run `scU1` (`SKY_LANGUAGE=de SKY_VOICE_LANGUAGE=en`): `[language] sound packages: voices en (6), text 9`; the movie track logged 6 (that
build mapped German to 7, fixed since; the movie's voice switch is not re-run [?]).

## Movies: the skip (2026-11-05) [V]
**Y ends a story movie** (`fmv01`), as Imaginators' Y: the adapter's `storyMovieSkipButton` (0x8000); a new Y press read by the title
while a story movie plays (not a logo of `skip_intro_movies`) sets the movie's Bink frame count (HBINK+8) to the frame it shows
(+12) plus one, and the game goes on as at the movie's end (`runtime/d3d/bink_display.cpp`, `game::StoryMovieButtons`, called by
`XamInputGetState` for the first pad). Run `skD` (a new game, Y at 52 s): `[movie] skipped fmv01 at frame 411 of 3905 (the skip
button)`, the vehicle's intro drawn, `prompt for player 1's vehicle`, `souljam.pak` at 91.5 s (195 s when the movie plays); the
same with the movie cut at its first frame (`SKY_SKIP_MOVIES=1`, run `skC`): 61 s. No new fault or stub.

**Why the game's own skip never comes** (runs `skA`, `skB`, debug hooks taken out since). `fmv01` is a cutscene
(`FMV01.pak`: `CCutsceneStage`, `CutsceneActionPlayMovie`, its voice lines). The skip test, `sub_82E7C950`, skips when the byte
**`0x8377677B`** (the info message's `get_ReadyToSkip_Internal`, `sub_82EA9988`) is set and `sub_829E2CD8` allows it (`this+233`,
`PROGRAMMER_VARS` `AlwaysAllowSkipping`, never `Finale01_Outro`, the online checks), or when `sub_8298E780` says so. The byte is
set only by the info message's handler of `CGuiEventNetSkipCutscene` (`sub_82F028E0` -> `sub_82EF1490`), an event made by name
from the game's scripts (its type `0x83777264` has no caller in the code), and cleared at each cutscene's start and end
(`sub_82EF1550`, `sub_82EEA7D8`). During `fmv01` the title reads every press (A, X, Y, B: `SKY_INPUT_LOG`), but the info
message's handler gets no skip state at all, only Start's pause (state 4) and its end (0); the byte stays 0 to the movie's end.
So the cutscene never offers its skip on this path; which script condition holds it back (a first viewing, the online layer) is
not found [?]. Imaginators' info message has a settable `ReadyToSkip` and no `AlwaysAllowSkipping`: another design.

## Save tasks (2026-11-04) [V]
Swap Force's stop at the save slots (its task start cleared the done flag after starting the worker thread) **does not exist here**:
SuperChargers' start, `sub_829AAB90`, sets the worker, clears the done flag (`0x83743058`) and only then creates and starts the thread
(`sub_82475260`, `ExCreateThread`). Its poll `sub_829AAB40` is Swap Force's `SaveTask_Poll` instruction for instruction (busy byte
`+205`, thread `+208`). No hook.
