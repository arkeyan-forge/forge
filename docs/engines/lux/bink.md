# Bink (RAD): Bink 1 in SSA, Bink 2 in Giants

> **Purpose:** how the games use RAD Bink (Bink 1 in SSA, Bink 2 in Giants) and how we show the movies · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants]

Bink is an XDK-level library: the same model (the game decodes and draws with run-time shaders) holds in both engines. The SSA part is **[Lux]**; "Bink 2 in Giants" below is **[Alchemy]** **[Giants]** and is linked from [the Alchemy README](../alchemy/README.md).

Code: [`d3d/bink_display.cpp`](../../../runtime/d3d/bink_display.cpp), [`host/display.cpp`](../../../runtime/host/display.cpp). Class **[Lux]** use of **[Xbox]** Bink 1; addresses **[SSA]**.

- Bink lives in its own sections (`BINK` `0x829E7000`, `RADCODE` `0x829FB000`, `BINKCONS`, `RADCONSTD`, `BINKBSS`, `BINKDATA`, `RADDATA`, `.XBMOVIE`): `BinkDoFrame` `0x829EDCF8`, `BinkWait` `0x829EA3F0`, `BinkShouldSkip` `0x829EA388`, `BinkNextFrame` `0x829EDD00`,
  `RADTimerRead` `0x829EEF90` (millisecond timer on `GetTickCount` = `KeTimeStampBundle->TickCount`: **it must tick**).
- **The game draws the movie frames itself** with shaders **built at run time** (YUV->RGB); they are not in any archive: dump with `SKY_DUMP_SHADERS=<dir>` once and feed `build_cache.sh` (**without it Bink movies are not drawn by the GPU path**; D15).
  Frame planes live in the `0xE0000000` window (+0x1000 offset: planes were read 4 KB too low until honoured, D8).
- **Before the renderer existed** (and as a fallback when `IsGameRendering()` is false) `BinkDoFrame` is hooked: after the original, `ShowFrame` reads Bink's own frame buffers: `HBINK` `+0` width, `+4` height; it points at the `BINKFRAMEBUFFERS` the game registered
  (`BinkRegisterFrameBuffers`), found **inside HBINK by content** (a pointer whose `+4` equals the width and `+8` the height rounded up by < 64): `+20` FrameNum, `+24` the frame sets, each of **four BINKPLANEs (Y, cR, cB, A)**, each `{Allocate, Buffer (+4), BufferPitch (+8)}` (12 bytes), 48 bytes per set.
  Conversion is **BT.601 limited range** to RGBA, submitted to `SubmitDisplayFrame` (letterboxed by the present pass).
- Movie lengths (SSA, from the Bink headers): legal 4.95 s, `atvilogo` 10.38, `xpeclogo` 8.51, `tfb` 10.0, `sl_sc01` 62.3. Bink paces itself on its own timer: movies keep their speed at 60 fps (each frame presented twice). START/A/B skip behaviour in [boot_saves.md](boot_saves.md).
- **Multi-track audio**: tracks per language; the cutscene voice track is chosen by `sub_827F4400` (hooked; [languages.md](languages.md)). Bink audio is decoded by the game's Bink and goes through XAudio2/XMA like the rest.
- `Movie_Update` (`0x82852D90`) is the hook for `SKY_SKIP_MOVIES`.
- One unexplained fault once in ~25 runs during `tfb.bik` (D40).

## Bink 2 in Giants [Alchemy] [Giants] [V] (2026-10-26)

