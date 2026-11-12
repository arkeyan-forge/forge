# Kernel and XAM exports we implement (295 imports: SSA's 218, Giants' 33, Swap Force's 44)

> **Purpose:** which kernel and XAM imports does the runtime implement, which are weak stubs, and what does each do? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [SSA] [Giants] [SwapForce]

Code: `runtime/kernel/*.cpp`. The import list is `runtime/kernel/import_list.inc`: **295 names**, the union of SSA's
import table (218), the 33 that Giants adds (commit "Kernel: the imports Skylanders: Giants adds (33)"; most are in
`runtime/kernel/imports_giants.cpp`) and the 44 that Swap Force adds ([below](#swap-forces-extra-imports-44-all-implemented-xbox-swapforce)).
Counted from the code on 2026-10-26 (a name is *implemented* when a strong `PPC_FUNC(__imp__Name)` exists under
`runtime/`): **218 implemented, 77 weak stubs**; all 33 Giants imports and all 44 Swap Force imports are implemented. Class: **[Xbox]** = Xbox 360 generic (reuse as is), **[Lux]** = a behaviour the Lux engine depends on,
**[SSA]** = specific to this binary. Nearly everything here is [Xbox]; the traps are marked.

## How an import is bound

- XenonRecomp turns each import thunk into `nop; nop; nop; blr` and the loader maps the thunk address to
  `__imp__<Name>` in the function lookup table (`loader.cpp` `LoadXex`: first every code address becomes
  `UnmappedFunction`, then `PPCFuncMappings`). A strong definition `PPC_FUNC(__imp__Name)` in `kernel/` wins over the
  weak one that `kernel/stubs.cpp` generates for **every** name of `import_list.inc` (`IMPORT(name)` macro): the stub logs
  `[stub] Name (lr=.. r3=.. r4=..)` once and returns 0.
- Arguments are `ctx.r3..r10`; the 9th and later are on the guest stack at `r1+0x54, +0x5C, ...` ([threads_sync.md](threads_sync.md)).
- **Variable imports** (data, not functions) are resolved in `ResolveImports`: an IAT slot without a thunk is a variable;
  `CreateKernelVariable(name)` allocates guest memory for it (table below).
- Two xam exports are not imports: ordinals **0x4A1 / 0x4A2** (`XamInputNonControllerGetRaw/SetRaw`, the portal),
  reached by the title through `XexGetProcedureAddress` and answered with host thunks.
- **Adding a game**: diff the new XEX imports against this list (`tools/day1.sh` does it); implement each missing one
  *with its outputs written*, in our own code (Xenia and the other projects are read-only references; the Xenia-derived
  `kernel/xmp.cpp` keeps its attribution).

## Kernel variables (data imports) [Xbox]

| Variable | Value we give |
|---|---|
| `XboxKrnlVersion` | 8 bytes: 2.0.17559 as u16 major, minor, build (the SDK of SSA is 2.0.20500; the *system* is 17559) |
| `XboxHardwareInfo` | 16 bytes, byte +4 `NumberOfProcessors` = 6 |
| `XexExecutableModuleHandle` | pointer to the module's LDR entry |
| `ExLoadedCommandLine` | `"default.xex"` (quoted) in a 0x200 buffer |
| `KeTimeStampBundle` | 0x18 bytes updated **every 1 ms** by a host thread: `+0x00` InterruptTime (100 ns since boot), `+0x08` SystemTime, `+0x10` TickCount (ms) |
| `VdGpuClockInMHz` | 500 |
| anything else | 0x100 zeroed bytes (monitor data, object types, GPU globals) |

The LDR entry (0x100 bytes): `+0x18` DllBase, `+0x1C` ImageBase, `+0x20` ImageSize, `+0x38` FullImageSize, `+0x3C` EntryPoint,
`+0x58` XexHeaderBase (a **copy of the XEX header in guest memory**, since XAM/kernel callers inspect it).

## Status by group

Legend: *implemented* = a strong `PPC_FUNC(__imp__X)` exists (file in brackets); *weak stub* = the generic one that
returns 0 and logs once. **A stub that "succeeds" without writing its outputs is the classic trap** (see
[stubs.md](stubs.md)): every one of the 77 stubs below was checked to be harmless *for SSA*; recheck for another game.


### C runtime and misc

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `__C_specific_handler` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `_vsnprintf` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `sprintf` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `vsprintf` | implemented (`kernel/printf.cpp`, formatting in `kernel/guest_format.cpp`) | Microsoft conversions (`%S` wide string, `I64`): the title's only caller names save packages ("Skylanders - Slot 1"). `sprintf`/`_vsnprintf`: stubs (only D3D and the movie player link them). |

### Debug and misc

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `DbgBreakPoint` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |
| `DbgPrint` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |
| `HalReturnToFirmware` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |

### Executive (Ex*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `ExCreateThread` | implemented (`kernel/threads.cpp`) | creates a guest thread (r3 handle*, r4 stack size or the XEX default, r5 id*, r6 XAPI startup thunk, r7 start, r8 context, r9 flags; bit0 suspended, bits 24-31 affinity mask). Host thread with a **16 MB** stack, own PCR/KTHREAD/TLS. Logs `[thread] N created`. |
| `ExGetXConfigSetting` | implemented (`kernel/xconfig.cpp`) | emulated North-American retail console: see [xam.md](xam.md#xconfig). |
| `ExRegisterTitleTerminateNotification` | **weak stub** (returns 0, logs `[stub]` once) | harmless: nothing to notify, no DPCs. Weak stub returns 0. |
| `ExTerminateThread` | implemented (`kernel/threads.cpp`) | throws a C++ `ThreadExit` caught at the host thread entry (no unwinding of guest frames). |

### Kernel (Ke*, Kf*, Ki*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `KeAcquireSpinLockAtRaisedIrql` | implemented (`kernel/sync.cpp`) | CAS on the lock word with the PCR (big-endian) as owner; spins with `yield`; returns old IRQL 0. |
| `KeBugCheck` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |
| `KeBugCheckEx` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |
| `KeDelayExecutionThread` | implemented (`kernel/threads.cpp`) | (mode, alertable, interval*): alertable = wait with the thread's APC queue (ends early with `STATUS_USER_APC` and runs APCs); zero interval = `Yield()` with back-off; else host sleep. **Zero-length sleeps are the job system's idle loop.** |
| `KeEnableFpuExceptions` | **weak stub** (returns 0, logs `[stub]` once) | harmless: nothing to notify, no DPCs. Weak stub returns 0. |
| `KeEnterCriticalRegion` | implemented (`kernel/sync.cpp`) | no kernel APCs / L2 lock here: no-ops. |
| `KeGetCurrentProcessType` | implemented (`kernel/system.cpp`) | always 1 (title process). |
| `KeInitializeDpc` | **weak stub** (returns 0, logs `[stub]` once) | harmless: nothing to notify, no DPCs. Weak stub returns 0. |
| `KeInsertQueueDpc` | **weak stub** (returns 0, logs `[stub]` once) | harmless: nothing to notify, no DPCs. Weak stub returns 0. |
| `KeLeaveCriticalRegion` | implemented (`kernel/sync.cpp`) | no kernel APCs / L2 lock here: no-ops. |
| `KeLockL2` | implemented (`kernel/sync.cpp`) | no kernel APCs / L2 lock here: no-ops. |
| `KeQueryBasePriorityThread` | implemented (`kernel/threads.cpp`) | scheduling hints ignored; the host scheduler decides (affinity: writes the old mask). |
| `KeQueryPerformanceFrequency` | implemented (`kernel/system.cpp`) | host counter frequency (`cntfrq_el0`, or TSC measured over 100 ms on x64): `mftb` is translated to the host counter. |
| `KeQuerySystemTime` | implemented (`kernel/system.cpp`) | writes the host time as FILETIME (100 ns since 1601). |
| `KeReleaseSpinLockFromRaisedIrql` | implemented (`kernel/sync.cpp`) | atomic store 0. |
| `KeResetEvent` | implemented (`kernel/sync.cpp`) | dispatcher-header events; header bound through the `SSAK` marker. `KeSetEvent` on a null header is tolerated (XAudio2 driver callback). |
| `KeSetAffinityThread` | implemented (`kernel/threads.cpp`) | scheduling hints ignored; the host scheduler decides (affinity: writes the old mask). |
| `KeSetBasePriorityThread` | implemented (`kernel/threads.cpp`) | scheduling hints ignored; the host scheduler decides (affinity: writes the old mask). |
| `KeSetCurrentProcessType` | implemented (`kernel/system.cpp`) | no-op. |
| `KeSetDisableBoostThread` | implemented (`kernel/threads.cpp`) | scheduling hints ignored; the host scheduler decides (affinity: writes the old mask). |
| `KeSetEvent` | implemented (`kernel/sync.cpp`) | dispatcher-header events; header bound through the `SSAK` marker. `KeSetEvent` on a null header is tolerated (XAudio2 driver callback). |
| `KeTlsAlloc` | implemented (`kernel/threads.cpp`) | dynamic TLS slots after the static `.tls` block (`TlsSlotAddress = *(PCR+0) + tlsDataSize + 4*index`), bitmap of 64 slots limited by the XEX `tlsSlotCount`. |
| `KeTlsFree` | implemented (`kernel/threads.cpp`) | dynamic TLS slots after the static `.tls` block (`TlsSlotAddress = *(PCR+0) + tlsDataSize + 4*index`), bitmap of 64 slots limited by the XEX `tlsSlotCount`. |
| `KeTlsGetValue` | implemented (`kernel/threads.cpp`) | dynamic TLS slots after the static `.tls` block (`TlsSlotAddress = *(PCR+0) + tlsDataSize + 4*index`), bitmap of 64 slots limited by the XEX `tlsSlotCount`. |
| `KeTlsSetValue` | implemented (`kernel/threads.cpp`) | dynamic TLS slots after the static `.tls` block (`TlsSlotAddress = *(PCR+0) + tlsDataSize + 4*index`), bitmap of 64 slots limited by the XEX `tlsSlotCount`. |
| `KeUnlockL2` | implemented (`kernel/sync.cpp`) | no kernel APCs / L2 lock here: no-ops. |
| `KeWaitForMultipleObjects` | implemented (`kernel/sync.cpp`) | `WaitForObjects` over one condition variable (`g_kernelSignal`). Alertable waits return `STATUS_USER_APC` (0xC0) and deliver user APCs. **`NtWaitForMultipleObjectsEx` with count 0 or > 64 fails at once with `STATUS_INVALID_PARAMETER_1`** (WideLoad). Timeout: negative = relative, positive = absolute system time. |
| `KeWaitForSingleObject` | implemented (`kernel/sync.cpp`) | `WaitForObjects` over one condition variable (`g_kernelSignal`). Alertable waits return `STATUS_USER_APC` (0xC0) and deliver user APCs. **`NtWaitForMultipleObjectsEx` with count 0 or > 64 fails at once with `STATUS_INVALID_PARAMETER_1`** (WideLoad). Timeout: negative = relative, positive = absolute system time. |
| `KfAcquireSpinLock` | implemented (`kernel/sync.cpp`) | see above. |
| `KfReleaseSpinLock` | implemented (`kernel/sync.cpp`) | atomic store 0. |
| `KiApcNormalRoutineNop` | **weak stub** (returns 0, logs `[stub]` once) | never reached in normal play; weak stub (return 0). A `[stub]` line here means a fatal path. |

### Memory manager (Mm*, Nt*Virtual*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `MmAllocatePhysicalMemoryEx` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md): page-size flags choose the 0xA/0xC/0xE window; E allocations reserve one extra page. |
| `MmFreePhysicalMemory` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md). `MmMapIoSpace` returns the guest address of the physical allocation. |
| `MmGetPhysicalAddress` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md). `MmMapIoSpace` returns the guest address of the physical allocation. |
| `MmMapIoSpace` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md). `MmMapIoSpace` returns the guest address of the physical allocation. |
| `MmQueryAddressProtect` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md). `MmMapIoSpace` returns the guest address of the physical allocation. |
| `MmQueryAllocationSize` | implemented (`kernel/memory.cpp`) | see [memory.md](memory.md). `MmMapIoSpace` returns the guest address of the physical allocation. |
| `NtAllocateVirtualMemory` | implemented (`kernel/memory.cpp`) | first-fit page heaps; MEM_DECOMMIT is ignored (pages stay reserved and keep their contents). |
| `NtFreeVirtualMemory` | implemented (`kernel/memory.cpp`) | first-fit page heaps; MEM_DECOMMIT is ignored (pages stay reserved and keep their contents). |
| `NtQueryVirtualMemory` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |

