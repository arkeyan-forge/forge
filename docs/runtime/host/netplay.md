# Online play (0.10.0): netplay in input lockstep

> **Purpose:** how two players, each on his own computer with his own portal and Skylanders, play one game together online, and what the runtime must change to make that possible? · **Status:** design; step 0a probe implemented, game validation pending · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam] [SuperChargers] [Imaginators]

The owner's decision of 2026-11-04 (free exploration) stays the long-term goal; 0.10.0 is its first step, with
the game's shared camera.

## 1. What the owner asked (2026-11-08)
Online play run by the launcher ("play with a friend"): create a room or join one; the host gets a port and gives his IP
and port to his friend. Each player has his own portal or Toy Box and his own Skylanders, and sees the other's profile
(pseudo, picture). Each game runs on each player's own PC. Netplay, not streaming. An **Online** button on every game's
page in the launcher and in the F3 menu. Target 0.10.0.

## 2. The model: input lockstep
Both PCs run the same game from the same start, and exchange only their inputs. Frame N runs on both PCs with the same
inputs: P1's pad and portal from the host, P2's from the guest. If the game is deterministic (same inputs, same results),
the two games stay identical frame after frame, and each player sees the game's own two-player co-op, with the game's
shared camera. No game code knows it is online: to the game, P2 is a second pad and a second figure on its portal.

What crosses the network, per real frame (30 or 60 a second): a frame number, the pad state of the local player (about 16
bytes), the portal events of that frame (a figure placed or taken off, as the portal layer already sees them, with the
figure's data the first time), and every N frames a hash of the game state. Tens of kilobytes a second at most.

What each player keeps: his own disc (both must be the same game, region and version: the default.xex SHA-256 matches,
checked at join), his own figures (their data goes to the other PC at the start, so both games see both figures; each
player's progress is written back to his own Collection only), his own profile (passed to both games as P1 and P2:
`SKY_PLAYER_*`, `SKY_PLAYER2_*`, `docs/runtime/host/profiles.md`), his own picture settings (scale, FSR, frame
generation, window: rendering only, see risks).

What must be equal on both PCs (the host's values are sent and forced on the guest for the session): the game's save at
the start (the host's slot is sent; the guest's own saves are not touched), the frame-rate limit (30 or 60: it changes the
game's step), the game language and voices (they change the files loaded), the aspect (16:9 forced: Hor+ widens what the
game culls), cheats and debug switches, the figures' data, the game version.

## 3. Determinism: the hard part (to be proven first, per game)
The ported games run the console code natively, on many host threads (31 to 103 guest threads per game: job-queue
workers, Havok workers, loaders, Bink, audio), with wall-clock time. That is not deterministic today. A **netplay mode**
of the runtime must make it so; each point below is a known source of divergence and its remedy.

| Source | Today | Netplay mode |
|---|---|---|
| Time | `mftb` is translated to the host counter (`__rdtsc()`, XenonRecomp); `KeQuerySystemTime`, `KeQueryPerformance*` from host clocks | virtual time from the frame number and a fixed step: XenonRecomp emits `mftb` as a read of a runtime counter (a patch: every player's game is rebuilt, as on any game update) |
| Thread scheduling | each guest thread is a host thread, run in parallel | a deterministic guest scheduler: one guest thread runs at a time, switched only at kernel calls (waits, signals, sleeps, yields, thread start and end) in a fixed order; host parallelism stays for the GPU, audio mixing and file reads |
| Interrupts and callbacks | VBlank and GPU interrupts from a wall-clock thread; the audio driver calls the game's render callback from its own thread at the device's pace | delivered by the scheduler at fixed points of the frame: one VBlank per frame, the audio callbacks a fixed number per frame (fractional accumulator), the GPU's swap and fence interrupts after the frame's commands |
| GPU results the game reads | fences, ring read pointer, resolves into guest memory, written by the host GPU thread when done | the guest sees the GPU done at fixed points (its waits complete deterministically); GPU-written memory (resolve targets) is left out of the state hash (its bytes differ between Metal, D3D12 and Vulkan); a per-game audit that the game logic never reads it |
| File reads | synchronous (`kernel/files.cpp`) | already deterministic; loading time differs per PC, the lockstep waits for the slower one |
| Randomness | game RNGs seeded from time; `<random>` in the host's XNet stubs | follows from virtual time; host randomness seeded from the session seed |
| Floating point | native, same code on both PCs; x64 and arm64 differ in fused multiply-add and some library math | the same build flags on every platform (no FMA contraction in the game code: a recompiler option), or Mac-Mac and PC-PC only at first (to measure) |
| Host-side game changes | the Toy Box (F6), the Toy Box window, cheats change guest state outside the frame | go through the input stream: applied at a frame number on both PCs |

**Cost**: one guest thread at a time means the guest's CPU work runs on one host core. The consoles' three cores did it
in 33 ms; one Apple core is several times faster than one Xenon core, so 30 FPS is likely, 60 is to measure. This is the
first thing measured (step 0).

**Spin waits**: a guest thread that loops on a memory flag without a kernel call would never yield in a cooperative
scheduler. XenonRecomp can mark the console's spin hints (`db16cyc`, `or r31,r31,r31` thread-priority hints, `lwarx`
loops) as yield points; the step-0 probe finds any loop that hangs.

## 4. Desyncs, input delay, rollback
- **Desync detection**: every 30 frames, each PC hashes the game's state (the guest heaps, without GPU-written memory and
  audio buffers; a fast hash, under a millisecond) and sends it; a mismatch stops the session at once with a clear
  message to both players and, with a debug switch, a dump of the frame's state on both sides for a diff (the tool that
  finds a desync's cause).
- **Input delay**: inputs are sent for frame N + D; D = 2 frames at 30 FPS (67 ms) by default, chosen at the start from
  the measured round trip (D = ceil(RTT / 2 / frame) + 1), shown in the lobby. A co-op platformer with a shared camera
  hides 60-100 ms well.
- **Waiting**: a PC that has not received the other's input for frame N waits (the picture holds; a "waiting for <pseudo>"
  notice after 1 s; the session ends after 15 s).
