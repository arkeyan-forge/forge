# Memory: address space, heaps, physical windows

> **Purpose:** how is the guest address space laid out (heaps, physical windows) and how do faults show? · **Status:** current (2026-10-26) · **Tags:** [Xbox]

Code: [`memory.{h,cpp}`](../../../runtime/memory.cpp), [`kernel/memory.cpp`](../../../runtime/kernel/memory.cpp),
[`platform/*`](../../../runtime/platform). Class: **[Xbox]** unless marked.

## The guest address space

- One host reservation of **4 GB** (`GuestMemory::Reserve`, `PlatformReserveMemory`): guest address `A` = `base + A`.
  Big-endian accessors swap on every read/write; **floats swap their bits** (`GuestMemory::Swap` uses `bit_cast`). The
  XenonUtils `ByteSwap(float)` converts numerically (cast to integer and back): that bug returned garbage for every
  `Read<float>` (viewports 0,0,0,0) until fixed (gotcha D7).
- The first 64 KB (`0x0..0xFFFF`) are made inaccessible: null-pointer accesses fault like on the console.
- The XEX image maps at its base (SSA: `PPC_IMAGE_BASE 0x82000000`, `PPC_IMAGE_SIZE 0xD40000`, code `PPC_CODE_BASE 0x82110000`,
  `PPC_CODE_SIZE 0x8EBAAC`; these constants come from `private/<id>/ppc/ppc_config.h`, generated per game). The **function lookup
  table** for indirect calls sits right after the image in the same block:
  `PPC_LOOKUP_FUNC(base, a) = *(PPCFunc**)(base + IMAGE_BASE + IMAGE_SIZE + (a - CODE_BASE) * 2)`, i.e. 8 bytes (one host
  pointer) per 4-byte instruction slot, starting at guest `0x82D40000` for SSA (~18 MB). `LoadXex` fills every slot with
  `UnmappedFunction` then the `PPCFuncMappings` of `private/<id>/ppc/ppc_func_mapping.cpp`, then makes the table
  **read-only** (`PlatformProtectReadOnly`, in 64 KB granules inside it): a stray guest write there faults where it happens
  instead of making a later call jump anywhere. `CreateHostThunk` opens one granule for its write.