Giants plays **Bink 2** (`KB2e`, `.mov` files in `\movies\`, 1280x704, 30 fps; the Activision logo 67800/2257 =
30.04 fps) with the same Xbox model: the game's own Bink 2 decodes and **draws the frames on the GPU** with its 50
run-time shaders, which the kit recovers by a short run at install ([shaders.md](../../../games/giants/docs/shaders.md)).
Nothing else was needed: with those 50 in the shader cache (4443 shaders) the logos (`atvinewlogo_640`,
`tfb_logo_640x456`) and the story movies (`fmv01`..`fmv12`, 13 audio tracks in `fmv01`) are drawn, colours right.
- **Hook:** `BinkOpen` `sub_821550A0` (`games/giants/addresses.h`), in [`d3d/bink_display.cpp`](../../../runtime/d3d/bink_display.cpp):
  logs `[bink] open <name> WxH, N frames`; a movie made one frame long (HBINK `+8` Frames = 1) ends at once. That
  serves `SKY_SKIP_MOVIES=1` (every movie) and **`[video] skip_intro_movies=1`** (the game's start-up logos,
  `Adapter::introMovies`, logged `[movie] skipped <name>`; the story movies play). SSA keeps its `Movie_Update` hook; both
  games now take the logo list from their adapter (`game::IsIntroMovie`, `runtime/game.cpp`).
- **Pace and sync:** `SKY_BINK_LOG=1` (test aid) logs the last opened movie's frame number (HBINK `+12`) once a second.
  `fmv01`, offscreen, 90 s measured: 2708 frames in 90.27 s = **30.00 fps** (the header says 30/1); the audio capture
  (`SKY_AUDIO_DUMP`) holds 147.85 s for a 150 s run and is non-silent through the movie. Bink 2 paces the picture on its
  sound clock, so a picture at the header's rate with the sound consumed in real time means they stay in sync. A
  person's check of lip sync is still welcome.
- **Wide windows:** see [ultrawide.md](../../runtime/gpu/ultrawide.md) (movies keep 16:9 with bars).

## Empty pictures and failed opens (issue #26)

The runtime has no FFmpeg movie decoder: FFmpeg decodes XMA audio, while the translated game's Bink decodes movies.
Swap Force enables `BinkOpen` and `BinkClose`, and its movie pictures, including alpha video, pass through the guest
renderer. It does **not** enable `BinkDoFrame` or Lux's `Movie_Update`. There is no separate host treasure or
picture-in-picture video player. The ranked candidates are in [issue #26](https://github.com/mflr0/Arkeyan-Launcher/issues/26) and the merge that brought this change.

The host `BinkDoFrame` fallback runs only before `IsGameRendering()` becomes true. Its conversion now checks allocated
buffer ranges, frame indices, plane pitches and visible bytes before reading YUV. An all-zero visible Y/Cr/Cb image
would produce RGB `(0,135,0)` with the limited-range conversion. It now produces opaque black, logs the reason and
sets HBINK's frame count and current frame (`+8`, `+12`) to 1, the terminal state used by the movie skip. Missing
buffers, missing/truncated planes and invalid fallback dimensions take the same path. Row padding does not make an
empty picture valid; decoded black (`Y=16, Cr=Cb=128`) continues playing. Cached buffer offsets are revalidated when
a handle or its layout changes. This is an empty-buffer check, not a general decoder-error detector.

For games with `BinkOpen`, a successful handle with zero width, height or frame count is logged by movie name and
made terminal. Until the game closes that failed handle, the swap hook uses `gpu::Present(nullptr)`: opaque black
with the host overlay still drawn, even if the front-buffer texture is missing. The failure is cleared on that
handle's close. Closing another movie does not clear it. `SKY_BINK_LOG=1` stops reading a handle before it is freed.

A null `BinkOpen` result logs `[bink] open failed <name> (no handle)`, supplies black to the host fallback display and
returns the original failure to the guest. No handle is fabricated and no guest completion flag is guessed. Whether
Swap Force's caller finishes a pickup on that null result **cannot be established here**: the workshop lacks its
translated movie functions. Likewise, decode failure after a valid GPU-path open is not observable through the two
enabled hooks. Neither case is claimed fixed for issue #26; they need the caller/decoder code or a runtime log.

`ArkeyanBinkFrameTest` uses synthetic planes to check black replacement, terminal state, chroma order, decoded black,
odd dimensions, padding and truncated storage. `ArkeyanBinkDisplayTest` calls the real hooks with synthetic guest
memory and stub originals, checking failed opens, zero headers, fallback failures, lifetime cleanup and the GPU-path
bypass. Both are registered with `sky_add_tests`; neither opens a movie or starts a game.


## Movie data that stops arriving (issue #26)

2026-11-12 [SwapForce] [Alchemy] [V]. **What players saw.** Swap Force, Mount Cloudbreak (`tutorial/tutorial_b`), Windows, Direct3D 12, scale 4, HD textures,
60 FPS: the whole picture plain green (RGB about 0,155,0), the music going on, the game deaf to everything but the port's
own menu, once on picking up an item by the honey cave, once on a bridge. The disc's `Soul_Gem_*_Mix_360.bk2` previews
(1024x608, 359 frames for `Soul_Gem_BeeTree`) play full screen over the level when a Soul Gem is picked up; the movie is
the likely event [?] (not reached in a scripted run: no route to those spots).

**The mechanism, reproduced** [V]. One read of a movie file that returns nothing makes exactly that picture: with the
11th read of `Tutorial_B_Intro.bk2` (128 KB at 917504) failing, Bink 2's guest decoder draws all-zero planes from then
on (RGB 0,135,0 on every frame, 84 of 121 frames to the end of the 120 s run) and the game waits for the movie forever.
`NtReadFile` returned 0 bytes with `STATUS_END_OF_FILE` and nothing in the log.

**Why a read could fail on a player's PC.** Two ways, both silent before: the write watch protecting a page of the read's
buffer while the read ran ([renderer_threads.md](../../runtime/gpu/renderer_threads.md#the-write-watch-2026-11-04-ssa-giants-swapforce-trapteam-lux-alchemy-xbox-v),
`ERROR_NOACCESS`), and Windows refusing a big read or the commit of a guest page while memory is short
(`ERROR_NO_SYSTEM_RESOURCES`, `ERROR_WORKING_SET_QUOTA`, the commit limit): scale 4 (5120x2880 targets, 2.2 GB of
textures in the tutorial at scale 4 on the Mac) with HD textures is the heaviest setup. Which one the players met is
not known [?]: their logs were not sent. Two runs on the Mac at scale 4 with HD textures logged no failed read.

**The fix.** `NtReadFile` keeps its buffer writable for the whole read (`writewatch::HostWrite`), and the disc reader
(`DiscFileSystem::Read`) reads through `readfully::Read` (`runtime/read_fully.h`): a short read goes on, a failed one is
tried again up to 8 times, each piece half as big down to 64 KB, after 1 to 8 ms; any failure is logged
(`[file] read <name> ...: K failed attempt(s), system error E`). Checks: `ArkeyanReadFullyTest` (retries, halving, a
lasting failure), `ArkeyanWriteWatchTest` (the race fails a real read without `HostWrite`, not with it). Scripted runs
(`SKY_LEVEL=tutorial/tutorial_b`, intro movie on, scale 4, HD textures, 60 FPS, anisotropy 16, edge smoothing light):
`SKY_TEST_READ_FAIL=intro.bk2:11:1` logs `1 failed attempt(s) ... read in full on a retry` and the movie plays into the
level (0 green frames); `intro.bk2:11:9` (past the retries) gives the green picture again. A failure that lasts still
ends the movie's data; the log then says so.
