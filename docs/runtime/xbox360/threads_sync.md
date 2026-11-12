# Threads, synchronisation, APCs, time

> **Purpose:** how are guest threads, dispatcher objects, waits, APCs, critical sections, time and fences implemented? · **Status:** current (2026-11-11) · **Tags:** [Xbox] [Lux] [SSA]

Code: [`thread.{h,cpp}`](../../../runtime/thread.cpp), [`kernel/threads.cpp`](../../../runtime/kernel/threads.cpp),
[`kernel/sync.cpp`](../../../runtime/kernel/sync.cpp), [`kernel/objects.{h,cpp}`](../../../runtime/kernel/objects.cpp),
[`kernel/system.cpp`](../../../runtime/kernel/system.cpp), [`kernel/overlapped.cpp`](../../../runtime/kernel/overlapped.cpp).
Class: all **[Xbox]** unless marked **[Lux]** (the way the engine uses it) or **[SSA]**.

## Guest calling convention (XenonRecomp output)

- A recompiled function is `void f(PPCContext& ctx, uint8_t* base)`. `base` = host address of guest address 0
  (guest address A is `base + A`, big-endian data: `g_memory.Read<T>/Write<T>` swap).
- Arguments `ctx.r3..r10`, floats `ctx.f1..`, return `ctx.r3`. **Arguments 9 and up are on the guest stack at
  `r1+0x54, +0x5C, ...`** (the caller's parameter area). Known uses: `XamContentCreateEx`'s overlapped (`r1+0x54`),
  `NtCreateFile`'s options (`r1+0x54`), `D3DDevice_Resolve`'s clear stencil (`r1+0x5C`).
- Hooking: generated functions are weak. `PPC_FUNC(sub_82xxxxxx)` overrides one; call the original through
  `PPC_FUNC_IMPL(__imp__sub_X); __imp__sub_X(ctx, base);`. No `midasm_hook` is used anywhere: every hook is a whole-function
  override ([hook inventory](../../tools/toolchain/sigmatch.md)).
- `r13` is the **PCR** pointer; `r1` the guest stack; `ctx.lr`/`ctx.ctr` hold the link/count registers (the "Indirect
  call to unrecompiled guest code (ctr=.. lr=..)" message prints them).

## Thread model

| Piece | Value |
|---|---|
| Host thread per guest thread | `PlatformStartThread(16 MB stack, ...)`: recompiled calls nest deeply on the host stack |
| PCR (0x2D8 bytes, `SystemAlloc`) | `+0x00` TLS pointer, `+0x30` self, `+0x70` stack base, `+0x74` stack limit, `+0x100` current KTHREAD, `+0x10C` CPU number |
| KTHREAD (0xAB0 bytes) | `+0x14C` thread id (`GetCurrentThreadId` reads it, **[SSA]** `sub_822DCD50`); dispatcher header type 6 at +0 (so `KeWaitForSingleObject(thread)` works) |
| TLS | `.tls` template copied at `*(PCR+0)`; `__declspec(thread)` data addressed directly from it; dynamic slots (`KeTlsAlloc`) after `tlsDataSize` |
| Guest stack | `SystemAlloc(stackSize, 0x1000)`, size from `ExCreateThread` (min 0x4000) or the XEX default (0x40000) |
| Reuse | `SystemAlloc` never frees (about 1000 stacks filled it): an ending thread gives its PCR, KTHREAD, TLS and stack back (`GiveBackBlocks`, under `g_kernelLock` with the exit), and a new thread of the same stack size takes them, zeroed, once 8 others of that size have ended after them (`QuarantinedBlocks`: a title may still wait on the KTHREAD of a thread that just ended). `ArkeyanGuestThreadTest`: 2000 create/exit cycles at 256 KB |
| Start | `xapiStartup(start, context)` when given (XAPI's thread entry), else `start(context)`; suspended threads wait on `g_kernelSignal` for `suspendCount == 0` |
| Host calls into the guest | `CallGuest(ctx, target)` (`indirect_call_guard.cpp`) for every guest address host code calls: thread starts, APCs, the graphics interrupt and audio callbacks. The indirect-call guard's range test: a target outside the code (or misaligned) is logged once (`[guard] host call to 0x... refused`) and not called, r3 = 0; unchecked, the function table read there gave a garbage host pc (SIGBUS) |
| Exit | normal return, or `ExTerminateThread` -> C++ exception `ThreadExit` caught at the entry; then `exited = true` (the thread object becomes signalled) |
| Host-driven threads | `StartHostDrivenThread(body, cpu)`: a thread with a guest context that *calls into the guest*: the graphics interrupt thread and the audio driver thread |
| Main thread | `CreateGuestThread(entryPoint, ...)`; the host's main thread owns the window and input (macOS requires it) |
| Quit | `SDL_EVENT_QUIT` / window gone / guest main exit -> `std::_Exit(0)` immediately, **no SDL teardown under running threads** (D28: an `SDL_Quit` freed the audio stream while the audio thread polled it: `pthread_mutex_lock+12` crash) |

CPU affinity: `flags >> 24` of `ExCreateThread` picks `cpu = ctz(mask)` only to fill the PCR field; the host scheduler decides.

## Kernel objects

- **Handle objects** (`Nt*` APIs): `g_handles`, handle = `0x10000 + 4n`. `0xFFFFFFFE` = current thread.
- **Dispatcher objects** (`Ke*` APIs get the *guest header* address): `LookupDispatcher(header)` creates the host object
  from the header type on first use (type 0 notification event, 1 synchronization event, 2 mutant, 5 semaphore; limit at
  `header+0x10`), and **binds it through a marker in the header's wait list**: `+0x08 = 'SSAK' (0x5353414B)`, `+0x0C = binding id`.
  Inlined `KeInitializeEvent/...` in game code overwrite the header, so the binding is recreated from the header type.
  The table (`g_dispatchers`) is keyed by header address: a new binding replaces the header's previous object (keyed by
  binding id, it kept one more object at each re-initialisation); waiters hold their own reference.
  Signal state is mirrored into `header+4` after every change for code that reads it directly.
- All signal state is under one mutex `g_kernelLock`. **Each waiting thread sleeps on its own condition variable**
  (`WaitForObjects`' `thread_local` sleeper), listed in every object it waits for (`KernelObject::sleepers`) and in its
  APC queue for an alertable wait; a signal (`SetEvent`, `PulseEvent`, `ReleaseSemaphore`, `ReleaseMutant`, a thread's
  exit, an overlapped completion, `QueueUserApc`) wakes only those threads, with the lock held (a sleeper's condition
  variable lives as long as its thread, and it leaves every list before it returns). `g_kernelSignal` is left for new
  threads waiting to be resumed. **[Xbox] [V] (2026-10-26)**: until then every waiter slept on that one condition variable
  and every signal was a `notify_all`. Giants keeps ~25 guest threads waiting (5 JQ workers, audio, streaming, network...)
  and its render thread releases a job semaphore many times per frame: in Chapter 1, `NtReleaseSemaphore` was **27 % of
  the render thread** (`__psynch_cvbroad` 22 %) and the woken threads fought over `g_kernelLock` (`__psynch_mutexwait`).
  The fix halved the render thread's work per frame (Chapter 1 median 22.7 -> 10.8 ms, p99 51 -> 18 ms;
  numbers in [renderer_threads.md](../gpu/renderer_threads.md), "Giants"). Generic: any title with many waiting threads gains.
- **Events**: type 0 manual-reset, 1 auto-reset. `PulseEvent` is real: `pulseEpoch` + `pulseReleases`; waiters register
  (`waiters++`) at the start of a wait so a pulse can release them. A manual pulse releases all registered waiters, an
  auto pulse one.
- **Semaphores** clamp at the maximum; **mutants** are recursive per KTHREAD.
- **Critical sections** are `X_RTL_CRITICAL_SECTION` in guest memory: `+0x10` LockCount (init -1), `+0x14` RecursionCount,
  `+0x18` OwningThread = **KTHREAD stored big-endian** (CAS + `atomic_ref::wait`). Not a host mutex: game code reads the fields.
- **Spin locks** (`KfAcquireSpinLock`): CAS with the PCR as owner, `yield` loop.
- **Waits** `WaitForObjects(objects, waitAll, timeoutPtr, thread, apcs)`: relative (negative) or absolute (positive, system
  time) timeouts in 100 ns; `STATUS_TIMEOUT` 0x102; alertable waits end with `STATUS_USER_APC` 0xC0 as soon as an APC is queued.
  **Count 0 or > 64 for `NtWaitForMultipleObjectsEx` fails with `STATUS_INVALID_PARAMETER_1` (0xC00000EF)**:
  **[Lux]** after a level load that queued no WideLoad jobs the title waits for *all of zero* events and relies on the failure
  ([lux/str_levels_loading.md](../../engines/lux/str_levels_loading.md); blocking there hung the main thread while it held the portal lock, D6).

## User APCs (what Wwise depends on)

`NtReadFile`/`NtWriteFile` with a completion routine (XAPI passes `ReadFileEx` routines with the **low bit tagged**):
the transfer completes synchronously and `CompleteTransfer` queues `UserApc{routine, context, ioStatusBlock, 0}` on the calling
thread. The APC runs in that thread's next alertable wait (`DeliverUserApcs`: saves the whole PPC context, sets
`r3..r5 = context, argument1, argument2`, calls the routine through `CallGuest`, restores). `KeDelayExecutionThread` with
alertable=1 also ends early to run them. **Wwise bank loading depends on this.** XAM overlapped completions
(`CompleteXOverlapped`) queue the XOVERLAPPED completion routine as an APC the same way.

## XOVERLAPPED completion (`kernel/overlapped.cpp`)

`FinishXamCall(overlapped, result, length, pcr)`: with an overlapped, the call is completed *at once* but reports
`ERROR_IO_PENDING` (997) as the console does. Fields: `+0x00 InternalLow` (Win32 result), `+0x04 InternalHigh` (length),
`+0x0C hEvent`, `+0x10 completion routine`, `+0x18 dwExtendedError` (HRESULT `0x8007xxxx`). **`InternalLow` is published last**
(release fence): the title polls it through `XGetOverlappedResult` (`0x3E5` = pending) from another thread.

## Yielding and idle loops [Lux]

The job system polls queues with `NtYieldExecution`; LOD workers and spin waits use `KeDelayExecutionThread` with a zero
interval. On the console that only burns a hardware thread the title owns; on a PC the main thread spent 91% of its samples in
`sched_yield`. Fix (`Yield()` in `threads.cpp`): per thread, if the previous yield was less than **100 us** ago the spin
counter grows; after **32** such yields every further one sleeps **200 us**. The counter resets once the thread did real work.
The decision is `kernel/yield_backoff.h` (`ArkeyanYieldBackOffTest`, a fake clock).

**A job that yields between items of real work is not a spin** (2026-11-11) [SSA] [V]: SSA's sun tile classification job
(`sub_821404D8`) calls `Sleep(0)` after each of its 385 tiles, 2-5 us of work apart (a polling loop: under 1 us). The rule
above took it for a spin from the 33rd tile on, so each job slept ~350 x 200 us and took **~115 ms instead of ~2.7 ms**:
the tile classes reached the screen 3 to 7 frames late, and on Windows, where a 200 us sleep lasts about 1 ms or more, far
later. A gap threshold cannot tell the two apart on every CPU, so the hook that knows the job says so:
`kernel::WorkingThreadScope` around the call (`engines/lux/light_tiles.cpp`) keeps that thread's yields plain while it
lives. The polling loops keep their back-off. Found with `sample` on the job's thread: 97 % in
`KeDelayExecutionThread`'s `sleep_for`. Effect on the picture: [plume_backends.md](../gpu/plume_backends.md).

## Time

| Item | Behaviour |
|---|---|
| `KeTimeStampBundle` | ticked every 1 ms by a detached host thread (`StartTimeStampBundle`). XAPI `GetTickCount` and Bink's `RADTimerRead` read `TickCount` **directly**; movies stalled without it |
| `KeQuerySystemTime` / `GuestSystemTime` | host system time as FILETIME (1601 epoch) |
| `mftb` | translated by XenonRecomp to the host counter (`cntvct_el0` on arm64, TSC on x64) |
| `KeQueryPerformanceFrequency` | `PlatformTimeBaseFrequency()`: `cntfrq_el0`, or the TSC measured against `steady_clock` over 100 ms once |
| Timers (`NtCreateTimer`, `NtSetTimerEx`) | **not implemented** (weak stubs): the title does not use them |
| Windows | `timeBeginPeriod(1)` in `PlatformInit` (sleeps and timed waits would otherwise round up to the 15.6 ms system tick; desktop only, UWP has none) |

## Fences (the biggest trap) [Xbox]

XenonRecomp emitted **nothing** for `lwsync`, `sync`, `eieio`. The title's spin locks release with `lwsync; stw 0`: a plain
store a weakly ordered CPU (ARM64) may publish before the critical section's stores, so the next owner read stale free
lists: rare heap corruption (1 run in 10-94) in the heap coalesce `sub_828A2360`. Fix in `patches/xenonrecomp.patch`:
`lwsync` -> acquire-release fence, `sync` -> seq-cst fence, `eieio` -> release fence (1773 sites in SSA's `private/ssa/ppc/`).
`stwcx.`/`ldarx.` are translated as seq-cst CAS. See [toolchain/xenonrecomp.md](../../tools/toolchain/xenonrecomp.md).
