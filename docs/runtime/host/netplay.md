# Online play (0.10.0): netplay in input lockstep

> **Purpose:** how two players, each on his own computer with his own portal and Skylanders, play one game together online, and what the runtime must change to make that possible? · **Status:** design; step 0a and 0b probes implemented and measured on SSA and Giants (the guest scheduler is next) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants] [SwapForce] [TrapTeam] [SuperChargers] [Imaginators]

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

## Step 0b: long replays and frame-counted deliveries [Xbox]

[V] Three probe tools: `SKY_NETPLAY_HASH_EVERY=<n>` hashes one frame in `n` (a 10-minute replay at the game's own
speed), `SKY_NETPLAY_HASH_BLOCK=<bytes>` narrows the blocks of `<file>.blocks` (256 to 65536), and
`SKY_NETPLAY_EVENTS=<file>` writes a frame-stamped log: per frame the vblanks, other graphics interrupts, audio periods
and timebase reads, and in order the guest threads created, the files opened and the kernel's allocations and frees
(`NtAllocateVirtualMemory`, `MmAllocatePhysicalMemoryEx`). `hash_diff.py --events <a> <b>` compares two logs (the
notes' order, then the frames each note moved by, then each counter per frame); `hash_diff.py --summary <a> <b>` gives
the differing blocks per heap in every sidecar frame. Every variable is in
[debugging.md](../../porting/debugging.md). A section that runs past the mapped image (Giants' last, read-only one) is
cut at the image's end; 0a refused the hash for it.

[V] `SKY_NETPLAY_DELIVERY=frame` counts two host-paced sources in frames instead of wall time
(`runtime/netplay/delivery.cpp`): each flip grants its blanks (the present interval's, at least 1) and its audio
periods (256 samples at 48 kHz: 3.125 per 1/60 s, the remainder carried), the interrupt and audio threads deliver one
grant per tick of their own clock, and the flip returns once the grants' guest callbacks have run. The blanks keep their
16.667 ms tick, which still paces the frames. A thread with no grant for about 500 ms outside a flip delivers anyway
and logs `vblank fallback` or `audio fallback` (a game that waits for a blank without presenting); a flip whose
callbacks have not run in 250 ms logs `late`. Off by default: deterministic mode alone keeps 0a's wall-clock deliveries.

[V] [SSA] [Giants] Measured 2026-11-16 on an M1 Pro (Europe discs, a loaded machine), one 10-minute recording per game
(SSA: the fast recipe, then walking, jumping and attacking in level_027; Giants: `tools/regress/giants_press.py 600`),
then two replays per mode, hashing every 30th frame:

| | SSA, wall clock | SSA, `frame` | Giants, wall clock | Giants, `frame` |
|---|---|---|---|---|
| frames compared | 10279 | 13557 | 19848 | 19179 |
| frames whose vblank count differs | 7660 | 0 | 976 | 0 |
| frames whose audio period count differs | 9646 | 0 | 9801 | 0 |
| frames whose graphics interrupt count differs | 4179 | 5527 | 9428 | 9304 |
| frames whose timebase read count differs | 10185 | 11915 | 19102 | 18486 |
| threads created and files opened: same order, largest frame shift | yes, 37 | yes, 1 | yes, 1 | yes, 2 |
| 64 KB blocks differing at the plateau | ~2360 of 10107 | ~2180 of 10110 | ~300 of 7307 | ~360 of 7307 |

[V] Frame-counted deliveries are exact: 0 differing frames for both sources and both games, no fallback and no `late`
in any of the four `frame` replays. They hold SSA's file opens within 1 frame of each other (37 with the wall clock) but do
not shrink the heap divergence, so the blanks and the audio periods are not its main source. In both games the divergence settles and stops growing: SSA's jumps at the level load (frame ~600,
from about 250 to about 1700 blocks) and holds about 2180 from frame ~2000 to 13557; Giants' holds about 300 to 360 from
frame ~3000 to 19000, almost all in physical memory (2 to 3 blocks of the 64K heap, 6 to 7 of the executable).
[?] A divergence that stops growing points to buffers rewritten every frame (GPU and audio data, job queues) rather
than a simulation drifting apart; hashes cannot tell the two apart, and the owner's 2 min 45 s replay showed no visible
drift.

[V] [SSA] What still differs is guest thread timing. In `frame` mode the kernel receives the same allocation requests
(3714 and 3709, the same sizes but for 7 and 2) in a different order from the 40th, at frame 2; only 25 and 22
allocations land at an address the other replay never used, so the heap layout is nearly the same and the differing
blocks are differing contents. The timebase read count per frame (each thread's reads of `mftb` and the system time)
differs in 88 % of SSA's frames and 96 % of Giants': threads poll the clock as often as the host lets them run. The GPU's
swap and fence interrupts follow the GPU thread, and their count differs in 40 to 49 % of the frames.

[V] Verdict: the cooperative guest scheduler of section 3 is needed whole, with the GPU interrupts delivered at fixed
points of the frame like the blanks and audio periods here. Per-thread clocks alone cannot help while the threads'
order is free. The next step is the scheduler behind `SKY_NETPLAY_DETERMINISTIC`, with `SKY_NETPLAY_DELIVERY=frame`
as its base, measured with these same recordings.

## Step 0c: cooperative guest scheduler [Xbox]

[V] `SKY_NETPLAY_SCHEDULE=cooperative` runs one guest context at a time (`runtime/netplay/scheduler.{h,cpp}`).
Startup refuses it unless `SKY_NETPLAY_DETERMINISTIC=1` and `SKY_NETPLAY_DELIVERY=frame` are set too; without it,
threads keep the host scheduler and the deliveries of 0a and 0b.

[V] One baton, one FIFO. Guest threads and the graphics and audio callback contexts register before their host thread
starts, so the host's wake order never picks the next holder. The holder hands over only at fixed points: a kernel
wait or delay, an explicit yield, a contended critical section or spin lock, thread start, resume and exit, the flip,
a spin hint, and the poll quantum. At a signal and at each handover the blocked contexts are checked in creation order;
a wait that succeeds takes its event, semaphore, mutant or lock before it joins the queue, and the signaller joins
after the contexts it woke. APCs run on their target's context. A suspended thread waits for its resume; a thread
that exits signals its object before it passes the baton.

[V] Virtual deadlines. Waits and delays carry 100 ns deadlines on the virtual clock. When no context is runnable the
clock jumps to the earliest deadline (ties in creation order), and the time-stamp bundle and the timebase follow; the
events log notes each jump (`idle <100 ns units>`). Otherwise the scheduler waits for a host completion (the GPU, a
file read).

[V] Poll loops. A thread that polls a flag never blocks, so it would keep the baton for ever. `PollQuantum` (1024)
counts its clock reads (`mftb`, the system time) and its entries into a free critical section, and yields once the
count is reached; the count restarts only at a handover, not at a signal or a kernel call that did not block (Giants
hung at boot until both changed: a thread took and left a free section in a loop). The translator patch also emits
`PPC_SPIN_HINT()` for `db16cyc`, `cctpl` and `cctpm` (regenerated: 49 sites in SSA, 23 in Giants); code generated
before the patch still builds and keeps the poll quantum.

[V] The flip. A cooperative flip finishes the GPU commands already submitted (`gpu::SynchronizeGuestInterrupts`), then
grants the pending graphics interrupts, the blanks and the audio periods; the graphics context runs that interrupt
batch before its blanks. The flip returns once every granted callback has returned, with no host timeout and no
wall-clock fallback. Since the blanks no longer wait for their tick, the flip then holds the baton until its blanks'
wall time (`netplay::FlipDue`: two periods at 30 FPS, a late frame at once); the first measurements, without it, ran
Giants at 60 with the limit at 30, twice its speed. GPU execution and host output stay on their own host threads.

[V] Host file times. A host file's time (the save) was the host clock minus the file's age; in deterministic mode it
is now the virtual clock's epoch (`netplay::HostFileTime`), the same in every replay.

[V] The watchdog reports a holder that has not handed over for 2 s, with each context's state, deadline and the
holder's guest location at its last handover, and a scheduler blocked with no deadline. It never makes up an
interrupt to get out.

[V] `ArkeyanSchedulerTest` failed first against a pass-through scheduler. It checks 200 identical switch orders with
the host threads started in reverse, waits, a signal undone before the handover, spin hints, the poll quantum (polls
short of a quantum keep the baton, a full one hands over), virtual deadlines and their ties, resume, and the
pass-through when the option is off; then 200 frames of blank and audio grants with callbacks that yield, and flips that
wait for them. The watchdog mode reports its stalled holder. `ArkeyanNetplayTest` checks the host file times and the flip's pacing.

[V] [SSA] [Giants] Measured 2026-11-17 on an M1 Pro (Europe discs) with 0b's recordings, two 10-minute replays per
game, hashing every 30th frame:

| | SSA, 0b `frame` | SSA, 0c | Giants, 0b `frame` | Giants, 0c |
|---|---|---|---|---|
| frames compared | 13557 | 9233 | 19179 | 25479 |
| frames whose vblank, audio, graphics interrupt or timebase count differs | up to 11915 | 0 | up to 18486 | 0 |
| threads, files, allocations, idle jumps: same order and frame | from the 40th allocation, no | 4113 notes, yes | no | 45 notes, yes |
| 64 KB blocks differing at the plateau | ~2180 of 10110 | 1 of 10108 | ~360 of 7307 | 3 of 7307 |
| largest count | ~2180 | 29 (frame 2190, gone by 2400) | ~360 | 3 |

[V] The scheduler makes both games' event logs identical: every count per frame, and the order and frame of every
thread, file, allocation and idle jump. What still differs is a handful of blocks. SSA's first difference is at the
level load (frame 630, two 1 KB ranges of the executable's data) or, in some pairs, at frame 30; Giants' at frame 30,
1 block of the executable and 2 of physical memory, unchanged to the end. Ruled out by a run without each: the
emulated portal, the GPU thread (`SKY_GPU_THREAD=0`) and the idle jumps (identical in both logs).

[V] [SSA] A trace of every guest load and store, in order, with one chain hash (valid because one context runs at a
time) finds the first differing access at a different place in each pair: once a 64-bit value loaded by
`sub_828531B8` (two times about one frame apart), once a register saved by `__savegprlr_19` (0 against
0xFFFFFFFFFFFFFB00). Neither value was stored by translated code before it differs, so each comes from the host side:
a kernel import's result or a write by the runtime into guest memory.

[V] Frame cost, the mean of the `[perf]` frame medians over 150 s (no hashing, each run alone, the same recordings):

| | SSA at 30 | SSA at 60 | Giants at 30 | Giants at 60 |
|---|---|---|---|---|
| `frame` deliveries, host scheduler | 40.1 ms (24.8 FPS) | 39.9 ms (24.9 FPS) | 33.3 ms (30.0 FPS) | 19.5 ms (50.6 FPS) |
| `frame` deliveries, cooperative | 59.5 ms (16.7 FPS) | 59.8 ms (16.7 FPS) | 33.5 ms (29.8 FPS) | 16.7 ms (58.6 FPS) |

[V] [SSA] SSA's level is bound by the guest's CPU on this Mac in both modes (offscreen at the automatic scale), and one
context at a time costs it about half again: its worker threads no longer run beside the main one. Giants holds 30 in both;
at 60 the cooperative run is the faster one, for a reason not measured yet. Two 4-minute
replays per game after the flip's pacing still give identical event logs, with 1 differing block in each game.

[V] Verdict: the scheduler is the base lockstep needs: the guest's order of events is the same in every replay of both
games. The residue is not one cause but several host values that reach the guest outside the scheduler's control. The
next step (0d) logs every kernel import's result and every runtime write into guest memory, in the same chain, to name
them, then fixes each, until two replays match for 10 minutes. SSA's cost (half again per frame) is the other open
item before lockstep.

[?] Not tested: the four other games, portal changes during a replay, a real portal, the fence-retirement stubs and the
GPU-written ranges of section 3.

## Step 0d: keep host jump buffers out of guest memory [Xbox]

[V] The translator's `setjmp`/`longjmp` buffer expression pointed into guest memory. The host `jmp_buf` contains
host stack pointers, return addresses and process-dependent state; it accounts for the reported 192 differing bytes
in Giants' executable data between replays.

[V] XenonRecomp now emits `PPC_SETJMP_BUF` / `PPC_LONGJMP_BUF`. Only the buffer expression changes: `setjmp` stays
directly in the translated function. The runtime keeps stable buffers keyed by guest address in host thread-local
storage, without a lock or guest writes. A missing buffer on the calling thread logs one `[setjmp]` line and aborts.

[V] `ArkeyanJmpBufTest` checks both round-trip return values (including zero becoming one), reuse on one thread,
stable addresses after storage growth, isolation between live threads using the same guest address, and byte-identical
synthetic guest memory. The missing-buffer check verifies the diagnostic and abort. The existing generated Giants
code still builds with `ninja -C build-giants -j4`.

[?] No game or replay was run for this change. Existing translations retain their old buffer expression until they
are regenerated; no game code was regenerated here.

### Trace host-sourced loads

[V] `ARKEYAN_NETPLAY_TRACE` is a CMake option, default **OFF**. ON forces `runtime/netplay/trace_macros.h` into the
translated code and enables the runtime's trace integration. The integer `PPC_LOAD_U8/U16/U32/U64` and
`PPC_STORE_U8/U16/U32/U64` macros, including their forwarding `PPC_MM_` forms, use inline accessors. A prebuilt
`ARKEYAN_PPC_LIBRARY` is refused with this option: its accesses could not be instrumented.

| Variable | Effect in a trace build |
|---|---|
| `SKY_NETPLAY_TRACE=<file>` | enables the shadow and binary host-sourced load records in `<file>`, plus per-frame summaries in `<file>.frames`; startup requires `SKY_NETPLAY_SCHEDULE=cooperative`, `SKY_NETPLAY_DETERMINISTIC=1` and `SKY_NETPLAY_DELIVERY=frame` |
| `SKY_NETPLAY_TRACE_FRAMES=<a>-<b>` | writes full records only for decimal frames a..b inclusive; default all frames; invalid or reversed bounds are refused; shadow updates, global sequence numbers and frame summaries continue outside the range |

With `SKY_NETPLAY_TRACE` unset, the accessors only read or write guest memory. With it set, the shadow reserves a
second 4 GB address space, initially zero, without eagerly backing it with physical memory: anonymous `mmap` on
POSIX, `VirtualAlloc` reservation and commits on demand on Windows. A translated store updates both memory and
the shadow. A translated load compares its captured bytes with the shadow; a difference records a host-sourced
load and updates those shadow bytes. An unchanged second read is not recorded. There is no host heap allocation
per access; record writes are buffered and both files are flushed at frame end.

The binary format is little-endian, with no padding: eight ASCII bytes `SKYTRC01`, then `u32 version = 1`, then
37 bytes per record in this order:

| Field | Type | Meaning |
|---|---|---|
| sequence | `u64` | host-sourced load count since startup, starting at 1, including records filtered out by the frame range |
| frame | `u32` | the probe's frame number; 0 covers startup before the first flip, then advances once per flip |
| address | `u32` | guest address |
| size | `u8` | 1, 2, 4 or 8 bytes |
| value | `u64` | loaded guest value, zero-extended |
| shadow | `u64` | previous shadow value, zero-extended |
| lr | `u32` | low 32 bits of the translated context's link register |

Every frame, including an empty frame, has one text line `frame <n> <count> <64-bit hex chain hash>` in
`<file>.frames`. The count and hash restart each frame; the hash starts at `netplay::HashSeed` and applies
`netplay::Mix` to address, size and value, in that order, for every host-sourced load. Finalization also writes the
current partial frame and closes the files once. The frame number follows the probe, rather than virtual-clock
ticks (a flip can cover more than one tick).

Build separately, then use the same replay and isolation variables as step 0a. First compare the summaries to find
the frame; repeat with `SKY_NETPLAY_TRACE_FRAMES` around that frame to narrow the binary trace:

```sh
cmake -S . -B build-giants-trace -G Ninja -DARKEYAN_GAME=giants -DCMAKE_BUILD_TYPE=RelWithDebInfo -DARKEYAN_NETPLAY_TRACE=ON
ninja -C build-giants-trace -j4
python3 -I tools/netplay/trace_diff.py --frames .scratch/netplay/a.trace.frames .scratch/netplay/b.trace.frames
python3 -I tools/netplay/trace_diff.py .scratch/netplay/a.trace .scratch/netplay/b.trace
python3 -I tools/netplay/trace_diff.py --self-test
```

The binary comparison prints the first differing record on each side, then each side's ten preceding records, with
guest addresses and link registers in hex. Missing records or frames also differ. Exit status: 0 equal, 1 different,
2 malformed or unreadable input. Comparisons stream the files and keep only ten preceding records in memory.

[V] `ArkeyanTraceTest` failed first with the missing implementation, then passed: guest store/load suppression,
a plain host write recorded once, overlapping widths, zero shadow pages, cross-page shadow copies, inclusive range
filtering with continuous sequence numbers, frame summaries and a changed host value changing the chain.
`trace_diff.py --self-test` also failed first, then passed, including malformed files and unequal lengths.
The synthetic startup check verifies activation, the unset-variable path, refusal without cooperative scheduling,
frame-range filtering and the recorded link register. The separate Giants `RelWithDebInfo` trace build passes with
`ninja -j4`; every translated macro use compiled with `ctx` in scope. The existing OFF build also passes, and all
93 translated objects remain byte-identical to their pre-change copies.

[?] No game or replay has been run with this trace. Host values returned only in registers are outside this memory
trace, as are accesses that bypass these macros. Windows reservation and commitment have not been exercised here.

## Decisions (owner, 2026-11-08)
1. **Shared camera in lockstep for 0.10.0; free exploration stays the goal.** 0.10.0 ships the game's own co-op (one
   world, one shared camera, each player on his own PC). Free exploration (each player his own camera, the 2026-11-04
   entry) is a later stage after 0.10.0, not dropped: it builds on the lockstep core (deterministic runtime, protocol,
   session setup) and adds per-game world sync; its own study comes after 0.10.0 ships.
2. **Sessions start from the game's launch** in 0.10.0 (no joining a game in progress).
3. **Same platform by default**: Mac-Mac and PC-PC. A launcher option unlocks cross-platform sessions for testing, with a
   clear "not tested" warning.