### Network (NetDll_*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `NetDll___WSAFDIsSet` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_bind` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_closesocket` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_connect` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_ioctlsocket` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_recv` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_recvfrom` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_select` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_send` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_sendto` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_setsockopt` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_socket` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_WSACleanup` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_WSAGetLastError` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_WSAStartup` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_XNetCleanup` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_XNetConnect` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetCreateKey` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetGetConnectStatus` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetGetTitleXnAddr` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_XNetInAddrToString` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetInAddrToXnAddr` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetRandom` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_XNetRegisterKey` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetServerToInAddr` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetStartup` | implemented (`kernel/net.cpp`) | offline network stack; see [xam.md](xam.md#network). |
| `NetDll_XNetTsAddrToInAddr` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetUnregisterInAddr` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetUnregisterKey` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |
| `NetDll_XNetXnAddrToInAddr` | **weak stub** (returns 0, logs `[stub]` once) | XNet key/address/select calls: not needed offline (the title never gets an address). |

### Nt* (objects, files, sync)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `NtCancelIoFile` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `NtCancelTimer` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `NtClearEvent` | implemented (`kernel/sync.cpp`) | set/clear a handle event; `NtSetEvent` returns the previous state. |
| `NtClose` | implemented (`kernel/sync.cpp`) | handle table (`g_handles`, handles start at 0x10000, step 4). Duplicate with close-source supported; 0xFFFFFFFE = current thread. |
| `NtCreateEvent` | implemented (`kernel/sync.cpp`) | (handle*, attrs, type, initial): type 0 = notification (manual reset), 1 = synchronization (auto reset). |
| `NtCreateFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtCreateMutant` | implemented (`kernel/sync.cpp`) | owner = KTHREAD, recursion count. |
| `NtCreateSemaphore` | implemented (`kernel/sync.cpp`) | count/maximum; release clamps to the maximum, returns the previous count. |
| `NtCreateTimer` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `NtDuplicateObject` | implemented (`kernel/sync.cpp`) | handle table (`g_handles`, handles start at 0x10000, step 4). Duplicate with close-source supported; 0xFFFFFFFE = current thread. |
| `NtFlushBuffersFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtOpenFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtPulseEvent` | implemented (`kernel/sync.cpp`) | real PulseEvent: releases only threads already waiting (all for manual-reset, one otherwise) through `pulseEpoch`. |
| `NtQueryDirectoryFile` | implemented (`kernel/files.cpp`) | one `X_FILE_DIRECTORY_INFORMATION` per call (Xenia's layout), file mask, restart flag on the stack (`r1+0x54`); `NO_SUCH_FILE` when nothing matches, then `NO_MORE_FILES`. |
| `NtQueryFullAttributesFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtQueryInformationFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtQueryVolumeInformationFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtReadFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtReadFileScatter` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `NtReleaseMutant` | implemented (`kernel/sync.cpp`) | owner = KTHREAD, recursion count. |
| `NtReleaseSemaphore` | implemented (`kernel/sync.cpp`) | count/maximum; release clamps to the maximum, returns the previous count. |
| `NtResumeThread` | implemented (`kernel/threads.cpp`) | decrements the suspend count under `g_kernelLock`, notifies; writes the previous count. |
| `NtSetEvent` | implemented (`kernel/sync.cpp`) | set/clear a handle event; `NtSetEvent` returns the previous state. |
| `NtSetInformationFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtSetTimerEx` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `NtWaitForMultipleObjectsEx` | implemented (`kernel/sync.cpp`) | `WaitForObjects` over one condition variable (`g_kernelSignal`). Alertable waits return `STATUS_USER_APC` (0xC0) and deliver user APCs. **`NtWaitForMultipleObjectsEx` with count 0 or > 64 fails at once with `STATUS_INVALID_PARAMETER_1`** (WideLoad). Timeout: negative = relative, positive = absolute system time. |
| `NtWaitForSingleObjectEx` | implemented (`kernel/sync.cpp`) | `WaitForObjects` over one condition variable (`g_kernelSignal`). Alertable waits return `STATUS_USER_APC` (0xC0) and deliver user APCs. **`NtWaitForMultipleObjectsEx` with count 0 or > 64 fails at once with `STATUS_INVALID_PARAMETER_1`** (WideLoad). Timeout: negative = relative, positive = absolute system time. |
| `NtWriteFile` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `NtYieldExecution` | implemented (`kernel/threads.cpp`) | `Yield()`: `std::this_thread::yield()` for the first 32 yields less than 100 us apart, then 200 us sleeps (back-off). |

### Object manager (Ob*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `ObCreateSymbolicLink` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `ObDeleteSymbolicLink` | implemented (`kernel/files.cpp`) | see [files_xdvdfs.md](files_xdvdfs.md). |
| `ObDereferenceObject` | implemented (`kernel/sync.cpp`) | no-op. |
| `ObReferenceObjectByHandle` | implemented (`kernel/sync.cpp`) | only threads have a guest-side object (their KTHREAD); others log `no guest object`. |

### Runtime library (Rtl*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `RtlCaptureContext` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `RtlCompareMemoryUlong` | implemented (`kernel/strings.cpp`) | repeated 32-bit pattern compare/fill (length rounded down to 4). |
| `RtlEnterCriticalSection` | implemented (`kernel/sync.cpp`) | owner = KTHREAD stored big-endian, CAS + `atomic_ref::wait/notify`; recursion counted in guest memory. |
| `RtlFillMemoryUlong` | implemented (`kernel/strings.cpp`) | repeated 32-bit pattern compare/fill (length rounded down to 4). |
| `RtlFreeAnsiString` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlImageXexHeaderField` | implemented (`kernel/module.cpp`) | walks the XEX optional headers in guest memory (key low byte 0/1 = inline value, else pointer). |
| `RtlInitAnsiString` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlInitializeCriticalSection` | implemented (`kernel/sync.cpp`) | writes the X_RTL_CRITICAL_SECTION: header type 1, LockCount -1 (+0x10), RecursionCount 0 (+0x14), owner 0 (+0x18). |
| `RtlInitializeCriticalSectionAndSpinCount` | implemented (`kernel/sync.cpp`) | writes the X_RTL_CRITICAL_SECTION: header type 1, LockCount -1 (+0x10), RecursionCount 0 (+0x14), owner 0 (+0x18). |
| `RtlInitUnicodeString` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlLeaveCriticalSection` | implemented (`kernel/sync.cpp`) | owner = KTHREAD stored big-endian, CAS + `atomic_ref::wait/notify`; recursion counted in guest memory. |
| `RtlMultiByteToUnicodeN` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlNtStatusToDosError` | implemented (`kernel/strings.cpp`) | table of the statuses the title meets; `0x8007xxxx` HRESULTs return their code, anything else 317. |
| `RtlRaiseException` | implemented (`kernel/threads.cpp`) | only the MSVC `SetThreadName` exception (0x406D1388) is expected: logs `[thread] N named "..."` and names the host thread. Any other code is logged and ignored. |
| `RtlTimeFieldsToTime` | implemented (`kernel/rtl_time.cpp`) | proleptic Gregorian from 1601-01-01 (a Monday); needed for save time stamps. |
| `RtlTimeToTimeFields` | implemented (`kernel/rtl_time.cpp`) | proleptic Gregorian from 1601-01-01 (a Monday); needed for save time stamps. |
| `RtlTryEnterCriticalSection` | implemented (`kernel/sync.cpp`) | owner = KTHREAD stored big-endian, CAS + `atomic_ref::wait/notify`; recursion counted in guest memory. |
| `RtlUnicodeStringToAnsiString` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlUnicodeToMultiByteN` | implemented (`kernel/strings.cpp`) | Latin-1 style narrowing/widening (chars above 0xFF become `?`); outputs written. |
| `RtlUnwind` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |

### Video driver (Vd*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `VdCallGraphicsNotificationRoutines` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdEnableDisableClockGating` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdEnableRingBufferRPtrWriteBack` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdGetCurrentDisplayGamma` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdGetCurrentDisplayInformation` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdGetSystemCommandBuffer` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdInitializeEngines` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdInitializeRingBuffer` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdInitializeScalerCommandBuffer` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdIsHSIOTrainingSucceeded` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdPersistDisplay` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdQueryVideoFlags` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdQueryVideoMode` | implemented (`kernel/video.cpp`) | 1280x720, progressive, widescreen, HD, 60 Hz, NTSC. |
| `VdRetrainEDRAM` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdRetrainEDRAMWorker` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdSetDisplayMode` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdSetDisplayModeOverride` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdSetGraphicsInterruptCallback` | implemented (`kernel/video.cpp`) | starts the **interrupt thread** (vblank every 16667 us) and stores callback/context. See [video_vblank.md](video_vblank.md). |
| `VdSetSystemCommandBufferGpuIdentifierAddress` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdShutdownEngines` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |
| `VdSwap` | **weak stub** (returns 0, logs `[stub]` once) | D3D's own init/ring calls: the game's D3D functions that call them are either hooked or only write device memory; nothing needs their result. |

### XAM (Xam*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XamAlloc` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamContentClose` | implemented (`kernel/xam_content.cpp`) | see [xam_content_saves.md](xam_content_saves.md). |
| `XamContentCreateEx` | implemented (`kernel/xam_content.cpp`) | see [xam_content_saves.md](xam_content_saves.md). |
| `XamContentGetDeviceData` | implemented (`kernel/xam_content.cpp`) | see [xam_content_saves.md](xam_content_saves.md). |
| `XamContentGetDeviceState` | implemented (`kernel/xam_content.cpp`) | see [xam_content_saves.md](xam_content_saves.md). |
| `XamCreateEnumeratorHandle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamEnumerate` | implemented (`kernel/xam_achievements.cpp`) | hands out the items of an `XamEnumerator` (`kernel/xam_enumerator.h`): the achievements of the disc's XDBF with their unlocks ([xam.md](xam.md#achievements)), the hard drive's packages for `XamContentCreateEnumerator` (Swap Force). |
| `XamFree` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamGetExecutionId` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamGetPrivateEnumStructureFromHandle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamGetSystemVersion` | implemented (`kernel/xconfig.cpp`) | AV pack 6 (HD), game region 0xFF (NA), language from the host (default English), system version 2.0.17559.0. |
| `XamInputGetCapabilities` | implemented (`kernel/input.cpp`) | user 0 only; see [input.md](input.md). |
| `XamInputGetState` | implemented (`kernel/input.cpp`) | user 0 only; see [input.md](input.md). |
| `XamInputSetState` | implemented (`kernel/input.cpp`) | user 0 only; see [input.md](input.md). |
| `XamLoaderLaunchTitle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamLoaderTerminateTitle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamNotifyCreateListener` | implemented (`kernel/xam.cpp`) | returns a waitable handle that is never signalled (Wwise init needs it). `XNotifyGetNext` returns FALSE. |
| `XamSessionCreateHandle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamSessionRefObjByHandle` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowDeviceSelectorUI` | implemented (`kernel/xam_content.cpp`) | see [xam_content_saves.md](xam_content_saves.md). |
| `XamShowDirtyDiscErrorUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowFriendsUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowKeyboardUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowMessageBoxUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowMessageBoxUIEx` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowMessagesUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamShowSigninUI` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamUserAreUsersFriends` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserCheckPrivilege` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserCreateAchievementEnumerator` | implemented (`kernel/xam_achievements.cpp`) | achievements: the list from the disc's XDBF and the kept unlocks; see [xam.md](xam.md#achievements). |
| `XamUserGetDeviceContext` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamUserGetName` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserGetSigninInfo` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserGetSigninState` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserGetXUID` | implemented (`kernel/xam_user.cpp`) | the launcher's profiles on users 0 and 1, by default one on user 0 (XUID 0xE000000000000001, name "Player"); see [xam.md](xam.md#profile). |
| `XamUserReadProfileSettings` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamVoiceClose` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamVoiceCreate` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamVoiceHeadsetPresent` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamVoiceIsActiveProcess` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XamVoiceSubmitPacket` | **weak stub** (returns 0, logs `[stub]` once) | UI and voice calls the title links but never needs offline. |
| `XNotifyGetNext` | implemented (`kernel/xam.cpp`) | FALSE: nothing pending. |

### XAudio

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XAudioGetDuckerLevel` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XAudioGetSpeakerConfig` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XAudioGetVoiceCategoryVolume` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XAudioRegisterRenderDriverClient` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XAudioSubmitRenderDriverFrame` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XAudioUnregisterRenderDriverClient` | implemented (`kernel/audio.cpp`) | see [audio_xma_xaudio.md](audio_xma_xaudio.md). |

### XEX loader (Xex*, RtlImageXexHeaderField)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XexCheckExecutablePrivilege` | implemented (`kernel/module.cpp`) | bit test of the XEX system flags. |
| `XexGetModuleHandle` | implemented (`kernel/module.cpp`) | only `xam.xex` / `xboxkrnl.exe` exist: LDR entry with a fake DllBase (0x81000000 / 0x80040000). Any other module: `STATUS_NO_SUCH_FILE`. |
| `XexGetProcedureAddress` | implemented (`kernel/module.cpp`) | hands out **host thunks** (`CreateHostThunk`) for xam ordinals 0x4A1/0x4A2 (portal); anything else: `STATUS_ORDINAL_NOT_FOUND` (0xC0000262) and a log line. |
| `XexLoadImage` | implemented (`kernel/module.cpp`) | only `xam.xex` / `xboxkrnl.exe` exist: LDR entry with a fake DllBase (0x81000000 / 0x80040000). Any other module: `STATUS_NO_SUCH_FILE`. |
| `XexUnloadImage` | implemented (`kernel/module.cpp`) | only `xam.xex` / `xboxkrnl.exe` exist: LDR entry with a fake DllBase (0x81000000 / 0x80040000). Any other module: `STATUS_NO_SUCH_FILE`. |

### Console configuration (XGet*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XGetAVPack` | implemented (`kernel/xconfig.cpp`) | AV pack 6 (HD), game region 0xFF (NA), language from the host (default English), system version 2.0.17559.0. |
| `XGetGameRegion` | implemented (`kernel/xconfig.cpp`) | AV pack 6 (HD), game region 0xFF (NA), language from the host (default English), system version 2.0.17559.0. |
| `XGetLanguage` | implemented (`kernel/xconfig.cpp`) | AV pack 6 (HD), game region 0xFF (NA), language from the host (default English), system version 2.0.17559.0. |
| `XGetVideoMode` | implemented (`kernel/video.cpp`) | 1280x720, progressive, widescreen, HD, 60 Hz, NTSC. |

### XMA

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XMACreateContext` | implemented (`kernel/xma.cpp`) | 320 contexts of 64 bytes in physical memory (64 KB window); see [audio_xma_xaudio.md](audio_xma_xaudio.md). |
| `XMAReleaseContext` | implemented (`kernel/xma.cpp`) | 320 contexts of 64 bytes in physical memory (64 KB window); see [audio_xma_xaudio.md](audio_xma_xaudio.md). |

### XAM messages (XMsg*)

| Import | Status | Behaviour and our shortcuts |
|---|---|---|
| `XMsgCancelIORequest` | **weak stub** (returns 0, logs `[stub]` once) | never needed so far. |
| `XMsgInProcessCall` | implemented (`kernel/xmp.cpp`) | XMP (app 0xFA) answers; see [xam.md](xam.md#xmsg-and-xmp). |
| `XMsgStartIORequest` | implemented (`kernel/xam_achievements.cpp`) | achievements: an unlock is logged, kept in the data folder and shown as a notice; see [xam.md](xam.md#achievements). |
| `XMsgStartIORequestEx` | implemented (`kernel/xmp.cpp`) | XMP (app 0xFA) answers; see [xam.md](xam.md#xmsg-and-xmp). |

## Giants' extra imports (33, all implemented) [Xbox] [Giants]

| Group | Imports | File |
|---|---|---|
| Pool allocator | `ExAllocatePoolTypeWithTag`, `ExAllocatePoolWithTag`, `ExFreePool` | `kernel/imports_giants.cpp` |
| I/O manager, devices | `IoCreateDevice`, `IoDeleteDevice`, `IoCompleteRequest`, `IoInvalidDeviceRequest`, `IoCheckShareAccess`, `IoSetShareAccess`, `IoRemoveShareAccess`, `MicDeviceRequest`, `RmcDeviceRequest`, `StfsCreateDevice`, `StfsControlDevice` | `kernel/imports_giants.cpp` |
| APCs, objects | `KeInitializeApc`, `KeInsertQueueApc`, `ObIsTitleObject`, `ObReferenceObject` | `kernel/imports_giants.cpp` |
| Loader, crypto | `XamLoaderGetLaunchData`, `XamLoaderGetLaunchDataSize`, `XeCryptSha` | `kernel/imports_giants.cpp` |
| Network (offline) | `NetDll_accept`, `NetDll_getsockopt`, `NetDll_inet_addr`, `NetDll_listen`, `NetDll_XNetGetEthernetLinkStatus` | `kernel/net.cpp` |
| Files | `NtWriteFileGather` | `kernel/files.cpp` |
| Strings, printf | `RtlCompareStringN`, `RtlUpcaseUnicodeChar`, `_snprintf` | `kernel/strings.cpp`, `kernel/printf.cpp` |
| XAM | `XamContentDelete`, `XamContentGetCreator` (`kernel/xam_content.cpp`), `XamInputGetKeystrokeEx` (`kernel/input.cpp`) | |

Calls in `imports_giants.cpp` whose arguments are not documented log their first use as `[kernel] Name (lr=.. r3..r9)`.

## Swap Force's extra imports (44, all implemented) [Xbox] [SwapForce]

The console is offline with one local profile: network calls fail as with no cable, Live calls as if not signed in to
Live, UI calls show nothing. The why of each choice: [games/swapforce/docs/boot.md](../../../games/swapforce/docs/boot.md#imports-2026-10-26-xbox-swapforce).

| Group | Imports | Behaviour | File |
|---|---|---|---|
| WSA events | `NetDll_WSACreateEvent`, `NetDll_WSACloseEvent`, `NetDll_WSAResetEvent` | manual-reset `Event` handles (no caller argument, as Xenia) | `kernel/net.cpp` |
| XNet | `NetDll_XNetDnsLookup`, `NetDll_XNetDnsRelease`, `NetDll_XNetXnAddrToMachineId` | DNS completes at once, failed (`XNDNS.iStatus` = WSAENETDOWN, event signaled); machine id 0, WSAENETDOWN | `kernel/net.cpp` |
| XHttp | `NetDll_XHttpStartup`, `Shutdown`, `Open`, `Connect`, `OpenRequest`, `SendRequest`, `ReceiveResponse`, `QueryHeaders`, `ReadData`, `WriteData`, `CloseHandle`, `SetStatusCallback`, `DoWork`, `CrackUrl` | handles open (local); `SendRequest` fails at once with ERROR_WINHTTP_NAME_NOT_RESOLVED (12007), later calls with ERROR_WINHTTP_INCORRECT_HANDLE_STATE; `CrackUrl` fails (ERROR_WINHTTP_INVALID_URL); the last error goes to KTHREAD+0x160 (XAPI `GetLastError`) | `kernel/net.cpp` |
| XAuth, tokens | `XampXAuthStartup`, `XampXAuthShutdown`, `XampXAuthGetTitleBuffer`, `XamGetToken`, `XamFreeToken` | XAuth fails to start (0x80158406, Xenia's code); no token (out cleared, ERROR_NOT_LOGGED_ON) | `kernel/imports_swapforce.cpp` |
| Live UI | `XamShowGamerCardUIForXUID`, `XamShowMarketplaceUI`, `XamShowPlayerReviewUI` | nothing shown, ERROR_SUCCESS (no XOVERLAPPED in their signatures) | `kernel/imports_swapforce.cpp` |
| Notifications | `XNotifyPositionUI` | ignored | `kernel/xam.cpp` |
| XAM tasks | `XamTaskSchedule`, `XamTaskCloseHandle`, `XamTaskShouldExit` | callback(message) on a new guest thread at once (as Xenia), its handle out; close; never asked to exit | `kernel/imports_swapforce.cpp` |
| Gamer pictures | `XamParseGamerTileKey`, `XamReadTileToTexture` | ids 0, ERROR_INVALID_PARAMETER; the profile's picture for a signed-in user's gamer tile (types 2-5), else texture cleared and ERROR_NOT_FOUND, through the XOVERLAPPED | `kernel/imports_swapforce.cpp`, `kernel/xam_user.cpp` |
| Console key | `XeKeysConsolePrivateKeySign`, `XeKeysConsoleSignatureVerification` | Xenia's fixed retail certificate plus the hash as "signature"; verification matches what this console signed | `kernel/imports_swapforce.cpp` |
| Content, profile | `XamContentCreateEnumerator` (`kernel/xam_content.cpp`), `XamUserWriteProfileSettings` (`kernel/xam_user.cpp`) | lists the type's packages on the hard drive (XCONTENT_DATA 0x134 each); settings accepted, not kept | |
| Loader | `XamLoaderSetLaunchData` | stored (at most 0x400 bytes) where `XamLoaderGetLaunchData(Size)` read it | `kernel/imports_giants.cpp` |
| Memory | `MmQueryStatistics` | 104-byte struct (size first, checked): 512 MB, title pages from the guest heaps' allocations | `kernel/memory.cpp` |
| Files, volumes | `IoDismountVolume`, `IoDismountVolumeByFileHandle`, `FscSetCacheElementCount`, `NtDeviceIoControlFile` | success; I/O controls fail with STATUS_INVALID_DEVICE_REQUEST (logged once) | `kernel/files.cpp` |

## Trap Team's extra imports (5) [Xbox] [TrapTeam]

All five are in `kernel/imports_trapteam.cpp`; each writes its outputs, logs its first call with the registers, and (with `SKY_ALLOC_LOG=1`) every call
with its result. Counts are from one run to the first level (2026-10-30), through the title, the autosave notice, the movie and `level_313_tutorial`'s load [V].

| Import | What our runtime does | What the game does with it |
|---|---|---|
| `NetDll_XNetQosServiceLookup` | **stub**: `*out = 0`, returns WSAENETDOWN (10050), as with no cable | never called in the run |
| `NetDll_XNetQosRelease` | **stub**: returns 0 (nothing was ever handed out) | never called in the run |
| `ObLookupThreadByThreadId` | **stub**: `*out = 0`, returns STATUS_NOT_FOUND (`0xC0000225`). A real lookup (thread id to its KTHREAD) needs a registry of guest threads by id, which we do not keep | called 24 times, twice per new thread id (`0x2F` to `0x3B`), from guest thread 31 (`BinkRequestThread`) while a level loads, then `ObReferenceObjectByHandle(0)` (logged "no guest object"); the game carries on, no effect seen in the run [?] (a thread's affinity or priority is the guess) |
| `ObOpenObjectByPointer` | **stub**: returns STATUS_NOT_FOUND, no handle written | never called in the run |
| `XamGetLocaleEx` | **stub**: returns 0, arguments unknown (`r3=0x6F r4=0x2B r5=0x351C r6=0xD47` seen) | called once, at start-up, from `sub_821D2A7C`; the title goes on, no locale-dependent text seen wrong yet [?] |

None of the five returns a null that the game then uses: the fault of the first level came from the game's own video-pool allocator, not from an import ([boot.md](../../../games/trapteam/docs/boot.md)).

## Summary

- 295 imports: **218 implemented**, **77 weak stubs** (+2 ordinals served by thunks). Recount:
  `grep -rhoE 'PPC_FUNC\(__imp__\w+\)' runtime` against `import_list.inc`.
- The set that must be exact for a title to boot (each was a real bug): `KeTimeStampBundle` ticking, dispatcher-header
  binding, `NtPulseEvent`, user APCs, zero-count waits failing, back-off on yields, `ExGetXConfigSetting` +
  `XamGetSystemVersion` outputs, `XNetGetTitleXnAddr` = NONE, `XamNotifyCreateListener` handle, XMP answers,
  `XamShowDeviceSelectorUI` + `XamContentCreateEx` (overlapped at r1+0x54), `vsprintf`, `RtlTimeToTimeFields`, the
  achievement enumerator handle, `MmAllocatePhysicalMemoryEx` windows, vblank interrupts with the status bit.
- In the list but stubbed, plausible in later games (check on day 1): `XamLoaderLaunchTitle` (already stubbed),
  `XamUserReadProfileSettings` (stubbed), `XamVoice*` (stubbed),
  Live/party/session calls (`XamSession*` stubbed).
