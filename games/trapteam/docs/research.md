# Trap Team: research before a disc (portal, speaker, traps, imports, GPU, figures)

> **Purpose:** what the references and our own code tell us about porting Trap Team, as leads, and what day 1 on the disc confirmed · **Status:** current (2026-10-30; day 1 done, sections 1-5 are still the pre-disc leads) · **Tags:** [Xbox] [XDK] [Alchemy] [V] [?]

Sections 1-5 were written with no Trap Team disc: **every finding in them is [?]** unless the Day 1 section below says [V], tagged "lead from <source>". Nothing is copied: references gave
mechanisms, written here in our own words. The public facts (release, toys, sources list) stay in
[public background](#public-background-2026-10-25); this page extends it. Read on 2026-10-30:
tsapZ repositories (docs, STATUS, issues and commit messages only, never `src/`), Dolphin and Cemu portal emulation (mechanism only),
rexglue-sdk (a shallow read-only clone, `c94f5eb`, 2026-08-21), the Xenia trackers, and our `office/lessons_cemu_2026-10-28.md`.

| Question | Verdict |
|---|---|
| 1. Portal, trap slot, speaker | **answered** for the protocol, **partly** for the Xbox 360 audio framing (one source) |
| 2. TrapTeamRecomp | **answered** from STATUS, commits and issues; `portal.md` and `reverse-engineering.md` are thin; no "301" page |
| 3. rexglue imports | **partly**: the list of what it implements vs ours is exact; Trap Team's own import table needs the disc |
| 4. Xenia compatibility | **blocked**: no Trap Team entry exists in the trackers (searched below) |
| 5. Our side | **answered** (files and the shape of the change) |

## Day 1 (2026-10-30): the Europe disc, from the XEX to the first level [V]
What `tools/day1.sh`, XenonRecomp, a build and isolated runs gave (details and the fault: [boot.md](boot.md), [status.md](status.md)).

| Question | Result [V] |
|---|---|
| Identity | title ID **`41560911`** (the older page's guess was right), media ID `7A74A451`, version `0x14`; XEX SHA-256 `f1d2373e...`, 9252864 bytes; image `0x00980000` at `0x82000000`, entry `0x8258A820`; 232 imports |
| XDK | D3D 2.0.**21256** (Swap Force 21250, Giants 21076) |
| Engine | **Alchemy**: SSA's inventory (297 entries) finds **177**, 52 at 1.0 (Swap Force 176 / 52, Giants 171 of 293): the XDK and CRT code only. Giants' inventory (38): **35 found, 27 strict** (Swap Force 34 / 21): D3D (11 of 11 strict), XMA (5 strict, 3 at 0.9), `BinkOpen`; not found: the Ultrawide hooks (5) and the display aspect. Swap Force's own engine hooks (zones, language, strings, fonts, ultrawide, save task, swap wait: 12) match **none** at 20 instructions; its two scene-tile hooks hit once each (`0x823EBCD0`, `0x823EF930`), unchecked [?] |
| Archives | IGA **v11**, little-endian header (Giants v8, Swap Force v10 big-endian); `.arc` + `.bld` pairs like Giants; `tools/shaders/giants_extract.py` finds 0 entries |
| XenonRecomp | one config, no new instruction, clean log: 336 jump tables (16 `bctr` left, all virtual tail calls at `0x821F9F0C`, `0x82375E84`-`0x82375F60`, `0x82643C64`/`CA4`/`D34`), 12193 explicit functions (pass 2 adds 1), **29961 functions** in the mapping (Giants 23631, Swap Force 82017), 121 files, 119 MB, 3 s |
| C++ EH | back (237 `__CxxFrameHandler` sites, 60 `__C_specific_handler`) |
| Imports | 5 beyond our 295: `NetDll_XNetQosServiceLookup`, `NetDll_XNetQosRelease`, `ObLookupThreadByThreadId`, `ObOpenObjectByPointer`, `XamGetLocaleEx` (section 3's guess, "the XAM locale group is the first miss", was right: `XamGetLocaleEx` is called at start-up). None of rexglue's other 219 was needed so far |
| Shaders | run-time dump: 69 containers by the title, 225 by the tutorial's start; 189 translate, **36 vertex shaders stop on `TexCoord 8`** |
| Build and boot | `build-trapteam` links (`TTRecomp`); first draw 0.5 s; title at 15-40 s; autosave notice, `fmv1`, then `level_313_tutorial` open; **that level faults** on the render thread (null vertex buffer address in `sub_823F0128`) |
| Figures | `ArkeyanFigureTest --make` writes a Trap Master (450), a core (452) and a trap (211) as blank tags, and `ArkeyanFigureTest <dump>` decrypts them with 0 failures out of 260 (our own generator and reader: a round trip, not a proof for a real Trap Team tag). No trap was placed (the game did not get to a menu that asks for one) |

### Sigmatch against Giants and against Swap Force, side by side (2026-10-30) [V]
Same method (`tools/sigmatch/sigmatch.py`, signatures made from each game's own image and function list), each game's **current** inventory
made by `tools/sigmatch/game_inventory.py <game>` (the committed `giants_hooks.json` is older: 38 entries, no LZMA, language or movie hook),
matched against Trap Team's image. Entries are hooks, data globals and the ten `[main]` helper addresses.

| | Entries | Found | At 0.9 or more | Strict (1.0) |
|---|---|---|---|---|
| Giants' inventory | 43 | **38** | 32 | 28 |
| Swap Force's inventory | 50 | **39** | 28 | 23 |
| The 34 names in both, same function set | 34 | Giants 33, Swap Force 32 | 32 / 26 | 28 / 22 |

- **Same address from both** (27 of the 34): D3D `InsertCallback`, `DrawVertices`, `ClearF_Worker`, `Swap`, `D3DVertexShader_Init`, `D3DPixelShader_Init`, seven of the eight XMA hooks, `Engine1CriticalSection`, `BinkOpen`, `BinkClose`, `LzmaDec_DecodeReal`, the ten helpers.
  `addresses.h` takes them (Swap Force's mapping where it ties).
- **Both found, different address** (5): `NextRingSegment`, `KickOff`, `BlockOnFence`, `DrawIndexedVertices`, `Resolve`. Giants' are strict (1.0); Swap Force's are fuzzy (0.52 to 0.80), 4 to 24 bytes off a neighbour or outside the D3D library
  (`Resolve` at `0x8268C3C8`): Swap Force's D3D wraps these functions differently, Trap Team's are Giants' shape. **Giants' addresses are kept** (the game's own `Lock` calls `BlockOnFence` with the flag in r6, Giants' signature).
- **Giants matched, Swap Force did not**: `Xma2_Init` (0.9), and the Giants-only hooks (`Ultrawide_*`, `Language_LoadPack` 0.55, `AppObject`): not in Swap Force's inventory.
- **Swap Force matched, Giants has no entry**: `LzmaDec_DecodeReal` 1.0, `Scene_Tiles` 1.0 (`0x823EBCD0`), `Scene_SurfaceTiles` 0.9 (`0x823EF930`), `Language_Get` (sigmatch 0.61, shape 0.83, asm read: same code, `0x823229A0`).
- **Swap Force's other engine hooks**: weak candidates only (`SaveTask_Poll` 0.50, `String_Intern` 0.52, `Font_CharMetrics` 0.55, `Ultrawide_ScenePass` 0.60), none for `Zone_IndexByName`, `SwapWait_Status`, `NightmareUnlocked`, `FallCheck`, `Language_SoundPak`, `Movie_Open`, `Ultrawide_WorldCamera` and the data globals.
- Reading: Trap Team is **between** the two. Its D3D and XMA are Giants' (strict), its CRT and LZMA are common to all three, its engine hooks are neither's.

### Reuse, hook by hook (2026-10-30) [V]
Order: `engines/alchemy/`, then each `games/swapforce/` hook re-addressed by sigmatch (`tools/xmatch.py` shape as the second witness), then new code.
A hook that is not needed stays out of `addresses.h` (compiled out).

| Hook (Swap Force's file) | Result for Trap Team | Evidence | Reused or new |
|---|---|---|---|
| `LzmaDec_DecodeReal` (`engines/alchemy/lzma.cpp`) | **found** `0x8239F0C0`, in `addresses.h` | sigmatch 1.0, shape 1.00 (844 instructions), `SKY_LZMA_CHECK=1` over a whole run to the first level: 0 calls differ | engine's, reused |
| `Zone_IndexByName` (`engines/alchemy/zone.cpp`, SKY_LEVEL) | not found, not needed yet | sigmatch none, shape best 0.38; the level opens by itself; Swap Force's key string `generated/maps` is absent | stays out; Giants' `adapter.firstLevel` path redirect is the other way [?] |
| Ultrawide (`engines/alchemy/ultrawide.cpp`, `games/swapforce/ultrawide.cpp`): `Ultrawide_ScenePass`, `Ultrawide_WorldCamera`, `Scene_Tiles`, `Scene_SurfaceTiles` | `Scene_Tiles` `0x823EBCD0` and `Scene_SurfaceTiles` `0x823EF930` **found**; `ScenePass` candidate `0x8243F650` (sigmatch 0.60, shape 0.63) unverified; `WorldCamera` not found | the scene is drawn at 1040x620 as in Swap Force (`vp 1040 620` in the frame dump); Scene_Tiles shape 1.00 (170 instructions), SurfaceTiles shape 1.00 (68) | **ultrawide enabled** (2026-10-30): `ScenePass` is `0x8243F650` (asm read, result at +92/+96), the camera is the projection builder `0x823B1AF0`, not Swap Force's `WorldCamera`; the tile hooks **are** needed at the full-resolution scene (1280x720 splits into 2 tiles; hooked 2026-10-31): [status.md](status.md) "The picture", "The constant blur" |
| `FallCheck` (`fall_check.cpp`) | **not needed** | the reflection names `_stuckFallingDistanceThreshold` and `_stuckFallingTimer` are in Swap Force's image (3 hits) and **not** in Trap Team's (0): the check does not exist | out |
| `Language_Get` / `Language_SoundPak` / `Movie_Open` (`language.cpp`) | `Language_Get` **found** `0x823229A0`; the other two not found | `Language_Get`: shape 0.83, same 17-entry switch and the same `XGetLanguage` wrapper (`0x8258A230` against Swap Force's `0x82F41C48`, instruction for instruction). `SoundPak`: the format `%s/%s_%s.pak` is in Swap Force (1 hit) and not in Trap Team (0); `Movie_Open` shape 0.33 | in (2026-10-31): `Language_Get` and the console language wrapper `sub_8258A230` answer with the host's text language (the chooser tests the console language: Dutch is 16), and the pack (text and voices) follows ([boot.md](boot.md#language)); independent pack and movie hooks added 2026-11-01; issue #51 fixes the global-pack-only availability check with shared speech detection (2026-11-11), real-run proof pending ([checks](boot.md#lb-and-independent-voices)) |
| `SaveTask_Poll` (`save_tasks.cpp`) | candidate `0x821CCD50` (0.50), rejected; not needed so far | `SL_LOAD_SLOTS` and the other task names are in Swap Force (3 hits), not in Trap Team; `Save_Header` and `Save_Slot_1`-`4` are created and the game goes on | out |
| `SwapWait_Status` (`swap_wait.cpp`) | not found, not needed so far | shape 0.43; sampled render thread waits normally (`NtWaitForSingleObjectEx`), no spin | out |
| `NightmareUnlocked` (`engines/alchemy/nightmare.cpp`) | not found | "nightmare" is 14 strings in Swap Force, 2 in Trap Team (only an achievement text): the property is not there; the lock is in the title level's scripts ([status.md](status.md), "Nightmare", 2026-11-05) | out |
| Portal prompt (`swapforce_portal_prompt.cpp`, `CMagicMoment`) | not applicable | `magicMoment` is 138 strings in Swap Force, 2 in Trap Team: no such class | **new**, after the main menu (script variables like Giants', section 2) |
| `String_Intern`, `Font_CharMetrics` (`engines/alchemy/prompt_text.cpp`) | candidates `0x8234B5B0` (shape 0.51) and `0x823CD448` (0.57), unverified | the title already draws the console's own A button ("Press (A)"); the hooks only swap glyph styles | out until glyph styles are wanted |
| `Gfx_ConfigInit` (`games/trapteam/vram_copy.cpp`) | **new**, found by the config string `Gfx/@enableVramCopy` | [boot.md](boot.md): the video pool is full; no Giants or Swap Force equivalent | new |
| Portal status pacing (`runtime/host/portal_emulated.cpp`) | **new** in the shared portal path, commit `8347ed7`, not merged before `test_all` | [boot.md](boot.md) | new (nothing equivalent existed) |
| Traps in `runtime/host/portal*`, `figures/` | not touched: no trap was placed | swappers (two toy types, block 6) stay the precedent: extend `ToyKind`/`LevelCap` there, not a parallel path (section 5) | to do |

Answers to the open questions of sections 1, 3 and 4:
- **Portal identity**: `R` answered `02 1B` (Traptanium, Xbox 360) is accepted; the game goes on to the title. `A`, `S` as in the other games.
- **New portal traffic**: `V` (`56 00 01 3F`, later `56 00 01 19`), `L 01` and `C` at the start, `J side r g b D0 07` (a fade of `0x07D0` = 2000 ms, little endian)
  every few seconds on the title; all unanswered by us and the game does not stop for them. `M` (speaker) not seen yet: that needs a trap.
- **The portal thread is a separate guest thread named "Portal Audio"** that reads until a read returns nothing, then sleeps 2 ms: an
  endless status stream hangs the game; a status per 8 ms is enough. Its reads may carry the speaker's audio too (to measure with a trap in).
- **XDK and device layout**: the D3D functions hooked are strict matches of Giants' at 1.0, so their offsets are Giants' (`DeviceStateShift` 0x80,
  vertex declaration 0x2FD0, no base vertex in `DrawIndexedVertices`, no level in `Resolve`), and the frames render. Not yet checked draw by draw.
- **Boot cost**: apart from the portal loop and five imports, no title-specific fix was needed to reach the title and the first level's load.

Leads read on 2026-10-30 for the day-1 work (ideas only, nothing copied): none was needed to get this far; the references were not opened.

## 1. The Traptanium portal against the portal we emulate

### What we emulate today (read in our code)
- `runtime/host/portal_emulated.cpp` answers `R`, `A`, `S`, `C`, `Q`, `W`; every other letter (`J`, `L`, `M`, `V`) is **ignored with one log line and no
  reply**. 16 slots, 2 status bits each (`EmulatedPortal::SlotCount`). Xbox 360 reports carry a `0B 14` header, added and stripped in
  `runtime/kernel/portal.cpp` (XAM ordinals `0x4A1`/`0x4A2`; one `GetRaw` per poll and at most **one** `SetRaw`, polled every 12 ms).
- `runtime/host/portal_identity.cpp` already names the Trap Team ids (`02 18`, `02 1B` Xbox 360, `02 19` mobile) for the `R` answer
  but gives a model per game only for SSA, Giants and Swap Force (`02 1B` for Swap Force).

### Identity and the trap slot
- **The game checks the `R` answer and rejects every portal but the Traptanium** (`02 18`, `02 19`, `02 1B`, `02 27`; the older portals' ids give a
  "wrong portal" screen). A trap needs a physical trap slot; changing the id does not create one. [?] (lead from SwapForceRecomp `docs/portal-compatibility.md`, itself citing RPCS3 reports)
- **Protocol-wise a trap looks like any other figure**: it is one of the portal's slots, read and written with `Q`/`W` like a character. Dolphin's
  portal code has no trap-specific logic besides one LED path; traps are only entries in its figure database. [?] (lead from Dolphin `Skylander.cpp`)
  So "the trap slot" is probably **a convention of the game** (which slot index, or which toy type, is a trap), not a protocol feature. To settle on
  a real capture (`SKY_PORTAL_LOG=1`).
- The fan port gives the trap **its own menu slot**, one trap at a time, because placing a trap through the normal menu lifted the player's
  Skylander (their issue 11). It reads the game's script variables to know the game wants a trap and for which element (`1000 + villain` for the vault value; Gulper = villain 9,
  trap type 211, Water). [?] (lead from TrapTeamRecomp STATUS, issues 11, 12). Our catalogue's 211 is "Water Trap": the numbers agree.
- Mini figures: public lists say they are ordinary figures (a toy type, progress) sold small; our catalogue has 8 minis for the game
  and Giants' sidekicks use the same category. Nothing portal-side differs. [?] (lead from the older page, our catalogue)

### What is on a trap and on a villain (the unknown that matters most)
- A trap tag holds a **captured villain**; the villain is not a toy of its own (our catalogue keeps the `villain` category empty for that reason).
  Where on the tag the villain id and its progress sit is **not documented in anything read**. The SuperChargers trophy bitmap
  (one byte of bits per sector, lead from Kneppers) is an analogue only. [?]
- A trap's variant in block 1 is its **shape** (51 shapes in our catalogue, `VariantKind::Shape`); the Kaos Trap has its own shape names.
- Villain pictures are on the disc (`misc/ui_villainvault_stream.arc`, 46 pictures, lead from TrapTeamRecomp STATUS): the villain list is derivable from the disc, not from a tag.
- Trap tags use the same MIFARE Classic 1K layout as other figures; whether Trap Team's encryption constant and areas are the same as Swap Force's is **unknown** (Imaginators changed the scheme, so do not assume it for later games either). [?]

### The speaker (villain voices)
Three sources, partly in conflict; the Xbox 360 case is the one that matters to us.
- **PC, Wii, Wii U, PS (USB HID):** the game sends `M` with a flag byte to switch the speaker on (the portal answers with 4 bytes: letter, the flag,
  zero, and `0x19`, read as an audio firmware version), then streams audio on the **OUT interrupt endpoint** in blocks longer than 32 bytes,
  **64 bytes = 32 samples of 16-bit mono, 8 kHz**, little-endian samples (the older gist says byte pairs swapped, i.e. the other order: to check). Dolphin plays them as they come,
  Cemu feeds an 8 kHz mono device with blocks of 32 samples. [?] (lead from Dolphin `Skylander.cpp`, Cemu `Skylander.cpp`, the Wilson gist)
- **Xbox 360 portal (`1430:1F17`):** the wire carries **compressed audio**. Cemu's Xbox 360 portal bridge takes the PCM a Wii U game sends and turns each
  4 bytes (two samples) into **one byte of two 4-bit codes of the ITU-T G.721 ADPCM coder** (32 kbit/s, 8 kHz), low nibble first, under a different
  2-byte header (`0B 17`; commands use `0B 14`), and resets the coder state when `M` with flag 1 arrives. So an Xbox 360 game sends about
  16 payload bytes per 32 samples. The fan port also says the speaker stream is G.726 (the same family). [?] (lead from Cemu `SkylanderXbox360.cpp`, SwapForceRecomp `portal-compatibility.md`, TrapTeamRecomp `portal.md`)
- **Answered 2026-11-02** (a run with a trap; [portal.md](../../../docs/runtime/host/portal.md), "The speaker"): 32-byte reports, `0B 17` and 30 bytes of G.721, low nibble first, one every 7.5 ms. **Framing we did not know:** the exact packet length on the 360, the header bytes after `0B 17`, whether samples are split across packets, and
  the rate at which packets come. **Risk:** our portal thread takes **one `SetRaw` per 12 ms poll**; 32 samples per packet at 8 kHz is one packet every 4 ms, so
  either Trap Team batches more per packet, or its audio goes through a faster path than the portal thread. Measure first (`SKY_PORTAL_LOG=1`).

**What our portal needs to route villain voices to the host speakers [?]:**
1. Answer `M` (today ignored): a 4-byte reply as above, otherwise the game may wait on it and show a portal error.
2. A **decoder**: ITU-T G.721/G.726 32 kbit/s, 4-bit code to 16-bit sample, written by us from the public recommendation (it is a small, well-specified
   algorithm; no outside code). A unit test with a generated stream (encode in the test, decode, compare) works without any game data.
3. A **new output path**: `runtime/host/audio.h` today takes one 5.1 float frame of 256 samples at 48 kHz from the guest's mixer. The speaker is a second,
   independent mono 8 kHz stream with its own clock: a **second SDL stream** on the default device (resampled by SDL), not mixed into the guest frames, with its own
   volume and mute (the fan port has a 0-100 % speaker volume). Under `SDL_AUDIO_DRIVER=disk` it must also write to disk, so tests can compare it.
4. A **drop policy**: a late or cut-off line must not block the portal thread (the fan port lost the last 1384 samples of each line until a decoder fix and had a
   "who is talking" overlay disappear after about a quarter second of silence; both were their bugs, worth a test of ours).
5. A **trap-removed rule**: when the trap leaves the portal, stop and flush the stream (their issue 14).

### LED commands
- `C r g b`: all lights, no reply (done). `J side r g b <16-bit fade time>`: fade one side, a one-letter reply (Dolphin ignores the time and replies). `L side r g b`: set one
  side or the trap-slot light, **no reply**. The trap-slot LED is lit while a trap is read; side numbering is inconsistent between Dolphin's comment and its setter, so
  take the numbers from a capture, not from the references. [?] (lead from Dolphin, Cemu, the older page)
- Our cost: remember the colour per side like `Light` does for `C` (the toy box could show it later). Not needed to boot; the reply to `J` is.

## 2. tsapZ/TrapTeamRecomp (docs, STATUS, issues, commit messages)
Read: STATUS, `portal.md`, `reverse-engineering.md`, `roadmap.md`, the 17 issues (all closed), the newest 60 commits (2026-10-27 to 10-30; older ones not read).
- **Boot was cheap.** Date 2026-10-21: no codegen fix needed, title screen after the logos; only **14 functions the analyser started one instruction late**
  had to be listed by hand (SuperChargers and Imaginators stopped on the same kind at boot). Scaffolded from the Giants, SSA and Swap Force ports: that is itself
  evidence that Trap Team shares their engine, so **Alchemy is likely**; the script-variable names below look like Giants' too. [?] (lead from README and STATUS)
- Runtime bugs they met: a settings key written twice made the whole TOML invalid (silent); a null read on the vblank thread when the window moved to another monitor;
  an SDL audio start race (exception `C0000264`, seen once); a CRT `longjmp` that never returned (a caught script error became a crash; fixed by using the XEX's own
  `setjmp`/`longjmp`); the XMA decoder dropped its last two frames (cut voice lines) and a stream could stick past its buffer; GPU register writes were the
  frame-time cost at 1080p until made cheaper; a write-watch crashed only Trap Team once. [?] (STATUS, commits of 2026-10-28)
- **The portal.** The game is script-driven: they read two script variables found by scanning the heap and polled every 30 ms: the "player state" macro
  (Need Toy = the game waits for a Skylander; Clear = a figure arrives, about 7 s) and the "portal state, tag state" (0 normal; others name an error screen: *too many
  minions*, *too many items*). The portal error strings use numbered keys: a **`304_` key = "needs a break"** (Skylander tired). The **"301" portal error** is the game
  asking for a Skylander back (the one that was lifted): their commit of 2026-10-29 shows a menu naming it and returning it "as in Giants", and says it was **not yet seen in Trap Team**.
  So 301 is a Giants-family prompt (to compare with our Giants prompt code); the Trap Team side is unseen even in their port. [?]
  Their portal read/write are XAM ordinals 1185/1186 (= our `0x4A1/0x4A2`).
- **Traps and villains.** Trap request: the menu opens on the element asked, but "the request itself has not been seen in a real capture" (their issue 12 note);
  vault value `1000 + villain`; each trap card names its villain (issue 17); speaker tested with a trap of a villain the game then "summons" (Chompy Mage, 2026-10-23).
  Open limits they list: no real capture of the trap request, "needs a break" unmeasured, Swap Force halves' upgrades.
- **Credits** (public handles): the base is GiantsRecomp (TheBiemGamer), their own `skylanders-portal` library (menu, USB, catalog), SkyReader (NFC format), a ReXGlue fork.
  Nothing of theirs to copy; our portal prompt is ours (`games/swapforce/swapforce_portal_prompt.cpp`).

## 3. rexglue-sdk: kernel and XAM imports as a checklist
Method: the names `REX_EXPORT`ed under `src/kernel/` of the shallow clone (460) against our `runtime/kernel/import_list.inc` (295, SSA 218 + Giants 33 + Swap Force 44).
Names only; nothing copied. Full lists in `private/trapteam/` (git-ignored, regenerate with the same method).
- **241 names are in both.** **54 are ours only**: `NetDll_XHttp*` (13), `NetDll_XNet*` (8), `XamShow*UI` (5), the XAuth and token calls, `Io*Share*` and device calls, a few C-runtime ones.
  Those are Swap Force's and Giants' Live-side imports: rexglue does not have them, we do.
- **219 are rexglue's only**, the candidates if Trap Team imports them (the only way to know is the new `default.xex`'s table: `tools/day1.sh` diffs it):

| Family | Count | Names that matter most |
|---|---|---|
| `Xam*` | 63 | locale and format (`XamGetLocale*`, `XamGet*String`, `XamFormatDate/TimeString`), content (`XamContentCreate*`, `Resolve`, `Flush`, `GetDeviceName`), `XamGetCurrentTitleId`, `XamUserReadProfileSettingsEx`, `XamUserGetGamerTag`, `XamSwapDisc`, `XamLoaderGetDvdTrayState`, `XamFeatureEnabled`, party, avatar, Nui |
| `XeCrypt*`, `XeKeys*` | 25 | AES, SHA, HMAC, RC4, DES3, random: save or token integrity checks; real code to write if reached |
| `XMA*` | 19 | the XMA context calls; our import list has only `XMACreateContext`/`XMAReleaseContext`: a title that imports the others is new [?] |
| `Ke*`, `Nt*`, `Rtl*`, `Ex*`, `Mm*`, `Ob*` | about 60 | semaphores, read/write locks, I/O completions, suspend, `RtlComputeCrc32`, `MmAllocatePhysicalMemory`: cheap, generic |
| `NetDll_*` | 12 | `WSAWaitForMultipleEvents`, others |

- **Weak stubs we have** (77 at the last count, [kernel_exports.md](../../../docs/runtime/xbox360/kernel_exports.md)) are fine for a first boot; the rule stays "implement with outputs written".
- Which of these Trap Team imports is unknown until the disc; the XAM locale and content group is the likely first miss (a guess).

## 4. Xenia compatibility for Trap Team (blocked: nothing to read)
- `xenia-project/game-compatibility`: searches for "skylanders", "trap team" and the title id `41560911` return only #1518 (Spyro's Adventure, `415608AB`, open) and
  Skylanders appears once more in `xenia-project/xenia` #2320 (a feature request for a portal emulator, open) and #2145 (Giants, closed). **No Trap Team entry, so no crash list.**
  (The title id of the older page, `41560911`, is therefore also still unconfirmed by this tracker.)
- Consequence: the GPU feature list cannot come from Xenia. Proxy: the fan port runs on the same recompiled-Xenos approach as ours and lists only **quality** work (native 3D at 1264x704 instead of 1040x620,
  bloom and motion blur options, ultrawide camera hook, softer shadows when the texture offsets stop being divided by the scale, pixel positions at scale 2+), all of
  which exist for Swap Force and which we mirror in `docs/runtime/gpu/` ([render_bugs.md](../../../docs/runtime/gpu/render_bugs.md), [ultrawide.md](../../../docs/runtime/gpu/ultrawide.md)). [?] (STATUS)
  Expect no new Xenos feature at boot; **new effects** (ring and villain-vault screens, Light/Dark effects) are only found with a disc.
- Cemu's default for Trap Team is 30 fps ("far faster animations than intended" at 60, in `lessons_cemu_2026-10-28.md`): start at 30, add 60 after the step-dependence list passes.

## 5. Our side: what adding trap, villain and mini figures takes
Closest precedent: Swap Force swappers (two toy types, one per half, block 6; [figures.md](../../../docs/runtime/host/figures.md), [history.md](../../swapforce/docs/history.md) "Figures of each type").
**Good news: the collection layer already knows Trap Team** (a `trapteam` game with 63 toys, a `trap` seat, trap shapes as variants, `villain` category reserved). The figure layer does not.

| File | Shape of the change |
|---|---|
| `runtime/host/figures/figure_catalog.cpp` | `ToyKind` has no `Trap`: in `Toys()` the mapping sends `Trap` and `TrapMaster` to `Character`, `Mini` to `Sidekick`. Add `ToyKind::Trap`; decide `HasProgress` (a trap may store a villain's progress; unknown) and the trap images. |
| `runtime/host/figures/figure_tag.cpp` | `LevelCap(game)` returns Giants' 15 for any unknown id: **Trap Team would silently get 15**. Add the game's cap (public sources say 20, as Swap Force: [?]) and, if the game adds an area (villain data), extend the A/B scheme beside the extra areas (blocks 17-21 and 45-49). Needs a real dump. |
| `runtime/host/figures/figure_data.cpp` | `CreateBlankFigure` writes identity only: fine for characters and minis (the game initialises a zero figure, [V] for Giants and Swap Force); a **trap needs its villain field**, and probably a valid blank trap state: from a dump. |
| `runtime/host/figures/figure_crypto.cpp` | Same key scheme as Swap Force unless a dump proves otherwise. First check on a real figure: decrypt, then the block checksums. |
| `runtime/host/figures/figure_test.cpp` | The cross-game round trips (`--make`, `--roundtrip`, `--same-extra`) gain Trap Team rows; the bug fix rule applies (test first). |
| `resources/collection/catalogue.json` and `runtime/host/collection/catalogue.cpp` | Already there; fill the `villain` category only if a villain turns out to be addressable (46 pictures on the disc); check the Trap Team re-released sidekicks (`0x3000`) once a Trap Team mini is read. |
| `runtime/host/portal_emulated.cpp`, `portal_backend.h` | Answer `M` and `J`; keep the last `L` colours; hold a trap in a slot like any figure; later an audio hook for the speaker (section 1). 16 slots are enough (Swap Force used the same). |
| `runtime/host/portal_identity.cpp` | A `trapteam` model with `02 1B` and a rule: accept only Traptanium ids; the `Wrong Portal` text for older ones. |
| `runtime/host/toybox.cpp` and the game's prompt file | Opening on the **trap seat** with the element the game asks, like the existing seat opening for the game's prompt; the game's own hook for "needs a trap" is [?] (their script variable is the lead). |
| new `games/trapteam/` | Copy the shape of `games/swapforce/` (`adapter.cpp`, `info.cpp`, `addresses.h`, `config/`, `game.txt`, `docs/` pages) via `tools/day1.sh`; engine hooks stay in `engines/alchemy/` if the check says Alchemy. |
| docs | `figures.md` (Trap Team rows), `collection.md`, `portal.md` (J, L, M, speaker), this page's list. |

## First steps when a disc exists, ordered by risk
1. **Day 1 and the import diff** (`tools/day1.sh trapteam <iso>`): title id, sigmatch hit rate against Giants and Swap Force (engine), the import table against sections 3 and the 77 stubs.
   Also confirm the title id and the Xenia nothing of section 4.
2. **Portal identity and the speaker commands** with `SKY_PORTAL_LOG=1` and the emulated portal: does the game boot past the portal check with `02 1B`? What does it send for `M`/`J`/`L`, the length and rate of the audio packets
   (the 12 ms `SetRaw` limit above is the top risk). Log only; no decoder yet.
3. **Cheap generic misses**: implement the XAM locale and content imports the first run asks for.
4. **Boot to the title and Chapter 1** with a blank character (works for Giants/Swap Force without any new figure code); then a **blank trap** to see the game's reaction to a figure with no villain.
5. **Trap and villain data**: a real trap dump (owner's) to find the villain field and its areas; the figure layer changes of section 5 follow.
6. **Speaker**: decoder, second output stream, tests (section 1).
7. **Trap request prompt** (menu opens on the asked element) and the "needs a break"/`304_` and `301` portal screens.
8. **60 fps** after the step-dependence list, animations first.

## What to ask the owner
- **A real portal capture on real hardware** (Traptanium on an Xbox 360 with a game running, USB analyser or a capture through the portal probe): the `R` answer, a trap placed, a villain speaking (the audio packets, their length and timing), the `L` and `J` traffic with the trap in and out.
- **Dumps from real figures** (private, never in git): one trap with a villain, the same trap empty, one mini, one Trap Master, one character used in Trap Team: to find the villain field and compare the areas.
- **A Traptanium portal** for the probe and the speaker if the owner has one (it also answers whether the 360 portal accepts our decoded stream).
- Which Trap Team disc (region): the title id and the media id.

## Frame rate: 60 (2026-10-30 and 31, lane I) [TrapTeam]
- **The game paced itself at interval 2** [V]: at `fps_limit=60` the first-level intro cutscene (`313_intro_igc_stream`) still ran at 33.3 ms a frame (29.6 fps, two runs) while the title and menus reached 60.0.
  Same as Swap Force's Alchemy: the adapter sets `presentIntervalPacesGame` (the runtime writes interval 1 before each swap at 60, `d3d/render.cpp`) and `maxFpsLimit = 60`.
  No sigmatch hook was needed: Swap Force's `SwapWait_Status` has no match in this binary (0.0), and `D3DDevice_Swap` was already hooked.
- **Proof at 60** [V] (`tools/regress/framerate.sh trapteam <disc> 60`, offscreen, `--vsync 0`): menus/title 59.9 fps, gap p50 and p99 16.7 ms; the cutscene reached 60.0 fps in 5 of 5 runs without pictures, the tutorial's gameplay 59.8-60.0 in three runs (`[perf]` lines).
  A frame that does not fit a blank three times in thirty turns it to 30 by the runtime's `HalfRate` (`[video] fps limit 60: frames miss their 16.7 ms blank`): it happened when the pictures of the speed test stalled the frames, and once in four under a busy machine.
- **Pictures stall the frame, and that moved the clock** [V]: each `SKY_GPU_SNAPSHOT` picture made the next frame 2 to 4 frames long (73 ms at 30, 55 ms at 60, `SKY_FRAME_TIMES`). With one picture every 2 s the cutscene took 31.2 s at 30 and 30.6 s at 60; with none it takes the same time (below). A measurement of the game's speed takes as few pictures as it can.
- **Cutscene clock, 30 against 60** [V] (`tools/regress/cutscene_clock.sh trapteam <disc> <fps> <name>`, no pictures: from the open of `313_intro_igc_stream.arc` to the first `Save_Slot` the game opens after it, both from the log; 5 runs each):
  30 fps: 30.64 s every run (30.64, 30.64, 30.64, 30.64, 30.64); 60 fps held: 30.56, 30.58, 30.57, 30.56, 30.56 s. The gap is 0.07 s (0.2 %), two frames at 30: the same duration. (The 0.5 s gap of the first runs came from the pictures.)
- **Gameplay speed** [V for the timing, [?] for the walk's distance] (`tools/regress/gameplay_speed.sh`, one picture a run so that nothing before it is stalled; Gusto's blue, the mean row of the blue pixels, in the picture taken at a fixed second): the jump (A, 0.15 s) at 0.4 s after the press: 202, 202 px at 30 and 229, 229 px at 60; at 0.7 s: 291 and 326 px at 30 (a frame of difference: the descent is fast), 322 and 324 px at 60. Gusto is at most one 30-fps frame (0.03-0.07 s) further along at 60 at both moments, the same offset, not a growing one: an input latency, not a faster game. Not measured unperturbed: the airtime itself (the 10-pictures-a-second series gave 0.94 s at 30 and 0.77 s at 60, 0.85 and 0.74 s with a picture every 0.2 s: it moves with the pictures, so it is the stall, not the game). The walk (left stick to the right, 2 s) ends against a cliff in 1.5 s at both rates (the same place), so it gives a timing (the plateau 0.2-0.5 s earlier at 60 in the series with pictures, not looked at unperturbed) and no distance.
- **Fixed per-frame steps** [?]: Swap Force's stuck-falling check is not in this game (no `stuckFalling` string in the image; `FallCheck` has no match), so `FrameScale()` has no user here. The cutscene and the jump show none at 60; the frame counters of a fade or a wait were not hunted (lane C's `SKY_HUNT` reads a Giants address).
- **Above 60** [V]: capped, `[settings] fps_limit=120: this game is capped at 60 FPS, using 60`. With `SKY_NO_FPS_CAP=1` at 120 the game still shows 60.0 fps in the tutorial (the `[perf]` lines of three runs: 59.8-60.0): nothing above 60 was reached, so 120 is untested, not proven.

## Liquids: the ripple simulation ran twice as fast at 60 (2026-11-01) [TrapTeam]
- **Why the earlier hunts found no constant** [V]: the ripples come from a height-field simulation of three full-screen passes into a 512x512 target (each followed by a resolve; vertex/pixel shaders `74B7940A`/`0682358C`, `C6150DC3`/`C7B28C59`, `DEBF35CD`/`C8C6CF1A`), drawn once for every frame the game renders. Their pixel constants never change from frame to frame (`ps c0-c5` = 0.3, 1.8, 0.955, 2000 ... in the first pass: a fixed weight and a damping of 0.955 a step), so there is no clock in them to find: the step is the frame. The same three draws were found in the tutorial and in `level_304_water_wave`, at 30 and at 60. The earlier 60 runs were not 60: the runtime's automatic drop to 30 was on, and the binary pre-dated the switch (`fps_auto_fallback=0` in `[trapteam.video]` keeps a real 60: 59.5 fps measured).
- **Cause** [V for the passes and their rate, [?] for seeing the waves]: one step per rendered frame is 60 steps a second at 60, so waves travel and fade twice as fast as at 30. No wading or rain was in view in the runs (the passes stay flat there), so the ripples themselves were not compared.
- **Fix**: `game::Adapter::stepPassShaders` (`runtime/game.h`) lists the three pixel shaders; above `fps_limit` 30 `d3d/step_pacer.h` lets them run in one frame of the 30 FPS cadence (a step comes when 29 ms have passed since the last one) and the renderer drops the other frames' draws and their resolves, so the textures keep their last step (`SkipStepPass`, `d3d/render.cpp`). At a limit of 30 or below nothing changes. It applies to every liquid that uses these passes. `SKY_RENDER_STATS=1` ends its line with the passes run and skipped since start-up. Test: `ArkeyanStepPacerTest` (30, 60, 120, no limit, a load, the automatic drop to 30).
- **Proof** [V] (tutorial, offscreen, `--vsync 0`, `fps_auto_fallback=0`, `SKY_RENDER_STATS=1`, passes counted by three a step): at limit 30 (30.0 fps) 60 steps run in each 60 frames, none skipped, as before; at limit 60 (58-60 fps over 100 s) 30 run and 30 skipped in each 60 frames: 30 steps a second at both rates. Without the fix the same draws run in every frame, 60 a second at 60. On a loaded machine (15 fps) every frame runs, as at 30. The ripple's speed on screen was not compared (no wading or rain in the runs) [?].

## Spy Rise's metal parts are near-black in the tutorial (2026-11-01) [TrapTeam]
- **Seen** [V]: Spy Rise (2011 + 1011, `level_313_tutorial`, scale 2) draws its head, hands and claws near-black and its torso purple. These parts are pure metal: the G-buffer holds the gold albedo 191,131,80 with metal = 1 and gloss 0.31 (`83328c19` writes `tex2.b` to RT2.x). Rubble Rouser's and Boom Jet's dark parts have metal 0.8 to 0.85: the same term.
- **The term** [V]: one stencil-masked light draw (`ps 22d38236`) writes the pixel; the CPU evaluation of its translated HLSL, fed with every input traced at the pixel, matches the GPU on non-metal pixels to within a few levels. With metal = 1 the diffuse is 0 and the colour is `tonemap(1.5 * F0 * ramp6(0.5 + 0.5 n.y)^2 * cube7(R).x * AO)`, plus the sun's specular only where N.L > 0 (this pixel faces away: N.L = -0.48). F0 is the albedo squared (0.56, 0.26, 0.10). `cube7` is the level's colour environment cube (the sky up, the ground down) and the microcode reads its **x (red) channel** only: about 0.08 in the blue sky, so the lit value is 0.016 instead of about 0.08 and the tonemap gives 22,12,4. Setting `cube.x` to 0.08 in the evaluation gives 24,13,5.
- **Not the scale** [V] (2026-11-02): the same idle frame at scale 1 and 2 gives the same body box (dark share 0.251 and 0.253, mean luma 96.5 and 96.2). Gusto's cutscene cannon stays bright because its parts are metal 0.52 to 0.74, not 1: some diffuse is left, and the later passes lift it (light pass 37-50, final image 122-126).
- **Faithful** [V]: the fetch constants have no gamma, signed or exponent field, an identity swizzle and DXT1; the cube faces decode as a coherent picture; the `cube` operands are canonical (`r0.zzxy`, `r0.yxzz`). The 55 cube-reading light shaders of Swap Force also read `.x` only; its tutorial cube is lava-orange, a high red, so the same figure looks lit there. No host defect explains the darkness: no fix. Not compared with a console: only a capture from one would show a difference.
- **With the explicit LOD and cube fixes** [V] (2026-11-02; `setTextureLod`/`useRegLod` and the cube operand swizzles, in `patches/xenosrecomp.patch`): the metal = 1 pixels of Spy Rise in the same tutorial frame at scale 2 go from lit 33,16,9 / final 24,11,6 to lit 35,17,11 / final 26,12,8 (about 25 000 pixels, albedo 174,115,65). The cube is now sampled at the guest LOD `6 * (1 - gloss)` (4.1) and the right direction, and the gold stays dark: the only environment input of a metal = 1 surface facing away from the sun is `cube.x`, the red channel of the level's environment cube, about 0.08 to 0.10 wherever it shows the blue sky, whatever the LOD. No host input is missing.
- **The cube's channel** [V] (2026-11-02): at the light draw the cube's fetch constant is DXT1 (format 18), endian 8-in-16, base 0x0D23B000, 256x256x6, unsigned, swizzle 0x688 (XYZW) with no exponent adjust; the host makes a BC1 cube with the identity mapping and its faces decode as a coherent picture of the level. The microcode writes `r6.___x` and the HLSL reads `.x`: the same component. Forcing that cube's channel order for the run, metal = 1 pixels of Spy Rise (about 25 000, same frame): RGBA lit 67,41,25 / final 54,28,16; BGRA 67,44,31 / 54,33,23; ARGB (x = alpha, 1 for an opaque DXT1 block) a plausible gold, as on the portrait. No order rule explains it: x = alpha for DXT1 would also change Swap Force, whose lava-orange cube read through the same red channel is what lights its metals. Not fixed. What remains: the console's value of this fetch, from a capture of the real game (whether the cube at 0x0D23B000 holds other data there, or the game binds another cube for this draw).
- **How to look**: `XenosRecomp <one .ps> out.hlsl <shader_common.h>` writes the translated HLSL of one shader; `SKY_DUMP_DRAWS` dumps the target after each draw. The `[dump]   tex` lines print the fetch constant's dwords 3-5: the base address (dword 1) is not there, and the last field is the mip address (0 for a texture whose base is its packed tail).

## A dark one-pixel fringe on the left and top edges of every model at scale 2 (2026-11-02) [TrapTeam]
- **Seen** [V]: in the intro cutscene (the close-ups, where the sky is behind the models) every silhouette has a dark fringe on its left and top sides at resolution scale 2. Edge smoothing Light turns it into the owner's checker; Off leaves a crisp dark line. At scale 1 the same frame has none. Gameplay frames hardly show it: the models there seldom stand against the sky.
- **Measured** [V] (the G-buffer albedo resolve against the first lit resolve of the same frame, outermost geometry pixel, lit/albedo median): scale 2 left 0.63, top 0.67, right 0.99, bottom 0.58, interior 1.43; scale 1 left 0.88, top 1.33, right 1.11, bottom 0.55, interior 1.44. Only the left and top edges drop, and only at scale 2.
- **Cause** [V]: the light passes read the G-buffer with denormalized fetches at the pixel position, `r = iPos / s - 0.5` in guest pixels, then sample host texel coordinate `r * s = h + 0.5 - s / 2`. At scale 1 that is the left edge of the pixel's own texel `h`, which the point fetch resolves to `h`, as on the console. At scale 2 it is the centre of texel `h - 1`. Odd host pixels read inside their own guest pixel; even ones read the previous guest pixel. At a left or top silhouette the outermost geometry pixel then reads the empty texel beside it (no albedo, no normal) and lights dark, in a two-pixel period: the checker.
- **Fix** [V] (`patches/xenosrecomp.patch`, `tfetch2DDenorm` on Metal and HLSL, and the explicit-LOD `tfetch2DLodDenorm`): sample `(texCoord + offset) * scale + 0.5 * (scale - 1)`. That reads texel `h` at any scale and is exactly `(texCoord + offset) * scale` at scale 1 (+ 0.0), so scale 1 is unchanged by construction. Trap Team at scale 2 after the fix: left 0.87, top 1.24, right 1.11, bottom 0.58, interior 1.43, the scale-1 profile; scale 1 after: 0.86, 1.32, 1.09, 0.55, 1.43. The lit image now aligns with the G-buffer at (0, 0) (edge strength 54.5 against 38.8 for the next shift; before, (0, 0) and (1, 1) tied at 50.5 and 49.0, half the pixels shifted). Swap Force had it too, in a whole pixel: its lit image aligned at (1, 1) before (33.2 against 22.7) and at (0, 0) after (43.4 against 30.4). 32 Trap Team, 31 Swap Force and 6 SSA shaders use a denormalized fetch, Giants none; the SSA and Giants level pictures at scale 2 are unchanged. `tools/shaders/translation_test.cpp` checks that all four denormalized helpers carry the term.


## Public background (2026-10-25)

These source notes predate the port. They preserve release, hardware and toy references;
engine, import and runtime findings above take precedence. Confidence: [2] two sources, [1] one, [inf] inferred.

### Release
- Oct 2 2014 (AU), Oct 5 (NA), Oct 10 (EU): 3DS, PS3, PS4, Wii, Wii U, **Xbox 360**, **Xbox One**; mobile later. [1] ([W-TT])
- Developer: **Toys for Bob and Beenox** (mobile: Vicarious Visions). [1] ([W-TT])
- Xbox 360 version exists. The 3DS version is different (no traps in its pack). No Xbox 360 reduction found. [1]
- **Title ID `41560911`** (European disc). [1] ([TTR]). Media ID, TUs, DLC, disc size: **not found**.

### Engine
- Wikipedia: Alchemy [1] ([W-TT]) Giants, also Toys for Bob, turned out Alchemy (our boot, 2026-10-25),
  so **Alchemy is likely** [inf]. **Decide on the disc.**
- Fan port is scaffolded from Giants/SSA/SwapForce ports. [1] ([TTR]) No middleware named.
- Changes: villain capture, Traptanium portal speaker, Light/Dark elements, Minis. No Kinect found.

### Portal hardware
- **Traptanium Portal**: NFC reader, **speaker**, a **ring lit from two sides** and a **trap slot with a white LED** that lights
  only while a toy is read. Works with every console game through SuperChargers on its platform family. [2] ([Wiki-Portal], [Kneppers])
- USB: HID, **VID 0x1430 PID 0x0150**, two interrupt endpoints, 32-byte commands, status polled ~50 Hz. [2] ([Kneppers], [Wilson-gist])
- Xbox 360/One variants: Xbox 360 `1430:1F17` (from Wilson gist, Xbox 360 portal in general); Xbox One GIP IDs **not found**.
- Protocol additions (documented on the Wii/PC Traptanium): `J` fade one ring side, `L` ring side colour or trap-slot
  brightness (no reply), `M` speaker on/off (reply) then raw **8 kHz 16-bit mono** audio streamed to endpoint 1 with byte pairs
  swapped; `C` base colour. [1] ([Wilson-gist]; wired Xbox 360 framing for these: not documented in the sources found)
- Software portal projects (Cemu, RPCS3, Dolphin) document the Xbox 360 frame format and the speaker format; the fan project
  `skylanders-portal` credits them (no code to copy). [1] ([SP])

### Toys
- **Traps** (store a villain as a playable character; where the villain data lives is not documented in the sources read),
  **Trap Masters** (18), **Minis** (16), core (18), reposed (5), Eon's Elite (8), Light/Dark. 46 or 47 trappable villains
  (the page disagrees with itself). [1] ([W-TT])
- Tag: MIFARE Classic 1K, variant nibble 3. Trap layout: **not found** (the SuperChargers trophy bitmap suggests one byte of
  captured-villain bits per sector [1] ([Kneppers]), an analogue only).
- Compat: Trap Team toys work in Trap Team and later. Traps work in SuperChargers; the Imaginators Switch edition drops
  them. [1] ([W-SC], [W-IMG])

### Saves and online
- Not found. Stored villains travel with the trap, so no online need. [inf]

### Port risks
- Xenia: nothing found in the trackers searched.
- Fan port: boot, menus, chapter 1 playable at 60 fps with an emulated portal; **traps and portal speaker still to do**;
  2 open issues. [1] ([TTR])
- Needs a portal audio path (8 kHz stream) and trap events: new to us.

### Sources
[W-TT] https://en.wikipedia.org/wiki/Skylanders:_Trap_Team · [TTR] https://github.com/tsapZ/TrapTeamRecomp ·
[SP] https://github.com/tsapZ/skylanders-portal · [W-SC] https://en.wikipedia.org/wiki/Skylanders:_SuperChargers ·
[W-IMG] https://en.wikipedia.org/wiki/Skylanders:_Imaginators · [Wiki-Portal] https://skylanderswiki.com/wiki/Portal_of_Power ·
[Wilson-gist] https://gist.github.com/skylandersNFC/5c9fb3debc11c1ea194ed6ddc9fb8faf ·
[Kneppers] https://marijnkneppers.dev/posts/reverse-engineering-skylanders-toys-to-life-mechanics/
