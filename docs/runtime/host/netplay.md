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
| GPU results the game reads | fences, ring read pointer, resolves into guest memory, written by the host GPU thread when done | the guest sees the GPU done at fixed points (its waits complete deterministically); what the host GPU writes for the game's CPU differs between two GPUs, so a game's hook marks it and what the game derives from it for drawing alone as render-only (`netplay::SkipRenderOnly`), left out of the state hash and the repair; a per-game audit that the game logic never reads it (SSA: the light tile job, below) |
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
- **Desync detection**: every 30 frames, each PC hashes the game's state (the guest heaps, without the render-only
  memory a game's hook marks: what the host GPU computed for the game's CPU) and sends it with its inputs; 8 frames
  later, when the other player's hash has come, they are compared, so no game holds for it. On a mismatch both games
  hold at that frame while the guest sends its block hashes.
  With identical layouts and at most 64 different blocks, the host supplies their hashed bytes and both verify the
  repaired total. Success resumes the session with **Back in sync** for two seconds. A changed layout, too many blocks,
  an unequal final total, five seconds without completing repair or a third repair within sixty seconds ends online
  play: both continue alone; the host can save, the guest cannot. The optional desync block dump remains available on
  failure. See step 4 for the transfer and validation rules.
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

## Step 0d: the last host values in guest memory [Xbox]

[V] Two replays of 0c still differed in a few blocks. The causes, each found and fixed, measured 2026-11-20 on an M1 Pro
(Europe discs, 0b's recordings, cooperative schedule, hashing every 30th frame):

| Cause | Where it showed | Fix |
|---|---|---|
| The translator emitted a guest `setjmp` as the host's, with the host `jmp_buf` (stack pointers, return addresses, values mangled with a per-process cookie) written into guest memory | Giants: 192 bytes at `0x8267DD30` of the executable's data from frame 30 on; SSA: the blocks that differed from the level load | XenonRecomp emits `PPC_SETJMP_BUF(r3)` / `PPC_LONGJMP_BUF(r3)`: host buffers kept by the runtime per host thread, keyed by guest address (`runtime/platform/jmpbuf.cpp`); guest memory is no longer written. Every game's code is regenerated (code generated before still builds, with the old behaviour) |
| The emulated portal sent its unprompted status reports at most every 8 ms of the **host's** clock, so the poll that got one depended on the host's speed | Giants: two physical blocks the game writes from what it read, from a frame that changed with each pair (240 to 1110) | In deterministic mode the interval runs on the game's virtual time (`EmulatedPortal::SetClock`, `GuestSystemTime`) |
| SSA's light-tile job reads the full-resolution depth the GPU resolved, written back into guest memory from an asynchronous read-back that is the latest finished one, so how old it was (or whether there was one) followed the GPU's speed | Two instances in lockstep, one slowed by frame dumps: 49 physical blocks from `0xA6A60000` (that depth target) differed at frame 750 | Online, read-backs are kept by request and the write-back always takes the one asked two depth resolves before, waiting for it if needed (`gpu::TakeReadback`, `gpu/readback_queue.h`); the shadows stay. Off line nothing changes. Different GPUs may still compute different depth bits: [?] |

[V] After both fixes, no block differs: SSA over 409 compared frames to frame 12270 (6.8 minutes of game at 30 FPS),
Giants over 177 to frame 5310 (a pair without any portal agreed too, which pointed at it). The event logs stay identical.
Tests: `ArkeyanJmpBufTest` (round trips, per-thread buffers, stable addresses, guest memory untouched), a virtual-clock
case in `ArkeyanPortalTest` and `ArkeyanReadbackQueueTest` (the one asked, not the latest; waits; a dropped request answers
at once); each failed first. With the read-back fix, two SSA instances in lockstep, both dumping frames, agreed on 190 of 190
checks to frame 5700.

[V] The four other games (2026-11-21, same machine and schedule, a generic route of presses recorded then replayed
twice) did not reach their first frame in deterministic mode; three causes, fixed:

| Cause | Where it showed | Fix |
|---|---|---|
| Before the game's first frame the host's main thread presents a stand-in every 16 ms, holding the GPU's producer lock through its vsync wait; a render thread that clears its targets meanwhile waited for the lock while holding the baton | Swap Force: the watchdog at boot, the render thread in `gpu::ClearColor` | Online, the stand-in presents only a new image (`host/display.cpp`) |
| The Alchemy games' wait for a swap's blank (`WaitForInterruptHandled`, from the swap-wait hooks) comes before the next frame's swap, and online the interrupts come only with the frames | Swap Force, Trap Team, SuperChargers, Imaginators: every thread blocked, no frame | The wait grants what the flip would: the GPU's interrupts for the commands submitted so far (`gpu::SynchronizeGuestInterrupts`), then one blank (`kernel/video.cpp`) |
| Bink's async decode thread polls its mailbox in a loop with no kernel call and no clock read | The watchdog with `BinkAsy*` holding the baton, 2 s at a time | A poll that found nothing yields online (`engines/alchemy/bink_poll.cpp`, `BinkAsync_Poll` in each game's `addresses.h`; Giants has no such thread) |