| Range | Use |
|---|---|
| `0x00010000..0x40000000` | virtual heap, **4 KB pages** (`g_virtualHeap4K`) |
| `0x40000000..0x70000000` | virtual heap, **64 KB pages** (`g_virtualHeap64K`; MEM_LARGE_PAGES requests and fixed addresses inside it) |
| `0x70000000..0x80000000` | `SystemAlloc`: runtime-owned memory, a bump allocator that **never frees** (PCRs, KTHREADs, TLS blocks, stacks, kernel variables, XAudio context cells, host thunks' data); ended threads' blocks are reused ([threads_sync.md](threads_sync.md)) |
| `0x82000000..` | the XEX image, `.data`, `.tls`, imports |
| `0x7FEA0000..` | XMA register file (ordinary guest memory; see [audio_xma_xaudio.md](audio_xma_xaudio.md)) |
| `0x7FC86544` | VBlank status register (bit 0, see [video_vblank.md](video_vblank.md)) |
| `0xA0000000` / `0xC0000000` / `0xE0000000` | **physical windows** (64 KB / 16 MB / 4 KB pages) onto 512 MB of physical memory |

## Virtual allocation (`NtAllocateVirtualMemory` / `NtFreeVirtualMemory`)

`GuestHeap` is a first-fit allocator over `[begin,end)` at page granularity, allocations come back **zeroed** (the free
path `memset`s the range). Rules we implemented from observed calls:

- base 0: allocate anywhere in the 4 KB heap, or the 64 KB heap if `MEM_LARGE_PAGES` (0x20000000);
- a base inside an existing allocation = **commit of pages of an earlier reservation** (returns the same address);
- an explicit free base: reserved at exactly that address;
- `MEM_DECOMMIT` (0x4000) is **ignored**: decommitted pages stay reserved with their old contents (the game decommits and
  recommits 64 KB pages of its `0x420F0000` reservation; filling decommitted pages with 0xCD changed nothing, so the game
  never relies on recommitted pages being zero) [SSA];
- `NtQueryVirtualMemory` is a stub (never called).

## Physical memory (`MmAllocatePhysicalMemoryEx`)

- 512 MB of physical memory is a `GuestHeap(0x10000, 0x20000000, 0x1000)` (offset 0 would read as a failed allocation).
  The **page-size flags choose the window**: `MEM_LARGE_PAGES` -> `0xA0000000` (64 KB pages), `MEM_16MB_PAGES` (0x80000000)
  -> `0xC0000000`, else `0xE0000000` (4 KB pages).
- The returned **guest address** is `window | physicalOffset`; our runtime does **not** alias the windows: each physical
  allocation remembers its window (`g_physicalWindows`, keyed by physical base), and `PhysicalToGuest(physical)` turns a
  physical address (what the GPU, XMA and fetch constants hold) back into the guest address the allocation is used through.
- **The `0xE0000000` window addresses physical memory +0x1000** (as on the console): `WindowAddress(E, physical)` =
  `0xE0000000 | (physical - 0x1000)`; `MmGetPhysicalAddress(addr)` adds 0x1000 for `addr >= 0xE0000000`. E allocations
  therefore **reserve one extra page** so the physical range the GPU sees never overlaps the next allocation. Without
  this, Bink planes were read 4 KB too low (D8).
- `MmGetPhysicalAddress(a)` = `(a & 0x1FFFFFFF) + (a >= 0xE0000000 ? 0x1000 : 0)`.
- **A read-only D3D lock** (`D3DLOCK_READONLY`) gets the cached `0xC0000000` view from the XDK (`0xC0000000 + physical`),
  which here reads other memory (zeros): the hook `GAME_HOOK_D3D_LockResource` (`d3d/resource_lock.h`) maps it to the
  allocation's own window with `PhysicalToGuest`. Found in Imaginators (#47); the other games' locks are not hooked [?].
- `MmMapIoSpace(type, physical, size, protect)` returns `PhysicalToGuest(physical)`: XMA engine 1 maps its contexts through
  it and writes through the result ([audio_xma_xaudio.md](audio_xma_xaudio.md)).
- `MmQueryAllocationSize`, `MmQueryAddressProtect` (image or known allocation = `PAGE_READWRITE`, else 0).
- `PhysicalToGuest` is called ~4000 times per frame (vertex, index, texture addresses): a **per-thread cache of the last 8
  allocations**, invalidated by a generation counter bumped on every `MmFreePhysicalMemory`, cut it from 247 to 106 profile samples.
- Known harmless bug: `GuestHeap::Free` zeroes `Translate(base)` where `base` is a *physical heap offset*, so freeing
  physical memory zeroes the low virtual address of the same number (nothing lives there) instead of the freed memory.

## Where things live at run time [SSA]

| Address | What |
|---|---|
| `0xAA5E0000` (2.56 MB) | the engine's **vertex streaming ring**: dynamic vertices; every draw's fetch constant says "size = the rest of the ring" |
| physical `0x06A60000` (via `0xA6A60000`) | a level's scene depth resolve (`DepthMap`), read by a CPU job ([lux/level_frame_lightprepass.md](../../engines/lux/level_frame_lightprepass.md)) |
| `0x40121xxx` / `0x4162xxxx` | the two alternating arenas of per-frame billboard nodes (the SIGBUS investigation) |
| `0x44xxxxxx..0x46xxxxxx` | live particle `Action` objects |
| `0x420F0000` | the game's main heap reservation (decommit/recommit of 64 KB pages) |

## Windows commit strategy (platform_win32.cpp)

`VirtualAlloc(MEM_RESERVE | MEM_COMMIT)` of 4 GB counts against the commit limit immediately. If it fails (no page file, or an
Xbox One title budget of about 5 GB), the runtime **reserves only** and installs a vectored exception handler that commits
**64 KB granules on first access** (`OnFirstAccess`, retry if another thread committed it meanwhile); kernel-mode writes into
a reserved page *fail* instead of faulting, so `PlatformReadFile` commits the destination before reading. A message
`[platform] cannot commit N MB up front: guest memory is committed on first access` marks the fallback. Faults are reported
through an *unhandled-exception filter* (VEH is desktop-only and also sees GPU drivers' first-chance faults).
POSIX: `mmap` with lazy commit.

## Fault report

On a fault the platform layer calls `OnFault(description, hostAddress)`: a host address inside the 4 GB block is printed as
`Fault (...) at guest address 0x...`; the POSIX handler adds thread name, sp/fp, pc and lr (symbolised), a frame-pointer walk
checked against the thread's stack bounds, and the executable's load address for `atos -l`; Windows prints module+offset per
frame and the load address (frames of the executable shown at the linker's base 0x140000000 for `llvm-symbolizer`).
One fault reports at a time; a fault inside the report exits at once; reports use an alternate signal stack so stack
overflows are reported too.