- **Rollback: not in 0.10.0.** It needs savestates of the whole game (guest memory plus every host object: threads, kernel
  objects, GPU and audio state) taken and restored in a few milliseconds; that is a project of its own, after lockstep
  works. Joining a game already running needs the same savestates: in 0.10.0 a session always starts from the game's
  launch (the host's save loaded on both PCs).

## 5. Network
- One UDP port on the host (default 47640, configurable), our own small protocol, versioned like the Toy Box link: hello
  (protocol version, game id, xex SHA-256, runtime version), room password check, profiles, settings, save, figures,
  then frames. Each packet carries the last several frames' inputs (loss is covered without resend); a sequence number
  and the session key (from the room password) authenticate packets; anything malformed is dropped and counted.
- **NAT**: the host's port must be reachable. In order: IPv6 when both have it (no NAT); UPnP IGD port mapping (SSDP and
  SOAP, written by us; also gives the router's external IP for the IP:port to share); NAT-PMP/PCP; otherwise the host is
  told to open the port on his router (a help page). No relay server, no matchmaking server (the owner's "no server").
- The public IP shown: from UPnP's GetExternalIPAddress when the router answers; else the local IP with a note "your
  public IP: ask your router or search 'what is my IP'" (no outside service is called by default).
- The game process does the networking (it must exchange inputs every frame); the launcher sets up the room and hands the
  session to the game on start, through the existing loopback link (`docs/runtime/host/toybox_link.md`).

## 6. UX (launcher and F3)
- Each game's page: an **Online** button beside PLAY. It opens the Online panel: **Host a room** or **Join a room**.
- Host: the port (default, editable), "Open the port automatically (UPnP)" (on by default), an optional password; then
  the room shows `IP:port` with a Copy button, the router's answer (port open / not open, with the fix), and waits.
- Join: an `IP:port` field (pasted), the password if any; Connect.
- Cross-platform: by default a Mac joins only a Mac and a PC only a PC (the hello carries the platform; a mismatch is
  refused with a clear message). The launcher's settings have **Allow cross-platform sessions (not tested)**, off by
  default; both players must turn it on, and the lobby shows a warning while it is used.
- The lobby (both players): both profiles (pseudo, picture, colour), each player's figure(s) from his own Collection or
  real portal, the game and version check, the round trip and the input delay, the host's settings that will apply, a
  Ready button each; the host starts. Both games start; the launcher shows "Online with <pseudo>" while it runs.
- F3 in game: an **Online** row: the other player's pseudo, ping, delay, state hashes OK, **Leave the session**. Starting a
  session from F3 restarts the game into the lobby (a session starts from launch).
- Every text in the launcher's ten languages; pad, keyboard and mouse navigation, as the rest of the launcher.

## 7. Risks
| Risk | Effect | What we do |
|---|---|---|
| A game is not deterministic even with the scheduler (a spin loop, a host callback we missed, Havok's threads) | desyncs in that game | step 0 measures it per game with two local runs; a game ships online only with zero desync over a full scripted route |
| One-core guest too slow at 60 FPS | online at 30 only | measured in step 0; 30 FPS is the games' native rate |
| Cross-platform floating point (Mac arm64 vs PC x64) | desync between a Mac and a PC | Mac-Mac and PC-PC by default; a launcher option allows cross-platform, marked not tested; step 2 measures Mac-PC (recompiler flags if it diverges) |
| GPU readbacks used by game logic | desync between GPU vendors | per-game audit of resolve reads (step 2) |
| Both players must have the same disc version and language | no session between two regions | checked at join, clear message |
| NAT without UPnP | the host cannot be reached | help page, IPv6, port forward; no server by design |
| An untrusted peer | crashes or worse through malformed packets | strict parsing, bounds, fuzz tests of the protocol, password, nothing executed from the peer, figure data validated as a dump file is |

## 8. Plan to 0.10.0 (each step ends with its proof)
0. **Determinism probe** (the go/no-go): netplay mode's virtual time and guest scheduler behind `SKY_NETPLAY_DETERMINISTIC`;
   record a route's inputs, replay it twice on one Mac, hash the state every frame. Per game: first diverging frame,
   cause, fixed, until two replays match for 10 minutes. Also the one-core frame time at 30 and 60.
1. **Lockstep core on one machine**: two game instances on one Mac over loopback (one as P1, one as P2), input delay,
   waiting, desync detection and the diff tool. Unit tests of the protocol and the scheduler.
2. **Two machines**: Mac-Mac over LAN, then Mac-PC (Wine first, a real PC by the tester); floating point and GPU audit.
   Mac-PC stays behind the launcher's cross-platform option (off by default, "not tested") until it is proven.
3. **Session setup**: settings forced, save transfer, figures from each player's portal or Toy Box, profiles as P1/P2,
   write-back of each player's figure progress.
4. **Launcher**: Online button, host/join, IP:port, UPnP/NAT-PMP, lobby, ten languages; F3 Online row.
5. **Per game**: the six games through their scripted routes online (loopback and LAN), zero desync; any game that
   desyncs ships with Online greyed and its reason; docs; the tester's checklist (two real PCs, two homes, NAT).
6. Release 0.10.0 after the owner's own test with a friend.

Rollback, joining a running game and more than two players: after 0.10.0.

## Step 0a: local determinism probe [Xbox]

[V] The runtime tools are behind **exactly** `SKY_NETPLAY_DETERMINISTIC=1`. Unset, or another value: native
clocks and live pad reads; no probe files, frame buffers or frame logs. This step supplies virtual time, frame-latched
pads, pad recording/replay and allocation hashes. [?] Two replays are still expected to diverge: guest threads run
freely, VBlank/audio/GPU callbacks still follow host scheduling, and portals are not recorded. This is a diagnostic
probe, not online play or proof of determinism. The cooperative guest scheduler and fixed-point delivery of interrupts
are the next step; the remaining sources in section 3 still need an audit.

[V] Each `WaitForFlip` starts the next numbered frame (first: 1), advances by the guest's present interval / 60 seconds
(0 and immediate count as 1), updates the time-stamp bundle and latches every XAM user. System time starts at
2026-01-01 00:00 UTC (FILETIME); `mftb` and the reported performance frequency use 49.875 MHz. Each host thread has
its own per-read tick, reset on observing a new frame; reads remain monotonic through long busy waits. Rebuild the
translated game code with the patched XenonRecomp so `mftb` calls `PPC_MFTB()` through the runtime.

| Variable | Effect (requires deterministic mode) |
|---|---|
| `SKY_NETPLAY_RECORD=<file>` | writes all users' latched pads and connection flags; packet numbers come from changes, not polling rate |
| `SKY_NETPLAY_REPLAY=<file>` | replaces live pads, SDL and every `SKY_PRESS*` source; mutually exclusive with recording; wrong game or malformed data refuses startup at the first frame |
| `SKY_NETPLAY_HASH=<file>` | one `<frame> <64-bit hex total>` line per frame; finalization logs count and mean/max milliseconds (exit integration below) |
| `SKY_NETPLAY_HASH_BLOCKS=1` | also writes `<file>.blocks`, with address, heap kind and hash for each block |
| `SKY_NETPLAY_HASH_SKIP=<start>-<end>,...` | excludes hex guest address ranges, end exclusive; `0x` optional; malformed ranges are refused |

[V] Replay EOF returns neutral pads while retaining the last recorded connection flags (an empty recording has no
connected pads), with one log line. Reads before the first flip return neutral, disconnected pads. Replays have a
512 MiB / 10,000,000-frame limit; magic, version, lengths, connection flags and trailing bytes are checked before
states are decoded. The unit test includes 10,000 fixed-seed random malformed buffers.

[V] Hash coverage: every current allocation in the virtual 4K, virtual 64K and physical heaps, and each mapped PE
section with the writable characteristic. The executable is mapped by `runtime/loader.cpp`; the probe reads its
mapped section table, without reopening the disc. Blocks end at 64 KiB guest address boundaries or allocation/section
ends. The multiply-xor hash packs eight bytes per word and includes addresses, sizes of included bytes and heap
identities in the total. Fully skipped blocks disappear. The block list is reused; each heap's allocation list is copied once per frame (a debug probe). Timing includes enumeration, hashing and output. [?] These are unsynchronised observations of running
guest memory, not consistent snapshots; GPU/audio buffers are included until explicitly skipped. Use matching skip
ranges for both runs. A hash contains no memory dump, and the diff tool reads only these probe outputs.

To record, replay twice and diff, first set `EXE` to the rebuilt game executable and `ISO` to its read-only disc image.
Use a new scratch directory for each run, the same starting save/settings/figures, and the full
[isolation recipe](../../getting-started/testing.md#isolation-every-game-run). Keep portal changes identical manually;
only pads are recorded. Example (run each game to normal exit, one process at a time):

```sh
probe="$PWD/.scratch/netplay"; mkdir -p "$probe"
probe_run()
{
    run="$probe/$1"; shift
    mkdir -p "$run"
    nice env SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=/dev/null \
        SKY_SETTINGS="$run/none.ini" SKY_LAUNCHER_INI="$run/none.ini" SKY_COLLECTION="$run/collection" \
        SKY_SAVE_DIR="$run/save" SKYLANDERS_DATA="$run/data" SKY_LOG="$run/game.log" \
        SKY_LAUNCHER_NO_UPDATE_CHECK=1 SKY_NETPLAY_DETERMINISTIC=1 "$@" "$EXE" "$ISO"
}
probe_run record SKY_NETPLAY_RECORD="$probe/pads.bin"
probe_run replay-a SKY_NETPLAY_REPLAY="$probe/pads.bin" SKY_NETPLAY_HASH="$probe/a.hash" SKY_NETPLAY_HASH_BLOCKS=1
probe_run replay-b SKY_NETPLAY_REPLAY="$probe/pads.bin" SKY_NETPLAY_HASH="$probe/b.hash" SKY_NETPLAY_HASH_BLOCKS=1
python3 -I tools/netplay/hash_diff.py "$probe/a.hash" "$probe/b.hash"
python3 -I tools/netplay/hash_diff.py --self-test
```

[V] `runtime/main.cpp` and the UWP entry point end with `std::_Exit` (static destructors never run), so both call
`netplay::Finish()` first: it closes the probe files and logs the hash timing once. Hash files are flushed every
frame, so a crash keeps every complete frame, without the timing line.

The diff reports the first unequal or missing frame, then up to 16 differing blocks with guest address and heap.
Exit codes: 0 identical, 1 divergence, 2 invalid input.

[V] [SSA] First measurement (2026-11-16, M1 Pro, Europe disc): the fast recipe's presses recorded over 1059 frames,
replayed twice. The replays differ from frame 1 (3 blocks: one 64K-heap block, two in the executable's data); from
frame 10 on, about 145 of the 8193 blocks (2%) differ in every frame, stable to frame 1400: about 92 of 6904 physical
blocks, 40 of 1240 in the 64K heap, 12 of 49 in the executable. Hashing the 512 MB costs about 80 ms per frame (the
game runs at 12 FPS while it hashes). The guest scheduler is what these numbers wait for.

[V] Formats are our own, little-endian, version 1. Pads: `SKYPAD01` (8 bytes), u32 version, u32 game-id byte length,
u64 frame count, u32 user count, then the ASCII game id (1–64 lowercase letters, digits or underscore). Each frame
has `MaxUsers` records: u8 connected, u16 buttons, u8 left/right trigger, four signed i16 sticks (13 bytes per user).
The recorder updates the frame count and flushes after every complete frame. Blocks: `SKYBLK01` (8 bytes), u32 version;
then per frame u64 frame, u32 block count, followed by `(u32 address, u32 heap, u64 hash)` records. Heap ids:
1 virtual4K, 2 virtual64K, 3 physical, 4 executable image. Neither format stores guest bytes.

## Decisions (owner, 2026-11-08)
1. **Shared camera in lockstep for 0.10.0; free exploration stays the goal.** 0.10.0 ships the game's own co-op (one
   world, one shared camera, each player on his own PC). Free exploration (each player his own camera, the 2026-11-04
   entry) is a later stage after 0.10.0, not dropped: it builds on the lockstep core (deterministic runtime, protocol,
   session setup) and adds per-game world sync; its own study comes after 0.10.0 ships.
2. **Sessions start from the game's launch** in 0.10.0 (no joining a game in progress).
3. **Same platform by default**: Mac-Mac and PC-PC. A launcher option unlocks cross-platform sessions for testing, with a
   clear "not tested" warning.