[V] Then every compared frame agrees, the event logs too: Swap Force 49 frames to 1470, Trap Team 108 to 3240,
SuperChargers 70 to 2100, Imaginators 319 to 9570. SuperChargers runs slowly online: a loading thread ("MagicMoment
Loading Thread") computes for over 2 s without a kernel call while every other thread waits for the baton [?].

[?] Not measured: longer pairs of those four, a 10-minute Giants pair, Windows and Linux, a real portal (sessions use
the emulated one).

### Tracing host-sourced loads (how the portal was found)

`-DARKEYAN_NETPLAY_TRACE=ON` (a separate build tree; OFF leaves the translated objects byte-identical) routes the
translated code's integer loads and stores (`PPC_LOAD_*` / `PPC_STORE_*`) through a **shadow** of guest memory (a second
4 GB reservation, committed on use). A store writes both; a load whose bytes differ from the shadow was written by the
host side (an import, a host thread, the loader): it is recorded and the shadow updated.

| Variable | Effect in a trace build |
|---|---|
| `SKY_NETPLAY_TRACE=<file>` | records host-sourced loads in `<file>` and one `frame <n> <count> <chain hash>` line per frame in `<file>.frames`; needs the cooperative schedule |
| `SKY_NETPLAY_TRACE_FRAMES=<a>-<b>` | full records only for frames a..b (inclusive); the summaries cover every frame |

