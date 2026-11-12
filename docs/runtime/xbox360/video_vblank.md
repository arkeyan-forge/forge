# Video: vertical blank, interrupts, fences, pacing

> **Purpose:** how do the vblank, the graphics interrupt, the null-GPU fences and frame pacing work? · **Status:** current (2026-11-03) · **Tags:** [Xbox] [Lux]

Code: [`kernel/video.cpp`](../../../runtime/kernel/video.cpp), [`d3d/null_gpu.cpp`](../../../runtime/d3d/null_gpu.cpp).
Class: **[Xbox]** (the console's behaviour), with **[Lux]** consumers marked.

## The graphics interrupt thread

`VdSetGraphicsInterruptCallback(callback, context)` stores the guest callback and **starts one host-driven thread** (the first time).
That thread plays the console's interrupt context: it delivers **every** graphics interrupt, in order, by calling
`callback(source, context)` with `r3 = source`, `r4 = context`.

| Source | Meaning | Who raises it |
|---|---|---|
| 0 | vertical blank | the thread itself, every **16 667 us** (60 Hz): writes **`1` to the VBlank status register `0x7FC86544`** first, then calls the callback. **D3D's handler ignores the vblank unless bit 0 of that register is set.** |
| 1 | command-processor interrupt | `QueueGraphicsInterrupt(1, prepare)`: `prepare` runs on that thread first (it writes what the GPU would have written), e.g. the interrupt info block |

**The blanks must be on time** **[Xbox] [V] (2026-10-26)**: the thread asks for the host's real-time scheduling
(`PlatformSetThreadTimingCritical`, macOS `THREAD_TIME_CONSTRAINT_POLICY`: a 16.7 ms period, 2 ms of work within 4 ms).
With the default policy macOS coalesces the timed wait: the blanks came **1.5-4.5 ms late (median), 7.5 ms (p95), 8.3 ms
(max)**, even on a quiet machine, so a 30 fps game showed frames 25-41 ms apart (Giants, Chapter 1: gap p99 40.5 ms with
9 ms of work per frame). With it: late 0.01 ms median, 0.02 ms max; gaps 32.9-33.7 ms (p99 33.4). A small test program
gave the same numbers, `nice` or not. `SKY_VBLANK_LOG=1` prints, every 600 blanks, `[vblank] late median/p95/max,
callback median/p95/max` (how late each blank was delivered, how long the guest callback took). Windows relies on
`timeBeginPeriod(1)` (`PlatformInit`), not measured [?]; Linux has no policy yet [?].

D3D's own handler is `D3D_GraphicsInterrupt` (`0x822A4000` [SSA]): source 1 calls `info+16(info+20)` (callback, context); source 0
handles the vblank. Both the NGL vblank callback (`0x82631968` [SSA], counter `0x82BB4FD8`) and shader/scene time count **vblanks**.

## Video mode

`VdQueryVideoMode` / `XGetVideoMode` write 0x30 bytes: `+0x00` width 1280, `+0x04` height 720, `+0x08` interlaced 0, `+0x0C` widescreen 1,
`+0x10` HD 1, `+0x14` refresh 60.0f, `+0x18` NTSC 1, `+0x1C` 0x8A, `+0x20` 0x01. All other `Vd*` calls are weak stubs
([kernel_exports.md](kernel_exports.md)).

## Fences without a GPU (`d3d/null_gpu.cpp`) [Xbox] (scaffold the renderer relies on)

The game's D3D still writes a command ring nobody executes, so **submitted work counts as executed the moment it is kicked off**:

- Device fields (D3D9LTCG 2.0.20500 layout, [gpu/d3d9ltcg_device.md](../gpu/d3d9ltcg_device.md)): `+0x2B10` pointer where the GPU writes the last
  retired fence, `+0x2B14` interrupt info block (callback at +16, context at +20), `+0x2B1C` current fence.
- `KickOff` (`0x822A3BB0`): `ScanRing` (it may wrap), run the original, `ResyncRing`, **retire all fences below the current one**
  (`*retiredPtr = current - 1`), raise the queued callbacks as source-1 interrupts. Inside a kick-off everything handed out is submitted.
- `BlockOnFence` (`0x822A3CB8`): if the fence is retired (`current - fence >= current - retired`) return; inside a kick-off retire; otherwise
  **kick off** (a fence in unsubmitted commands needs a kick-off; the saved context is restored). Unsubmitted work must **not** look
  done or the engine recycles frames still being built. While a command buffer is recorded there is no GPU fence.
- `InsertCallback` (`0x822A3DB0`): instead of writing packets that make the GPU fill the info block and raise a CP interrupt, queue
  `{device, callback, context}` and deliver after the next kick-off. Flags 0x2 = raise interrupt, default 0x6.

## Pacing (`WaitForFlip(presentInterval)` in the Swap hook)

`D3DDevice_Swap` -> `WaitForFlip(device+0x3630)` (D3DRS_PRESENTINTERVAL). Mapping (D3D's own `0x822A9088` [SSA]):
DEFAULT/ONE -> 1 blank, TWO -> 2, FOUR -> 4, IMMEDIATE (0x80000000) and THREE (sent as immediate) -> no wait.

- Target = `lastFlip + vblanks`, wait until `vblanks counter >= target`. **A frame that missed its blank is shown at once** (no scan-out
  to tear): before, `max(blanks + 1, lastFlip + 2)` turned 33 ms frames into 50 ms. Alternating A/B under load: worst 60-frame
  window 25.2 -> 29.0 fps.
- **The title (NGL) asks for interval 2** (`0x82631EA8` [SSA]): 30 fps on a 60 Hz blank. `fps_limit=60` (`SetFrameRateLimit(60)`) turns
  interval 2 into 1: 60 fps at the game's normal speed, because the engine steps by *measured* time
  ([lux/frame_pacing.md](../../engines/lux/frame_pacing.md)). **The emulated vblank must stay at 60 Hz** (scene and shader time count vblanks).
- Before pacing: 120-500 frames/s in the title because fences retire instantly; movies ran at 6-7 frames/s (D26).
- **Above 60** (`fps_limit` 120, 144, 240 or 0 = no limit; 2026-10-30, `FrameLimiter` in `kernel/frame_limiter.h`, unit test
  `ArkeyanFrameLimiterTest` with a fake clock, no sleep): `WaitForFlip` no longer waits for a blank. The guest's blanks stay 60 Hz in
  wall time (scene and shader time count them), and the frame is shown one period (1/fps) after the previous one: sleep until
  1.5 ms before, then spin. A frame more than a period late is shown at once and restarts the schedule (no burst to catch up).
  The title's interval is 1 as at 60 (`presentIntervalPacesGame` games get 1 written too). `SetFrameRateLimit` switches rates
  at run time. Offscreen has no display refresh and its stand-in vsync is 60 Hz: measure with `--vsync 0`. With vsync on, a
  window is held to the display's refresh, whatever the limit. The `HalfRate` fall back (below) is for 60 only.
### Above 60, measured
[V] 2026-10-30, macOS Metal, offscreen, `--vsync 0`, `tools/regress/framerate.sh`, 90-100 s runs. **The machine was shared with two other
builds and runs (load average 18-55): the rates below are what the machine gave, not what the limiter allows, and no row is a 60 or 120 FPS
proof.** What they show: the limit is not the ceiling any more (each game goes faster at 120 than at 60), and what the frame time
gives the game stays right.

| Game, window | limit 30 | limit 60 | limit 120 |
|---|---|---|---|
| SSA level_027, level (2000-2250 draws) | 17.7 fps, game 0.832 s/s | 28.5 fps, game 0.988 s/s | 57.5 fps, game 1.000 s/s |
| SSA, menus window (2550-2720 draws) | 21.5 fps | 25.2 fps | 31.6 fps |
| Giants, title and tutorial (169-189 draws) | 29.9 fps (gap p50 33.3) | 51.2 fps (16.7) | 91.9 fps (9.4) |
| Swap Force, tutorial window (310-327 draws) | 26.5 fps | 25.4 fps | 40.2 fps |

Game clock (SSA only, `SKY_CLOCK_STEP=82B62CA4`, Lux's F): 1.000 s/s at 120, 0.988 at 60; at 30 the machine gave 17.7 fps, under the
engine's 20 fps clamp, hence 0.832 (that is the 20 fps slow-down, not the rate). Swap Force: 1.00 game s per wall s at 30 and 60 by a probe on the engine's frame time (`games/swapforce/docs/boot.md`, "Frame rate", 2026-10-27). Giants and Swap Force at 120: by a cutscene, below. Draws a frame stay the same across the rates in each window (particles and scene are not rate-dependent in
these windows). Menus: the runs press through them at every rate (the tutorial / level was reached each time). Falls: Swap Force logged
the `[fall]` scale once at 60 and at 120 (`FrameScale()` 0.5 and 0.25); no player was put back in these windows, but no long fall was played [?].

**Game clock at 120, by an in-engine cutscene's wall-clock length** [V] (2026-10-30, `tools/regress/cutscene_clock.sh`, offscreen,
`--vsync 0`, no Bink movie and no press inside the cutscene; presses only before the level; two runs per rate, Giants has four at 30). A shot's
start and end are read on the frames' draw counts (`SKY_FRAME_TIMES`, median per half second: a shot change moves it, the frame rate does not) and
each boundary was checked on a dumped frame at 30 fps. Load average 3-9, the other lane running.

| Game, cutscene | Start event | End event | Seconds at 30 | Seconds at 120 | fps reached (at 120) |
|---|---|---|---|---|---|
| Giants, tutorial: Eon's narration over the Ark (shot A) | draws rise from ~300 to ~620 (the island) | draws fall to ~70 (the summon of the figure) | 11.2, 11.8, 11.3, 11.7 | 10.7, 11.2 | 108.0, 100.8 |
| Giants, the summon (shot B) | the fall to ~70 draws | draws rise to ~750 then ~575 (gameplay, HUD) | 6.1, 6.1, 6.1, 6.1 | 5.5, 5.5 | same runs |
| Swap Force, tutorial: the ship over the lava canyon, after the opening Bink movie (shot A) | draws rise from ~18 (movie) to ~340-400 | draws fall to ~70 (the flash that cuts to the player) | 13.8, 14.4 | 13.6, 13.7 | 53.9, 54.5 |
| Swap Force, the flash (shot B) | the fall to ~70 draws | draws rise to ~400 (the player, HUD) | 5.6, 5.1 | 5.7, 5.6 | same runs |

Verdict. **Swap Force: the clock is the same** (A + B 19.4 and 19.5 s at 30, 19.3 and 19.3 at 120; the 0.5 s spread at 30 is the noise), but
at "120" the game only reached 54 fps: the swap wait (`games/swapforce/swap_wait.cpp`) holds it near 54-60 on this machine, so this proves a
clock at 54 fps, not at 120 [?]. **Giants: the clock differs a little**: at 30 shot A is 11.2-11.8 s and shot B 6.1 s in all four runs; at ~105 fps
A is 10.7 and 11.2 s (about 5 % shorter, inside the 30 fps spread) and B is 5.5 s in both runs (0.6 s, 10 % shorter, every run on each side). A + B:
17.3-17.9 s at 30, 16.2 and 16.7 s at 120 (about 6 % shorter). **Giants is capped at 60 until this is fixed** (`Adapter::maxFpsLimit`, the launcher's list); not fixed; one reading: shot B's end follows something timed
by wall clock or by a fixed number of frames at the lower rate, not the clock. Not explained [?]. Three runs of the first batch were meant for 30 and 120 but all ran at 30 (a loop mistake: the log says `fps_limit=120 ... is not a number`); they are the four 30 fps runs of Giants.

**Not proven, for the owner's real display:** that 120 holds on a ProMotion or 120 Hz screen with vsync on (offscreen has no refresh and
its vsync stand-in is 60 Hz); that Swap Force really exceeds 60 (its swap wait is a blank callback, `games/swapforce/swap_wait.cpp`: the
40.2 fps at 120 against 25.4 at 60 says only that it is not held to the limit; a quiet machine must show 100+); and the evenness of the
frames on a real display. The launcher's "Display refresh rate" stays at 60 until those pass.

- **`FrameScale()`** (`kernel/video.h`) = 30 / `fps_limit`, at most 1 (0 counts as 240): the factor for a per-frame step tuned for
  30 FPS. Users: Swap Force's stuck-falling threshold (`games/swapforce/fall_check.cpp`, [SwapForce]). Found nowhere else so far:
  the engines step by measured time ([Lux] F, [Alchemy] the interval); a new fixed step found at a rate above 30 goes in this list.
  `SKY_CLOCK_STEP=<hex address>` (test aid) sums a float step in 60ths of a second per flip and logs `[clock]` every 5 s.
  `tools/regress/framerate.sh <game> <disc> <fps>` runs one game at one rate and prints the table row.
- **`fps_limit=60` falls back to 30 while the frames do not fit in a blank** [Xbox] (2026-10-28; `FrameRateFallback` in
  `host/settings_fallback.h`, called by `kernel/video.cpp`). Without it, a game at ~45 fps judders: a late frame is shown at once, so the game runs at a steady
  ~22 ms a frame and steps its scene by that much, while the display's vsync shows each frame for 1 or 2 refreshes (16.7 or
  33.3 ms). Equal steps shown for unequal times: on fast scenes the picture looks epileptic (the user's Swap Force recording
  of the tutorial, 2026-10-28: 44-48 distinct frames a second, 306 frames shown once, 128 twice, 11 three times in 10 s, and
  the scene's change from one frame to the next the same for all three, correlation 0.0). Rule (2026-11-11, 0.9.6): frames are counted in blocks of 60 (a second); when 5 blocks in a row each had a quarter of
  their frames late or more (late: the blank they were due at had gone by when they reached the swap; under ~50 fps for
  5 s), frames are shown every 2 blanks; the device interval stays unchanged (2 for Lux, 1 for engines that read it to
  pace themselves), so the engines measure 33.3 ms and show 33.3 ms. The rate then stays at 30 for at least 900 frames
  (30 s), and comes back to 60 once 90 % of the last 300 frames (10 s) had their work done within 3/4 of a blank
  (12.5 ms from the end of the previous wait: the margin 60 needs; a game that waits for a blank in its own code,
  `presentIntervalPacesGame` (Swap Force, Trap Team, SuperChargers, Imaginators), always arrives just after one, so for
  it a frame fits when it is not late). A drop within 3600 frames (60 s) of coming back
  doubles the hold, up to 3600 frames (2 min). Frames during a load (the 5 s after a file open: `LastFileOpenNs`,
  `kernel/files.cpp`) or a movie (`ultrawide::MoviePlaying`) are not counted and never switch the rate; after them the
  counts start again and the rate stays. Why (2026-11-10): the 2026-11-03 rule (20 late frames in 120, back after 300
  fitting frames at 30, and at once after a load) dropped on a 2 s burst of a busy scene and on a load's hitch, then came
  back within seconds: on a Windows PC (Ryzen 5 5500, RTX 3050) the log showed drop / back / drop within seconds, each a
  visible stutter, and the player found the game best with the switch off. Replayed on the owner's two 10 s Trap Team
  recordings, the old rule dropped in both (one frame in six late over 2 s); the new one in neither, even with them
  repeated for a minute. Before that, until 2026-11-03, it was 3 of 30 ("Why the games capped at 30" below).
  The test is in blanks,
  not host time, because Swap Force at interval 1 waits for a blank in its own code before it swaps (its frames at 30
  then measure 16.6 ms of "work" whatever they cost); the margin back to 60 is host time from the end of the wait. The log
  says each change (first 20): `[video] fps limit 60: a quarter of the frames miss their 16.7 ms blank for 5 s: drop to
  30 FPS until they fit again (held 30 s at least)` / `... back to 60 FPS`. Limit 30 is untouched.
  The switch is `[<game>.video] fps_auto_fallback` beside `fps_limit`. **Default (0.9.6): from the GPU** for a player who
  never set it: on for integrated, virtual and Apple GPUs, off for discrete ones (`gpu::DropTo30ByDefault`, the auto
  scale's cases); the game logs `[video] drop to 30 FPS when needed: off by default (discrete GPU)` and writes the value to
  its section once, so the launcher's page and the F3 row show it (the launcher shows on on macOS, off elsewhere until
  then). Before 0.9.6 it was on for everyone; a value the player set stays.
  `0` disables this decision and its toast; the chosen limit stays unchanged even when frames stutter.
  Changing the limit or disabling the switch clears the counts.
  `ArkeyanFrameRateFallbackTest` (`host/settings_fallback_test.cpp`) replays the two recordings (late frames as a
  string) and checks a load's hitch, a 20 s load, a sustained overload (a frame in 2 or 4 late: 30 after 5 s; one in 5:
  never), the 30 s hold, the 90 % margin, the doubling to 2 min and its reset, a movie at 30 and disabling. [V] on this
  Mac (SSA, `SKY_FPS_TEST_LATE_MS=40`, offscreen): unset gives `on by default (Apple GPU)` and `fps_auto_fallback=1` in the
  file, then one drop; set to 0, none. On the Windows PC [?]: [test_096_pacing.md](../../release/test_096_pacing.md).
  **Check it** [V] (headless): the tutorial at `--fps-limit 60 --scale 2`, `SKY_WINDOW_SIZE=1392x812` (the user's window),
  `SKY_FRAME_TIMES=<file>` on a loaded M-series Mac: present intervals 40-100 s before: median 38.9 ms, std 7.2 ms, 22 % in
  28-36 ms; after: median 33.3 ms, std 3.9 ms, 90 % in 28-36 ms. On the player's display (vsync) [?]: to be confirmed by
  the player; a machine that holds 60 keeps 60.
  **The player is told once per run** (2026-10-28, `ToyboxNotice` in `host/toybox.h`): at the first fall back the Toy Box's
  toast (top of the picture, 5 s, English or French by the game's text language) says "30 FPS: 60 not held" / "30 FPS : 60
  non tenu"; the later changes only go to the log. Nothing is shown when the Toy Box is unavailable (no window, fonts
  missing). The user's decision: on a machine that cannot hold 60, a steady 30 FPS, back to 60 when it holds.
  **Measured** [V] (2026-10-28, macOS Metal, offscreen, `--fps-limit 60`, 30 s in a level, load average 5-6): Giants'
  tutorial at scale 1 holds 60.0 fps (median gap 16.7 ms, p99 17.1, no fall back); Swap Force's tutorial at scale 1 ends on
  six `[perf]` lines of 60.0 fps; SSA's `level_027` (3734 draws, work 17 ms a frame) cannot: it falls back and shows a
  steady 31.3 fps (median gap 33.3 ms, p95 33.4) with the message. At load 20-40 all three fall back to a steady 30.

## Why the games capped at 30 (2026-11-03) [Xbox] [V]

The owner: "it caps at 30 easily". On the M1 Pro at the default 60 (scale 2, fallback on, a shared machine), the policy
of 2026-10-28 kept SSA `level_027`, Swap Force `brn2` and `ww1` and Trap Team's hub at 30-37 fps in a 30 s window of a
level (2-5 drops in 100 s each), while the same levels **hold 59.3-60.0 fps with the switch off**. Two faults
(`host/settings_fallback.h`): 3 late frames in 30 (a single burst of slow frames) halved the rate; and at 30 a single
frame later than one blank after the flip restarted the 300-frame count back to 60, which SSA's streaming hitches (1-3 a
second at 30, work over 33 ms) did all the time, so the game never came back. The rule above replaces it; with it, the
same runs show 57.3-59.9 fps in every level that holds 60, and 30 where the machine does not (Trap Team's tutorial,
GPU-bound at 53 fps; SSA `level_008`'s heaviest view, 41-50 fps). Numbers and commands:
[renderer_threads.md](../gpu/renderer_threads.md), "Where the frame goes, per game". `ArkeyanSettingsTest` covers bursts,
spread misses, the 1-in-20 tolerance, doubling to 1800 and disabling.

## Automatic-drop audit (2026-11-01)

The only runtime decision that changes 60 into 30 is the shared fallback above. It was already reachable in all four games:

| Game | Route to the decision at 60 | Timing safety |
| --- | --- | --- |
| Spyro's Adventure | Lux requests interval 2; `WaitForFlip` changes `vblanks` from 2 to 1 **before** testing the fallback | Lux measures its variable step; its vblank clock remains 60 Hz |
| Giants | Adapter `presentsAtEveryBlank=true`, `presentIntervalPacesGame=false`; interval 1 reaches the fallback directly (an interval 2 is lifted too) | Measured frame time, with the existing 60 cap |
| Swap Force | `presentIntervalPacesGame=true`; `D3DDevice_Swap` writes interval 1 into a device that still has 2 before the flip | Interval 1 selects measured time rather than the fixed 1/30 step |
| Trap Team | Same interval rewrite and shared fallback as Swap Force | Same existing measured-time route, with a 60 cap |

Other ways a presented rate can be lower: explicit limit 30; the game's requested interval 2 or 4 outside the
60 override; work that takes longer than the chosen period (late frames are presented immediately); display VSync
holding the presentation to refresh. Immediate intervals bypass vblank pacing and the fallback. Above 60 or unlimited,
`FrameLimiter` paces on host time and bypasses this policy. None is another automatic 60-to-30 decision.
Static caps remain unchanged: `maxFpsLimit=60` for Giants and Trap Team, none for Spyro's Adventure or Swap Force.
Giants' measured clock limitations above 60 are documented above; the launcher also caps Giants at 60.
No game flag or game patch is needed for this switch.

The GPU performance hint (`gpu/gpu.cpp`, `slowWindows`) only logs advice after sustained GPU pressure; it does not
change the limit or resolution. Swap Force's guest swap-status wait (`games/swapforce/swap_wait.cpp`) waits for a
pending swap's interrupt, not an adaptive 30-FPS choice. Movie playback follows the movie clock and may repeat
frames; it does not write `g_fpsLimit`. These paths remain independent of the switch.

Validation of the switch (2026-11-01): all four runtime targets built with prebuilt PPC libraries; each
`ArkeyanSettingsTest` passed 28014 checks. The launcher page saved `fps_auto_fallback=0` while keeping 60 for all four
games, in English and French, and its no-disc suite passed. The runtime quick harness passed every check except
`ArkeyanRecompGpuTest` in each of its three game builds (exit 139); an independent Metal probe exposed no default
device and zero enumerated devices in this execution environment. Rendered forced-late on/off game runs and the
launcher-file-to-running-game proof remain unverified. The game lock was still held while these checks were queued.
The F3 backend setter is ready; its row is not applied because the UI files are outside this lane's file fence.
