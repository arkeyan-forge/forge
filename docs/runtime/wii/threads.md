# The Wii host's threads, events and timers

> **Purpose:** how do the game's threads, its interrupts and its timers run on the host, one processor between them? · **Status:** current (2026-11-24) · **Tags:** [V]

## One processor
The Wii has one processor; the operating system (the SDK's OS, linked into the game) schedules its threads by
priority, with its queues in guest memory. On the host every guest thread has its own host thread, and only the one
holding the processor (`wii::Processor`, `runtime/wii/processor.{h,cpp}`) runs guest code; the others wait for it to
be handed to them. The host decides nothing: the SDK's scheduler (`SelectThread`) runs as the game's code, and the host
only carries out its context switches (`runtime/wii/threads.cpp`):
- `OSSaveContext(context)`: the calling host thread now runs as `context`; r3 = 0 (saved). Only the stack pointer is
  written into the OSContext (the SDK's checks read it); the registers stay with the host thread.
- `OSLoadContext(context)`: the processor is handed to that context's host thread, and the caller waits until its own
  context is loaded again; it then returns from `OSLoadContext`, and the scheduler returns as it would from
  `OSSaveContext`. A context never saved (a new thread, `OSCreateThread`) gets a host thread (8 MB of host stack) that
  starts at its saved `srr0` with its registers (gpr, fpr, ps1, the GQRs, cr, lr, ctr, xer, from the OSContext's
  offsets: gpr at 0x000, cr 0x080, lr 0x084, ctr 0x088, xer 0x08C, fpr 0x090, srr0 0x198, GQRs 0x1A4, ps1 0x1C8). When
  its function returns, its link register's function runs (`OSExitThread`, as `OSCreateThread` sets it).

## Events: the interrupts
An IOS reply, an alarm, the video retrace come from other host threads as events (`Processor::Post`); the holder runs
them at the guest's safe points, as interrupts would: where interrupts are turned on (`OSEnableInterrupts`,
`OSRestoreInterrupts` with interrupts on), or while the scheduler is idle. An event's guest function runs through
`CallGuest`, on a copy of the holder's registers (below its stack pointer), the OS's current context (`0x800000D4`) the
scheduler's idle one meanwhile, so that a wake-up inside switches nothing. Then `__OSReschedule` runs: a thread the
event readied above the current one takes the processor, as after an interrupt. Inside a handler, interrupts turned on
are no safe point (no event nests in another).
- An exception handler returns by loading the context it interrupted (`OSLoadContext`): `CallGuest` gives it the idle
  context, and loading that unwinds the handler back to `CallGuest` (a C++ exception through the translated frames).
- **Idle**: the scheduler clears its current thread (`0x800000E4`), turns interrupts on and spins until a priority is
  ready (`RunQueueBits`, at the game's address): the host waits for events instead, until one readies a thread. Nothing
  for 10 s: every thread's state and the queue it waits on are logged, and the boot stops.

## Timers (`runtime/wii/timers.cpp`)
One host thread waits for the next of two:
- the **decrementer**: `PPCMtdec(ticks)` sets it (the OS's alarms do), the timebase's ticks (60.75 MHz); when it runs
  out, the SDK's `DecrementerExceptionCallback` runs, which fires the alarms due and sets it again;
- the **video retrace**, 59.94 times a second from `VIInit` on: VI's first display interrupt's status bit
  (`0xCC002030`, 0x8000) set, then the SDK's `__VIRetraceHandler` (the retrace callbacks, `VIWaitForRetrace`'s
  waiters). One at a time: a retrace still pending is not posted again.
- the **audio interface's** interrupt, every buffer (3 ms) from `AIStartDMA` on: the DMA callback ([audio.md](audio.md)),
  one at a time likewise.

## What the game's addresses give (`games/racing/`)
`OSSaveContext`, `OSLoadContext`, the interrupt switch, `PPCMtdec`, `VIInit` (the host starts the retrace after it)
are hooks; the scheduler's `RunQueueBits`, its idle context, `__OSReschedule`, `DecrementerExceptionCallback` and
`__VIRetraceHandler` are addresses (`wii::os::GameAddresses`). The OS's own globals in low memory (the current thread
at `0x800000E4`, the current context at `0x800000D4`) are the same in every Wii game.

## Tests
`ArkeyanWiiTest`: the processor's hand-over between host threads in the holder's order, events posted from another
thread and run on the holder (one posting another), the idle wait's timeout; clean under the thread sanitizer. On the
real disc: the first new thread (the engine's, priority 17) starts and hands the processor back.