Records (`SKYTRC01`, u32 version 1, then 37 bytes each, little-endian): u64 sequence, u32 frame, u32 guest address, u8
size, u64 value, u64 previous shadow value, u32 the translated code's link register. `tools/netplay/trace_diff.py --frames
a b` gives the first frame whose summary differs; `trace_diff.py a b` the first differing record and the ten before it on
each side. A first run with a range of `0-0` finds the frame; a second around it names the load. About 37 bytes x 1500
host-sourced loads per frame: a 2500-frame range writes 3.7 GB per run.

[V] On Giants it showed the two runs reading the same host values until one called `time()` where the other read a flag:
the branch came from a value outside the trace (a register, here the portal's reply or no reply), and a pair without the
portal then agreed. Values returned only in registers and accesses outside the macros (vector loads) are not traced.

## Step 1: lockstep on one machine

[V] The host-side core is implemented in `runtime/netplay/protocol.{h,cpp}`, `session.{h,cpp}` and `socket.cpp`.
`runtime.cpp` routes the first local pad (including the keyboard) into the host's seat 0 or the joiner's seat 1;
only the remote pad supplies the other seat. At latch N the local input is queued for N + D. Both pads are neutral
for the first D latches. Missing input holds the cooperative guest's baton, so every guest context and the picture
wait together while a separate host thread continues UDP reception and transmission. The scheduler watchdog is
suspended during that intentional network wait. A waiting line appears after one second; a missing peer or input
ends the session after fifteen seconds. Normal play adds no frame logs or frame allocations.

[V] The switches are opt-in; without HOST or JOIN the existing probe and live-input modes remain available:

| Switch | Meaning |
|---|---|
| `SKY_NETPLAY_HOST=<port>` | host on UDP port 1–65535; local pad becomes P1 (XAM user 0) |
| `SKY_NETPLAY_JOIN=<host>:<port>` | join an IPv4 address or DNS name; use `[IPv6]:port` for IPv6; local pad becomes P2 (user 1) |
| `SKY_NETPLAY_PASSWORD=<text>` | optional shared password used to derive packet keys |
| `SKY_NETPLAY_DELAY=<frames>` | 1–8; the host's override wins; otherwise five ping/pong probes select D = clamp(ceil(RTT / 2 / frame time) + 1, 1, 8), using the minimum measured round trip |
| `SKY_NETPLAY_CROSS_PLATFORM=1` | both peers must opt in to allow different OS/CPU identities |
| `SKY_NETPLAY_TEST_DESYNC=<frame>` | session tests only (removed unless `SKY_NETPLAY_TEST=1`): the guest flips one hashed byte at that checkpoint's barrier (a multiple of 30), before hashing, for the repair to put back |
| `SKY_NETPLAY_DESYNC_DUMP=<file>` | on failed repair, write the held checkpoint's blocks as a `SKYBLK01` sidecar before continuing solo |

[V] Startup rejects HOST together with JOIN, record or replay, and requires all three of
`SKY_NETPLAY_DETERMINISTIC=1`, `SKY_NETPLAY_DELIVERY=frame` and `SKY_NETPLAY_SCHEDULE=cooperative`.
The hello checks session protocol version, game id, the decrypted/decompressed executable image's SHA-256
(using the same `Sha256Hex` as the loader), runtime version, OS/CPU and FPS limit. Each side supplies a fresh
128-bit nonce. A mismatch is reported on both sides as `[netplay] session refused: <reason>`.
Every thirtieth checkpoint uses the same total and block selection as `SKY_NETPLAY_HASH`, even without a hash
output file. Both peers wait for that checkpoint's hash before progressing. A mismatch holds both at that frame for
step 4's repair; a failed repair logs its frame, local hash and remote hash. Dumps can be compared with
`python3 -I tools/netplay/hash_diff.py <host-dump> <join-dump>`. Quits, refusals, timeouts and desyncs send an end
packet repeatedly when possible (a desync only after repair fails). Step 3 adds the end overlay and solo continuation;
step 4 extends continuation to failed repairs.

[V] All integer fields are little-endian. The original step-1 format below was bounded to 512 bytes, with packets
at most 280 bytes; step 3 extends frames and adds the reliable event channel:

| Part | Bytes and fields |
|---|---|
| Common header | 32: `SKYNET01` (8), wire version (2), type (1), reserved zero (1), total size (2), reserved zero (2), sequence (8), contiguous remote-input acknowledgement (8) |
| Hello payload | 232: game id (64), image SHA-256 lowercase hex (64), runtime version (32), OS/CPU (32), FPS (4), sender nonce (16), echoed peer nonce (16), cross-platform flag (1), host flag (1), session protocol version (2); text is zero-padded |
| Ping / pong / ready payload | 8: probe token, echoed token, or the host's chosen delay |
| Frame payload | count (1, 0–8), then count × (frame number (8) + the existing 13-byte pad encoding), checkpoint frame (8), checkpoint hash (8); checkpoint zero means none yet |
| End payload | reason (1), checkpoint frame (8), sender hash (8), other hash (8) |
| Authentication suffix | 16: our SipHash-2-4-128 tag over every preceding byte |

[V] Password derivation uses SipHash-128 with a fixed protocol domain key; the session key tags the ordered host
and joiner nonces with that password key. Initial hellos use the sender nonce and a zero unknown peer nonce;
subsequent hellos echo the peer nonce and bind both. Other packets use the negotiated pair of nonces. A hello's
fixed nonce fields are only inspected to verify its tag before they can affect session state. Wire-version, size,
reserved-field, tag and payload failures are dropped and counted. An authenticated hello's session-version
mismatch is refused. A sequence window filters duplicate/stale session packets. Frame packets repeat the latest
eight local inputs; an acknowledgement window prevents a missing input from falling out of that history. The
network thread keeps transmitting the history during a stall, without a separate resend request.

[V] `ArkeyanNetplayProtocolTest` and `ArkeyanNetplaySessionTest` are registered with `sky_add_tests`, including its
sanitizer configuration. The tests were written first; their initial build failed because the implementation sources were absent. Protocol coverage includes
all packet types, a SipHash-128 known-answer vector, identity refusals, bad tags/versions/sizes, truncations and
100,000 random or mutated packets (each rejected and counted). The session test uses an injectable transport and
clock: 600-frame pad-pair equality and exact N + D delivery, all eight delays, 20% loss, duplicates, reordering,
bilateral identity refusal, a frame-300 hash mismatch, quit notification, and silent-peer timeouts before and after
setup. It also attempts 600-frame loopback UDP runs and a loopback desync run.

[V] The configured Giants build and isolated startup refusal checks were exercised, without launching gameplay.
The transport tests and protocol fuzz run pass natively and with address/undefined sanitizers; the existing netplay and scheduler tests pass too. Documentation checks report zero broken links. [V] The sandbox denied UDP binding with `Operation not permitted`;
the test reports that part as skipped and still runs all injectable-transport checks. [?] Actual loopback UDP,
IPv6/DNS connectivity, two game instances, pad seats in the game, real desync dump comparison and Windows remain
unverified here. Step 1 did not include portal events, settings/save transfer, router mapping or launcher UI. Step 3 adds portal
events and game-side session settings; save transfer, mapping integration and launcher UI remain separate. The
remaining determinism differences documented in step 0c may stop a real session at its first hash checkpoint.
Profiles follow the launcher's `SKY_PLAYER*` settings and are not negotiated; step 3 checks P2 sign-in through the
host-side parser. P2 joining inside a game still needs a game run.

## Step 3: session setup (game side)

A session requires `SKY_NETPLAY_FPS=30|60`, in addition to the step-1 scheduling switches. It overrides the settings
file and command line; the game cap and live F3 controls cannot change it. Both peers still check the resulting FPS
in the hello. Aspect is fixed to 16:9 (`aspectAuto` and `aspectFill` off), and `fps_auto_fallback` is off with its
explicit-setting flag set, so the GPU default cannot turn it back on. Startup logos are not skipped and Nightmare
unlocking is off. Before guest threads start, session setup removes guest-changing level, movie, scripted-input,
clock, portal-model and execution-debug overrides, including their legacy aliases. Save, collection and other
isolation variables are preserved. Figure editing is unavailable for the session.

The launcher supplies the host as `SKY_PLAYER_*` and the guest as `SKY_PLAYER2_*` on **both** computers. XAM user 1
uses the second profile; it remains signed in during a session even if its name is missing (the normal fallback
profile). A session always selects the emulated portal and logs once that real portals are not read. The seats saved in the session's `portal.txt` are restored at start: the launcher writes
the same file and figures into both games' session folders, so both start identical; `SKY_PORTAL_PLACE`, when provided, queues session events just like the keys.

Portal place, remove, replace and clear requests from the Toy Box, its linked window, wheel and keys enter the local
player's event queue. The next latch stamps them for N + delay alongside that player's pad. The host owns P1 and
shared seats; the joiner owns P2, including P2 vehicle seats. Fixed slot assignments reserve both halves of a
swapper. The other player's seats stay visible and locked; changing or swapping them is refused both by the UI and
by session validation. Clear removes only the local player's seats. UI previews may show a queued local choice,
but the guest sees it only at its scheduled frame.

The authenticated protocol keeps the original hello and end layouts and extends frame packets:

| Payload | Fields, little-endian |
|---|---|
| Frames | count (1), up to eight records of frame (8), cumulative event sequence for that frame (8), pad (13); checkpoint frame (8), hash (8), contiguous reliable-event acknowledgement (8), menu-open flag (1; 0 or 1) |
| Event | reliable sequence (8), target frame (8), figure id (8; zero removes), slot (1), flags (1: dump present, transaction end), optional 1024-byte figure dump |

The menu-open flag is presentation metadata: it is not latched as a pad or applied to guest memory. Only the newest
authenticated frame packet changes the peer's marker, so reordered packets cannot restore an old menu state.
Datagrams are bounded to 1200 bytes; a full figure event is 1098 bytes. Each sender resends up to eight unacknowledged
events per network tick, independently of redundant pads. Only the next reliable sequence is accepted; duplicates
and out-of-order copies cannot reapply an event. A frame waits for both pads and every event through their advertised
sequences. Transaction boundaries preserve compound replacements, and both peers apply P1 before P2. The network
worker applies events while the cooperative guest holds its frame barrier; the guest's frame path uses fixed buffers
and performs no new allocation or portal file I/O. Queues are bounded to 32 changes per latch and 512 reliable records
per direction. A full local queue refuses the request rather than overwriting an accepted event.

Figure ids are separate for each player. The first accepted placement sends its dump; later placements use its id.
Both sides play session copies in `<SKY_SAVE_DIR>/online-figures/`. The receiver prepares the first dump before
acknowledging it. Guest progress updates those copies; normal session finalization copies only locally owned figures
back to their original files and releases collection locks. Remote progress stays in the session folder. No paths or
collection metadata are sent over the wire.

F3 has an **Online with <pseudo>** strip with the peer's picture, measured round trip, input delay and **in sync**
after a successful hash checkpoint (**checking** before the first). Shared Settings rows are greyed with **Set by the
host**. F3 leaves the game running and sends its open/closed state in frame-packet metadata; the peer sees **in the
menu**. In this checkout the offline overlay also leaves the guest running: it suppresses local inputs, with no
runtime pause to disable. The menu and Shift+F3 statistics are described in [gamemenu.md](gamemenu.md#debug-screen).

**Leave the session** confirms before sending repeated end packets and returning to the launcher. When an established
peer quits, times out or loses its connection, the remaining game continues solo immediately. It retains the local
player's input seat and supplies neutral, disconnected input for the departed seat, discards pending session events
and lifts the departed player's figures, including shared seats when the host departs. Network hash checks stop and
the worker stops sending after its final end packets. For four seconds a remaining host sees **<pseudo> left the
game**. A remaining guest sees **The host left. You can keep playing, but nothing will be saved.** Its title-save
writes, pending save commits, deletes, renames and content-package changes are disabled for the rest of the run.
Figure writes remain enabled. Local session copies are flushed to their source files and their save destinations
are retargeted without lifting the figures; later solo writes go directly to those sources. Finalization retries any
copy whose handoff failed.

Refusal and a local leave keep the existing four-second end screen/quit path (the menu's confirmed local leave posts
the normal quit event directly). A desync holds the cooperative guest at the hash barrier with its watchdog suspended
while step 4 attempts repair. Failed repair writes the requested block dump and releases both games into solo play.
The end notice says **The games went out of sync. Both players continue alone.** The guest also sees **Nothing will
be saved.** The same persistent title-save suppression and figure handoff used for a remote departure apply.

Every session end writes `<SKY_SAVE_DIR>/online-ended.txt` as `reason=<code>`, including solo continuation: **quit**
for a peer's leave, **timeout** for a timeout or lost connection, **desync** for a failed repair. Startup refusal and
other end codes remain `protocol`, `game`, `image`, `runtime`, `platform`, `fps`, `seats`, `transport` and `invalid`. Setup removes a stale end
marker. In solo F3, **Play online** confirms before writing `<save dir>/open-online.txt` (the game id and newline)
and posting the normal quit event, so the launcher can open this game's Online page.

[V] Tests were added before implementation, failing first against the missing protocol, statistics, continuation,
save-policy, formatting, marker and layout APIs. `ArkeyanNetplayProtocolTest` covers menu flags as well as
figure-event encoding and malformed packets. `ArkeyanNetplaySessionTest` exchanges 600 frames with loss, duplicates
and reordering at delays 1–8, checks desync barriers, reliable events, periodic hash counts, menu open/close delivery,
ping history, packet/byte rates, lost packets, rejected tags, input waits and sixty-second expiry. Both host and guest
survivors keep exchanging local input with a disconnected, neutral departed pad. Timeout is tested with virtual time.
`ArkeyanNetplaySetupTest` covers the persistent save-write switch, which the title-save path reads, the debug lines
from supplied numbers, exact launcher marker contents, forced settings, profiles and the four-second message clock.
`ArkeyanToyboxLookTest` checks that both the online strip and solo Play online row fit above the footer.
`ArkeyanToyboxSwapTest` checks that retargeting keeps both present and pending figures and their status reports.
These use existing `sky_add_tests` targets. `ArkeyanDebugScreenTest` was run directly in solo and online modes;
it covers the online rows and ping curve, collection and drawing without per-frame C++ allocation. This checkout
builds that executable but does not register it with CTest; changing build scripts is outside this task.

[?] No game was run. Two actual games, visual F3/end overlays, physical connection loss, Toy Box and wheel
interactions, figure progress flushes in a running game, P2 joining and Windows still need validation. Injectable
lossy transport does not need real sockets. This step does not transfer saves or languages, negotiate profiles,
integrate router mapping or build launcher UI. The determinism residue from step 0c still needs the game runs
described there. Both games must use this session implementation; the extended frame layout does not interoperate
with earlier frame layouts.

## Step 4: repairing a short desync

Protocol version **2** adds four authenticated messages. Each includes the checkpoint frame, so delayed packets from
an earlier repair cannot change a later one. The existing tag, sequence window, exact length checks and 1200-byte
datagram ceiling apply to all four.

| Message | Payload, little-endian |
|---|---|
| BlockList | frame (8), total stream bytes (4), chunk index (4), chunk length (2), up to 1024 bytes |
| BlockData | same chunk envelope as BlockList |
| BlockRequest | frame (8), stream (1: list or data), first chunk index (4), missing-chunk bitmap (8) |
| RepairResult | frame (8), phase (1: verify, commit or acknowledge), total hash (8) |

The guest's list starts with its block count (u32), then `(address u32, heap u32, hash u64)` in the probe's deterministic
order. The host checks every address and heap against its own list, including unchanged blocks. It sends a data stream containing a block count
and, for each differing block, its local list index, packed byte count (both u32) and packed bytes. Only bytes outside
the probe's sorted, merged `skips` are packed. The guest resolves each index through its own allocation extents, checks
the packed length and preserves all excluded bytes. Neither a peer address nor a peer length selects a memory mapping.
The runtime adapter uses the same virtual heaps, physical mappings and writable image ranges as `Probe::Allocation`.
`BlockSource` lets session tests supply ordinary byte vectors instead of guest memory.

The receiver requests missing chunks, up to four windows of 64 at once, and asks again only after a round trip
(counted in session loops of 2 ms, so a test clock held still does not stall it): asking every loop would have the
sender repeat each chunk dozens of times over a slow link. It marks each received chunk once. Requests and
chunks may be lost, duplicated or reordered. The complete list is limited to 1,048,576 blocks; repair data is limited
to 64 blocks of at most 64 KiB each. Stream sizes, chunk boundaries, final-chunk lengths, ordered unique data indices
and whole-transfer lengths are checked before use. The complete data stream is parsed before the first block write.
The guest recomputes its total and repeats **verify** until the host sends **commit** with the matching total. The guest
acknowledges commit; the host holds its barrier until that acknowledgement. Commit and its acknowledgement remain
repeatable after success, so losing either does not strand the peer. Successful repairs increment `SessionStats::resyncs`
once on each side, and the F3 debug screen's hash row shows the count. **Back in sync** shows for two seconds in the
upper-left notice area (where **in the menu** shows), once the game runs again: during the repair the picture holds,
as for any wait.

Each side allows two repair attempts in a sliding sixty-second window. A third attempt, a different layout, more than
64 differing blocks, invalid block bytes, a still-different total or a repair taking five seconds sends repeated
`End(Desync)`. Both games then use the established solo-continuation path: local input keeps its original seat, the
departed pad disconnects, remote figures leave and local figure writes continue. Host title saves remain enabled;
guest title saves stay disabled for the rest of the process. No repair buffers are created at ordinary checkpoints;
the regular frame path retains its fixed buffers and the probe's reserved hash list.

[V] Tests were written first and failed against the missing packet/source APIs.
`ArkeyanNetplaySessionTest` covers one repaired block, loss/duplicates/reordering, the 64-block boundary with full
64-KiB blocks, continuing through a later matching checkpoint, layout mismatch, 65 differing blocks, an unchanged
bad total, a third repair within sixty seconds, expiry of that limit and a five-second stalled transfer. It checks
both peers' repair counters and the two-second notice clock. `ArkeyanNetplayProtocolTest` round-trips all four
messages and rejects truncated, oversized and incorrectly bounded chunks. `ArkeyanNetplaySetupTest` checks desync
solo continuation, persistent guest save refusal, host saves and the end message. `ArkeyanNetplayTest` verifies packed
byte repair across skip boundaries, preservation of excluded bytes and rejection of a short write before mutation.

[V] macOS host tests: 95 of 95 pass. Live, two SSA instances on one Mac (loopback, offscreen, 30 FPS,
`SKY_NETPLAY_TEST=1` on both, `SKY_NETPLAY_TEST_DESYNC=900` on the guest): the guest flipped one hashed byte at frame
900; that checkpoint's totals differed in both hash logs, the repair ran at the barrier, and the 96 later checkpoints
(930 to 2910) were equal on both sides; the session ran on until the host was stopped.

[?] Not yet seen: a desync the game itself makes (the test flips a byte the game had not read yet), the notice on a
real screen, two machines, other platforms. Repair copies hashed guest bytes; it cannot restore host objects (thread
registers, kernel objects, GPU and audio state) or make a persistently nondeterministic game deterministic: such a
desync comes back at the next checkpoints and ends in the solo path.

## Step 5: speed, the Toy Box online, the version [Xbox] [SSA]

[V] The scheduler's handover. Each context waits on its own condition variable and a handover wakes the new holder
only (`Scheduler::Dispatch`); when no context holds the baton, every context polls again each millisecond for a host
completion, as before. Waking every context at each handover made SSA's forty threads queue on the scheduler's mutex:
a 5-second `sample` of an online session found guest code in 71 of about 22,500 samples, the rest in waits. The switch
order is unchanged (the 200 replays of `ArkeyanSchedulerTest` still match). `IdleWakes()` counts the wake-ups of a
context that is not the holder; the test that failed first ping-pongs two contexts 2000 times beside 16 parked ones and
asks for fewer than 100 (the old wake-up gave thousands).

[V] The state hash. `HashBytes` runs four independent multiply chains (one chain waits for each multiply), and
`HashAllocations` cuts every allocation of the checkpoint into its blocks first, then hashes them on four threads in
shares of equal bytes; each block's hash is the same whatever the thread count. The heap walk goes from one allocation
to the next (`GuestHeap::NextAllocation`, `AllocationAtOrAfter`) instead of page by page through free space.

[V] Measured 2026-11-22 on an M1 Pro, SSA's front end, Europe disc, cooperative schedule, offscreen:

| | Before | After |
|---|---|---|
| one game alone, no hashing | 19 FPS (53 ms frames) | 29.9 FPS (the 30 limit) |
| one checkpoint's hash, one game alone | 88 ms | 16 ms (the same 48 totals) |
| one game alone, hashing every 30th frame | 26.7 FPS | 29.8 FPS |

[V] The Toy Box online. A figure put on the portal from the Toy Box is copied into the session folder
(`online-figures/<player>-<n>.sky`) before it is sent; `SaveFigure` only rewrites a dump that exists, so the copy was
never written and every change was refused, shown as "The portal is full". `WriteNewFigure` creates the copies (the
sender's and the receiver's); `SaveFigure` still never creates a file. Every refusal now logs its reason
(`[portal] online: change refused: ...`, `[toybox] P1: ...`), and the Toy Box says the change failed instead of a full
portal. Live: the host put Gill Grunt on slot 0 from the Toy Box; the guest received its copy and the 63 checkpoints
(30 to 1890) were equal on both sides.

[V] The version. A game that plays online takes a minor version (SSA 0.10.0). In an online session the window title and
the F3 menu show `-o` after it (`netplay::VersionLabel`); the room compares the plain version.

## Step 6: a quality online [Xbox] [SSA]

The owner's bar (2026-11-22): solid and secure, playable with some latency and with a PC that cannot keep up, at 30 and
at 60 FPS. Measured on an M1 Pro, two SSA instances on one Mac, offscreen, the front end.

[V] **A lost connection.** No packet for `lostMs` (3 s) is a lost connection, not the end: both games hold, the session
keeps sending, and the first authenticated packet back resumes it (`[netplay] connection lost`, `[netplay] reconnected
after 10.1 s`). After `timeoutMs` (60 s) the game asks the player (`SessionConfig::askBeforeEnding`): wait a minute more
(`KeepWaiting`) or continue alone (`ContinueAlone`, the solo path of a timeout). A peer that answers but does not advance
(a long load on a slower PC) is waited for the same way instead of ending after 15 s. The UDP transport follows a peer
whose address changed (a router that restarted), only on a packet that is authenticated and new (`AcceptPeer` after the
sequence check). Live: the guest's process stopped for 10 s mid-session; the host reported the loss, then the
reconnection after 10.1 s, and the 39 checkpoints that followed were equal.

[V] **The input delay follows the network.** Each side moves its own delay (`adaptiveDelay`): the other game reads the
pads by frame number, so a frame skipped by a longer delay repeats the last pad sent and the pad of a frame that falls
on one already sent is dropped; both games read the same pads for every frame. Every 30 frames the worst of the last
three round trips sets it (one frame more at once, one less after five calm checks); waits add one frame at most (a
slower PC's waits would not be helped by more delay).

[V] **The frame advantage.** A side whose frame is ahead of the other's (counting half the round trip), on average over
30 frames by more than 1.5, sleeps half of it, two frames at most (`balanceAdvantage`, after GGPO's time
synchronisation): neither side runs ahead into the delay's edge and then waits in bursts.

[V] **60 FPS.** The checkpoint's hash is spread over the 30 frames of its window (`SliceAllocations`, `PlanWindow`): each
frame hashes one part on block boundaries, the window's total is compared at the next barrier, and a repair compares the
barrier's own blocks (`CombineBlocks`). At 60 FPS, scale 1: both games at 60.0 FPS, p95 16.7 ms; the hash 0.47 ms per
frame instead of 16 ms every 30th. A guest byte changed at frame 900 was repaired at 930 and every later checkpoint was
equal. At scale 2 the two instances share one GPU and hold 56 FPS; the online path itself holds 60.

[V] **A slower PC.** Both games follow the slower one frame by frame, evenly: a guest slowed by 30 ms a frame
(`SKY_NETPLAY_TEST_SLOW`) held both at 19.5 FPS, p95 within 6 ms of the median. The launcher's room says so beforehand and
starts a GPU-bound PC one resolution scale lower (`SKY_RESOLUTION_SCALE`, this run only): two games at scales 1 and 2
kept identical hashes. The scale cannot change in play (render targets and resolved textures keep it).

[V] **Security.** The session's packets after the hellos are encrypted with ChaCha20 (`netplay/chacha20.h`, our own
code checked against RFC 8439's vectors), a key from the session key, the nonce from the sender's side and the packet's
sequence; the tag covers the ciphertext and is checked before anything inside is read. Protocol version 3. The launcher's
room carries a secret in its code, encrypts its own messages and refuses guessing (described in the launcher's online page).

[V] **The banner.** Nothing on screen while it plays; a corner chip with the round trip from 120 ms; a banner while the
game is held: "Connection lost with Kaos, reconnecting... 42 s", "Still no answer", "Waiting for Kaos's game", with Esc
or B to continue alone, Enter or A to wait more, R or Y to play together again (`host/toybox/online_banner.h`). After a
session the F3 menu's "Play online" reads "Play together again". Either writes `online-replay.txt` and quits; the
launcher sets the same room up again. Closing the window during a session writes `quit`.

[V] **Two GPUs.** The first session between two Windows PCs (an AMD and an NVIDIA card) went out of sync in the
first lit scene, the title menu, about 20 s in: two 64 KB blocks differed at every checkpoint, the repair put them
back and they differed again. Lux's light tile job (`sub_821404D8`, the "ZtoLOD" worker threads) reads the scene depth
that the runtime writes back from the host GPU ([level_frame_lightprepass.md](../../engines/lux/level_frame_lightprepass.md)),
and two GPUs round its last bits differently. Its depth (4 KB a tile), its outputs (a class byte and two floats a
tile, read by the sun's tile draws) and its threads' stack frames are marked render-only from the job's hook
(`engines/lux/light_tiles.cpp`), at the same guest point on both computers, and taken into the hash's skips at the
next frame. Proof on one Mac: with one depth bit in 61 flipped on the guest (`SKY_NETPLAY_TEST_DEPTH_NOISE=1`, a
session test), the title menu desynced at frame 60 and ended at 120 before; after, five minutes played in `level_027`
(thousands of changed texels a frame, four figure changes) kept 275 checkpoints equal without a repair.

[V] **Two CPUs.** The same two PCs (an AMD Ryzen 7 7700, an Intel i5-12400F) repaired two 64 KB blocks a few seconds
after each figure was placed, and ended at the third repair within a minute. The Wwise sounds are XMA, decoded by
FFmpeg (`apu/xma_decoder_ffmpeg.cpp`), whose SIMD code is chosen by the CPU (AVX-512 on one, AVX2 on the other, NEON on
a Mac) and rounds differently; the title mixes the decoded samples into its own memory. A deterministic run now decodes
with FFmpeg's plain C code (`av_force_cpu_flags(0)`; `[xma] FFmpeg's plain C code`). Proof on one Mac: with the C code
on the host and NEON on the guest (`SKY_NETPLAY_TEST_XMA_NATIVE=1`, a session test) a played `level_027` repaired
`e4d70000` at frame 90, `e4d70000 e4d80000` at 180 and ended at 270, the testers' pattern; with the C code on both,
81 checkpoints stayed equal through four figure changes, with `SKY_NETPLAY_TEST_DEPTH_NOISE=2` on the guest too (one
depth texel in 61 at the nearest or farthest value: no leak from the light tile job either).

