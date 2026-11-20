#pragma once

#include "kernel/objects.h"
#include "netplay/scheduler.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <ppc_config.h>
#include <ppc_context.h>

// What a console thread sees: r13 points at its PCR (TLS pointer, current KTHREAD, CPU number),
// plus its KTHREAD, TLS block and stack, all in guest memory.
struct GuestThreadContext
{
    PPCContext ctx{};
    uint32_t pcr{};
    uint32_t kthread{};
    uint32_t tls{};
    uint32_t tlsSlots{}; // dynamic TLS slots (KeTlsGetValue / KeTlsSetValue)
    uint32_t stack{};
    uint32_t stackSize{};

    GuestThreadContext(uint32_t stackBytes, uint32_t threadId, uint8_t cpu);
};

// What a guest thread waits for, for the stall report (PrintGuestThreads): written by the thread itself, read by any.
struct GuestWait
{
    enum Kind : uint32_t
    {
        Objects,
        CriticalSection,
    };
    std::atomic<uint64_t> since{ 0 }; // PlatformNowNs() at the wait's start, 0 when not waiting
    std::atomic<uint32_t> kind{}, target{}, count{}, caller{}, outerCaller{}, owner{};
};

// A guest thread as a waitable kernel object, signaled once it exits. Fields but `wait` are guarded by g_kernelLock.
struct GuestThread : KernelObject
{
    std::unique_ptr<GuestThreadContext> context;
    uint32_t id{};
    netplay::Scheduler::Id scheduleId{};
    uint32_t suspendCount{};
    bool exited{};
    uint32_t exitCode{};
    ApcQueue apcs;
    std::string name; // the title's SetThreadName
    GuestWait wait;

    bool IsSignaled(uint32_t) const override { return exited; }
    int32_t SignalState() const override { return exited; }
};

// Runs `start(context)`, or `xapiStartup(start, context)` when given, on a new host thread.
std::shared_ptr<GuestThread> CreateGuestThread(
    uint32_t start, uint32_t context, uint32_t xapiStartup, uint32_t stackSize, bool suspended, uint8_t cpu);

// Unwinds the calling guest thread back to its host entry (ExTerminateThread).
[[noreturn]] void ExitGuestThread(uint32_t exitCode);

// Marks the calling guest thread as waiting while it lives: for `target` (the first object's dispatcher header or
// handle, or the critical section) and `count` objects; `ctx` at the kernel call gives where from (lr, and the return
// address its caller saved, through the stack's back chain); `owner` is the id of the thread holding a critical section.
// Nothing on a host-driven thread.
class GuestWaitScope
{
public:
    GuestWaitScope(GuestWait::Kind kind, uint32_t target, uint32_t count, const PPCContext& ctx, uint32_t owner = 0);
    ~GuestWaitScope();
    GuestWaitScope(const GuestWaitScope&) = delete;
    GuestWaitScope& operator=(const GuestWaitScope&) = delete;

private:
    GuestThread* m_thread;
};

// Names the live guest thread `id` in the stall report and its exit line.
void NameGuestThread(uint32_t id, const char* name);

// One line per live guest thread on stderr, each starting with `tag`: its name, what it waits for, since when, and from
// where. For a one-off report, never per frame.
void PrintGuestThreads(const char* tag);

// The GuestThread whose PCR is `pcr`, or null for host-driven threads.
std::shared_ptr<GuestThread> CurrentGuestThread(uint32_t pcr);

// Queues a user-mode APC to `thread` and wakes it if it waits alertably.
void QueueUserApc(GuestThread& thread, const UserApc& apc);

// Runs the APCs queued to the calling thread, preserving its guest registers.
void DeliverUserApcs(PPCContext& ctx, GuestThread& thread);

// True for a word-aligned address inside the recompiled code: the indirect-call guard's test
// (ArkeyanRecompLib/indirect_call_guard.h). The function table read at any other address gives a garbage host pointer.
bool GuestCodeAddress(uint32_t target);

// Calls the guest function at `target` from host code (a thread's start, an APC, an interrupt or audio callback). A target
// outside the code is logged once and not called: r3 = 0, and the result is false.
bool CallGuest(PPCContext& ctx, uint32_t target);

// The guest threads running now (started, not exited): the debug screen.
uint32_t GuestThreadsRunning();

// Runs host code that calls into the guest (interrupts, timers) on its own thread with a guest context.
void StartHostDrivenThread(std::function<void(PPCContext&)> body, uint8_t cpu);

// The crash report's "[crash] where:" line for the calling thread: its guest thread id and last stored lr, ctr, r1, r3, r31
// (the guest function itself is named by the host stack that follows, `sub_XXXXXXXX`). Safe to call from a fault handler.
void PrintGuestLocation();
void PrintGuestLocation(const PPCContext* ctx);