[V] **The disk.** `XamContentGetDeviceData` and `NtQueryVolumeInformationFile` told the title the host disk's size
and free space, which differ between two PCs and change with every file written. A deterministic run now tells it a
fixed 120 GB drive with 100 GB free (`netplay::TitleDiskSpace`, `ArkeyanNetplayTest`).

[V] **What else the host told the title.** A review of the kernel calls that answer from the host, after the two PCs:
the time zone and its name (`ExGetXConfigSetting` 3/1-3/3, which the title stamps its saves with; the testers' PCs were
two hours apart) are UTC in a deterministic run; `XAudioGetSpeakerConfig` says stereo whatever the device (Wwise mixes
for the layout it is told, in the title's memory); `XNetRandom` gives bytes from a fixed SplitMix64 seed, the same on
both computers and in a replay (SSA never calls it, a played session with a blank figure logged no call; other titles
may). Settled already: the video mode (1280x720 at 60 Hz, fixed), directory listings (sorted), the clocks (virtual),
the settings the host sends. `ArkeyanNetplayTest` covers the helpers (`netplay/clock.h`). Test switch:
`SKY_NETPLAY_TEST_HOST_RANDOM=1` gives the host's random bytes again.

[V] **Achievements.** Two Windows PCs went out of sync right after "Hello!! Skylander": the title enumerates the
achievements after an unlock, and each PC answered with its own player's list and wall-clock unlock times
(`kernel/xam_achievements.cpp`). Online, the title now sees the unlocks of this session alone, timed by the session's
clock (`GuestSystemTime`); each player's progress file still gets the unlock with its real time, and the overlay's page
shows the player's own list. `ArkeyanAchievementsKernelTest` runs itself a third time as a session to check it.

[V] **Where a repair was.** `[netplay] state repaired at frame N (B bytes sent): K blocks, at <address> ...` names up to
eight repaired blocks (guest addresses, the same on both computers), so a player's log says which memory differed.

[V] **The late check.** The checkpoint held both games until the other player's hash came: one network leg every 30
frames, which cost 3 to 9 FPS at 60 FPS with 50 to 150 ms of round trip. The hash now rides on the inputs at the
window's end (`Session::PostHash`) and is compared 8 frames later (`CheckPostedHash`), when it has come within the
input delay (at most 8 frames). A repair then happens at that frame, both games at once, and the window that began
before it is not posted. `ArkeyanNetplaySessionTest` (`LateCheck`) covers both cases.

[V] **A punched path.** For players without UPnP the launcher punches through both routers before the game starts
(the launcher's online page). The game keeps that path: a joiner binds the same local port (`SKY_NETPLAY_LOCAL_PORT`,
`UdpOptions::localPort`), and a host knocks at the joiner's public address until its first packet arrives
(`SKY_NETPLAY_PEER`, `Transport::Knock`), so the host's router lets the joiner in. A knock is one byte, which the joiner
drops as too short.

[V] **The desync report.** An online session records its inputs, both players' pads frame by frame, in its folder
(`online-inputs.rec`, the `SKY_NETPLAY_RECORD` format); a desync the repair could not mend copies it beside the game's log
(`netplay-desync.rec`) to replay the session alone (`SKY_NETPLAY_REPLAY`); figure changes made during the session are not
in it.

## Testing a bad network

With `SKY_NETPLAY_TEST=1`, set
`SKY_NETPLAY_TEST_NET=delay=150,jitter=20,loss=2,dup=1,cut=30000-35000,cut=60000-120000,seed=7`
to delay outgoing datagrams by 150 ± 20 ms, lose 2%, duplicate 1%, and cut both sending and receiving during the
specified intervals in milliseconds since the transport opened (start included, end excluded). Any subset is valid;
omitted values are zero. Jitter is uniform, negative delays clamp to zero, and datagrams may reorder. Only sending
is delayed: set the switch on both games to affect both directions. The same seed reproduces the random choices
for the same sends and clock schedule. Set `SKY_NETPLAY_TEST_SLOW=40` on one game to add a 40 ms sleep each frame,
simulating a PC that cannot keep up. Malformed values refuse the session; both switches are removed unless
`SKY_NETPLAY_TEST=1`. Use separate save and figure folders under `.scratch/` for each game.

## Decisions (owner, 2026-11-08)
1. **Shared camera in lockstep for 0.10.0; free exploration stays the goal.** 0.10.0 ships the game's own co-op (one
   world, one shared camera, each player on his own PC). Free exploration (each player his own camera, the 2026-11-04
   entry) is a later stage after 0.10.0, not dropped: it builds on the lockstep core (deterministic runtime, protocol,
   session setup) and adds per-game world sync; its own study comes after 0.10.0 ships.
2. **Sessions start from the game's launch** in 0.10.0 (no joining a game in progress).
3. **Same platform by default**: Mac-Mac and PC-PC. A launcher option unlocks cross-platform sessions for testing, with a
   clear "not tested" warning.
