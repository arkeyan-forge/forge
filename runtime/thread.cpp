#include "thread.h"
#include "loader.h"
#include "memory.h"
#include "platform/clock.h"
#include "platform/platform.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

namespace
{
    // The guest context of this host thread, for the crash report (PrintGuestLocation).
    thread_local const PPCContext* t_guestContext;

    constexpr uint8_t ThreadObjectType = 6;
    constexpr size_t HostStackSize = 16 * 1024 * 1024; // recompiled calls nest on the host stack

    struct ThreadExit
    {
        uint32_t code;
    };

    struct StartInfo
    {
        std::shared_ptr<GuestThread> thread;
        uint32_t start;
        uint32_t context;
        uint32_t xapiStartup;
    };

    std::atomic<uint32_t> g_nextThreadId{ 1 };
    std::atomic<uint32_t> g_threadsRunning;

    thread_local GuestThread* t_guestThread;
    std::mutex g_liveLock;
    std::vector<std::shared_ptr<GuestThread>> g_live; // started, not exited: the stall report

    void Forget(const GuestThread& thread)
    {
        std::lock_guard lock(g_liveLock);
        std::erase_if(g_live, [&](const auto& live) { return live.get() == &thread; });
    }

    // " \"name\"" when the title named the thread, else nothing. Call with g_kernelLock held.
    std::string QuotedName(const GuestThread& thread)
    {
        return thread.name.empty() ? std::string() : " \"" + thread.name + "\"";
    }

    // KPCR fields read by game code through r13.
    constexpr uint32_t PcrTlsPointer = 0x0;
    constexpr uint32_t PcrSelf = 0x30;
    constexpr uint32_t PcrStackBase = 0x70;
    constexpr uint32_t PcrStackLimit = 0x74;
    constexpr uint32_t PcrCurrentThread = 0x100;
    constexpr uint32_t PcrCurrentCpu = 0x10C;
    constexpr uint32_t PcrSize = 0x2D8;

    constexpr uint32_t KThreadThreadId = 0x14C;
    constexpr uint32_t KThreadSize = 0xAB0;

    // A thread's guest blocks come from the system heap, which never frees: about 1000 stacks filled it. A thread that
    // ends gives them back here, and a new thread with the same stack size takes them. Its KTHREAD is reused only after
    // QuarantinedBlocks other threads of that size have ended: a title may still wait on the KTHREAD of a thread that
    // just ended (ObReferenceObjectByHandle), and must see that thread, not a new one.
    struct ThreadBlocks
    {
        uint32_t pcr, kthread, tls, stack;
    };
    constexpr size_t QuarantinedBlocks = 8;
    std::mutex g_blocksLock;
    std::map<uint32_t, std::deque<ThreadBlocks>> g_endedBlocks; // stack size -> blocks, oldest first

    uint32_t TlsSize() { return g_module.tlsDataSize + g_module.tlsSlotCount * 4; }

    ThreadBlocks TakeBlocks(uint32_t stackSize)
    {
        ThreadBlocks blocks{};
        bool reused = false;
        {
            std::lock_guard lock(g_blocksLock);
            auto ended = g_endedBlocks.find(stackSize);
            if (ended != g_endedBlocks.end() && ended->second.size() > QuarantinedBlocks)
            {
                blocks = ended->second.front();
                ended->second.pop_front();
                reused = true;
            }
        }
        if (reused)
        {
            // As SystemAlloc gives them: zeroed.
            memset(g_memory.Translate(blocks.pcr), 0, PcrSize);
            memset(g_memory.Translate(blocks.kthread), 0, KThreadSize);
            memset(g_memory.Translate(blocks.tls), 0, TlsSize());
            memset(g_memory.Translate(blocks.stack), 0, stackSize);
            return blocks;
        }
        blocks.pcr = g_memory.SystemAlloc(PcrSize);
        blocks.kthread = g_memory.SystemAlloc(KThreadSize);
        blocks.tls = g_memory.SystemAlloc(TlsSize());
        blocks.stack = g_memory.SystemAlloc(stackSize, 0x1000);
        return blocks;
    }

    void GiveBackBlocks(const GuestThreadContext& context)
    {
        std::lock_guard lock(g_blocksLock);
        g_endedBlocks[context.stackSize].push_back({ context.pcr, context.kthread, context.tls, context.stack });
    }

    void ScheduleContext(PPCContext& ctx, netplay::Scheduler::Id id)
    {
        if (!netplay::Cooperative())
            return;
        auto& scheduler = netplay::GuestScheduler();
        scheduler.Enter(id);
        // The watchdog reads only a copy captured by the holder at a handover, never live guest registers.
        auto snapshot = std::make_shared<PPCContext>(ctx);
        scheduler.Diagnose([snapshot] { PrintGuestLocation(snapshot.get()); }, [snapshot, &ctx] { *snapshot = ctx; });
    }

    void HostThreadMain(const StartInfo* info)
    {
        GuestThread& thread = *info->thread;
        if (!netplay::Cooperative())
        {
            std::unique_lock lock(g_kernelLock);
            g_kernelSignal.wait(lock, [&] { return thread.suspendCount == 0; });
        }

        PPCContext& ctx = thread.context->ctx;
        t_guestContext = &ctx;
        t_guestThread = &thread;
        ScheduleContext(ctx, thread.scheduleId);
        ctx.fpscr.loadFromHost();
        g_threadsRunning.fetch_add(1, std::memory_order_relaxed);

        uint32_t function = info->start;
        if (info->xapiStartup != 0)
        {
            ctx.r3.u64 = info->start;
            ctx.r4.u64 = info->context;
            function = info->xapiStartup;
        }
        else
        {
            ctx.r3.u64 = info->context;
        }

        uint32_t exitCode;
        try
        {
            CallGuest(ctx, function);
            exitCode = ctx.r3.u32;
        }
        catch (const ThreadExit& exit)
        {
            exitCode = exit.code;
        }
        g_threadsRunning.fetch_sub(1, std::memory_order_relaxed);
        Forget(thread);

        {
            std::lock_guard lock(g_kernelLock);
            fprintf(stderr, "[thread] %u%s exited, code 0x%08X\n", thread.id, QuotedName(thread).c_str(), exitCode);
            thread.exited = true;
            thread.exitCode = exitCode;
            thread.SyncHeader();
            thread.WakeSleepers();
            // Still under the lock: once a waiter sees the exit, the blocks are free. A later wait on this thread's handle
            // no longer mirrors its state into the KTHREAD, which another thread may have by then.
            thread.header = 0;
            GiveBackBlocks(*thread.context);
        }
        if (netplay::Cooperative())
            netplay::GuestScheduler().Exit();
    }
}

GuestThreadContext::GuestThreadContext(uint32_t stackBytes, uint32_t threadId, uint8_t cpu) : stackSize(stackBytes)
{
    ThreadBlocks blocks = TakeBlocks(stackSize);
    pcr = blocks.pcr;
    kthread = blocks.kthread;
    tls = blocks.tls;
    stack = blocks.stack;

    // __declspec(thread) data (initialised from .tls) is addressed directly from the TLS pointer;
    // the dynamic slots follow it.
    memcpy(g_memory.Translate(tls), g_memory.Translate(g_module.tlsRawData), g_module.tlsRawDataSize);
    tlsSlots = tls + g_module.tlsDataSize;

    uint32_t stackTop = stack + stackSize;

    g_memory.Write<uint32_t>(pcr + PcrTlsPointer, tls);
    g_memory.Write<uint32_t>(pcr + PcrSelf, pcr);
    g_memory.Write<uint32_t>(pcr + PcrStackBase, stackTop);
    g_memory.Write<uint32_t>(pcr + PcrStackLimit, stack);
    g_memory.Write<uint32_t>(pcr + PcrCurrentThread, kthread);
    g_memory.Write<uint8_t>(pcr + PcrCurrentCpu, cpu);
    g_memory.Write<uint32_t>(kthread + KThreadThreadId, threadId);

    ctx.r1.u64 = stackTop;
    ctx.r13.u64 = pcr;
}

std::shared_ptr<GuestThread> CreateGuestThread(
    uint32_t start, uint32_t context, uint32_t xapiStartup, uint32_t stackSize, bool suspended, uint8_t cpu)
{
    auto thread = std::make_shared<GuestThread>();
    thread->id = g_nextThreadId++;
    thread->context = std::make_unique<GuestThreadContext>(stackSize, thread->id, cpu);
    thread->suspendCount = suspended ? 1 : 0;
    if (netplay::Cooperative())
        thread->scheduleId = netplay::GuestScheduler().Register(!suspended);

    // Waits on the KTHREAD (KeWaitForSingleObject) resolve to this object.
    g_memory.Write<uint8_t>(thread->context->kthread, ThreadObjectType);
    RegisterDispatcher(thread->context->kthread, thread);

    {
        std::lock_guard lock(g_liveLock);
        g_live.push_back(thread);
    }
    PlatformStartThread(
        HostStackSize, [info = StartInfo{ thread, start, context, xapiStartup }] { HostThreadMain(&info); });

    return thread;
}

void ExitGuestThread(uint32_t exitCode) { throw ThreadExit{ exitCode }; }

uint32_t GuestThreadsRunning() { return g_threadsRunning.load(std::memory_order_relaxed); }

GuestWaitScope::GuestWaitScope(
    GuestWait::Kind kind, uint32_t target, uint32_t count, const PPCContext& ctx, uint32_t owner)
    : m_thread(t_guestThread)
{
    if (m_thread == nullptr)
        return;
    // The kernel call usually comes through the title's wait wrapper, so lr names the wrapper; the wrapper saved its own
    // return address 8 bytes below its caller's frame, which the back chain at r1 points to.
    const GuestThreadContext& thread = *m_thread->context;
    auto onStack = [&](uint32_t address) {
        return address >= thread.stack && address < thread.stack + thread.stackSize;
    };
    uint32_t outer = 0;
    if (onStack(ctx.r1.u32))
        if (const uint32_t chain = g_memory.Read<uint32_t>(ctx.r1.u32); onStack(chain - 8) && onStack(chain - 5))
            outer = g_memory.Read<uint32_t>(chain - 8);
    GuestWait& wait = m_thread->wait;
    wait.kind.store(kind, std::memory_order_relaxed);
    wait.target.store(target, std::memory_order_relaxed);
    wait.count.store(count, std::memory_order_relaxed);
    wait.caller.store(uint32_t(ctx.lr), std::memory_order_relaxed);
    wait.outerCaller.store(outer, std::memory_order_relaxed);
    wait.owner.store(owner, std::memory_order_relaxed);
    wait.since.store(std::max<uint64_t>(PlatformNowNs(), 1), std::memory_order_release);
}

GuestWaitScope::~GuestWaitScope()
{
    if (m_thread != nullptr)
        m_thread->wait.since.store(0, std::memory_order_relaxed);
}

void NameGuestThread(uint32_t id, const char* name)
{
    std::lock_guard live(g_liveLock);
    for (const auto& thread : g_live)
        if (thread->id == id)
        {
            std::lock_guard lock(g_kernelLock);
            thread->name = name;
        }
}

void PrintGuestThreads(const char* tag)
{
    std::vector<std::shared_ptr<GuestThread>> threads;
    {
        std::lock_guard lock(g_liveLock);
        threads = g_live;
    }
    const uint64_t now = PlatformNowNs();
    for (const auto& thread : threads)
    {
        std::string name;
        {
            std::lock_guard lock(g_kernelLock);
            name = QuotedName(*thread);
        }
        const GuestWait& wait = thread->wait;
        const uint64_t since = wait.since.load(std::memory_order_acquire);
        if (since == 0)
        {
            fprintf(stderr, "%s thread %u%s: not in a wait (running, sleeping or yielding)\n", tag, thread->id,
                name.c_str());
            continue;
        }
        const double seconds = now > since ? double(now - since) / 1e9 : 0.0;
        const uint32_t target = wait.target.load(std::memory_order_relaxed);
        char what[96];
        if (wait.kind.load(std::memory_order_relaxed) == GuestWait::CriticalSection)
            snprintf(what, sizeof(what), "critical section 0x%08X held by thread %u", target,
                wait.owner.load(std::memory_order_relaxed));
        else
        {
            const uint32_t count = wait.count.load(std::memory_order_relaxed);
            snprintf(what, sizeof(what), "%u object%s (first 0x%08X)", count, count == 1 ? "" : "s", target);
        }
        fprintf(stderr,
            "%s thread %u%s: waiting for %s, %.1f s so far, called from 0x%08X (itself called from 0x%08X)\n", tag,
            thread->id, name.c_str(), what, seconds, wait.caller.load(std::memory_order_relaxed),
            wait.outerCaller.load(std::memory_order_relaxed));
    }
}

std::shared_ptr<GuestThread> CurrentGuestThread(uint32_t pcr)
{
    return std::dynamic_pointer_cast<GuestThread>(LookupDispatcher(CurrentKThread(pcr)));
}

void QueueUserApc(GuestThread& thread, const UserApc& apc)
{
    {
        std::lock_guard lock(g_kernelLock);
        thread.apcs.push_back(apc);
        if (thread.apcs.sleeper != nullptr)
            thread.apcs.sleeper->notify_one();
    }
    if (netplay::Cooperative())
        netplay::GuestScheduler().Signal();
}

void DeliverUserApcs(PPCContext& ctx, GuestThread& thread)
{
    while (true)
    {
        UserApc apc;
        {
            std::lock_guard lock(g_kernelLock);
            if (thread.apcs.empty())
                return;
            apc = thread.apcs.front();
            thread.apcs.pop_front();
        }

        PPCContext saved = ctx;
        ctx.r3.u64 = apc.context;
        ctx.r4.u64 = apc.argument1;
        ctx.r5.u64 = apc.argument2;
        CallGuest(ctx, apc.routine);
        ctx = saved;
    }
}

void StartHostDrivenThread(std::function<void(PPCContext&)> body, uint8_t cpu)
{
    auto* context = new GuestThreadContext(0x10000, g_nextThreadId++, cpu);
    const auto id = netplay::Cooperative() ? netplay::GuestScheduler().Register() : netplay::Scheduler::None;
    PlatformStartThread(HostStackSize, [body = std::move(body), context, id] {
        t_guestContext = &context->ctx;
        ScheduleContext(context->ctx, id);
        context->ctx.fpscr.loadFromHost();
        body(context->ctx);
        if (netplay::Cooperative())
            netplay::GuestScheduler().Exit();
    });
}

void PrintGuestLocation() { PrintGuestLocation(t_guestContext); }

void PrintGuestLocation(const PPCContext* ctx)
{
    // Called from the fault handler, on the thread that faulted. The registers are those the recompiled code last stored:
    // a value it kept in a host register is not here, so lr and ctr name the last call that went through the context.
    if (ctx == nullptr)
    {
        fprintf(stderr, "[crash] where: not on a guest thread (a host thread: video, audio or the main loop)\n");
        return;
    }
    uint32_t pcr = ctx->r13.u32;
    uint32_t threadId = pcr != 0 ? g_memory.Read<uint32_t>(CurrentKThread(pcr) + 0x14C) : 0;
#if defined(PPC_CONFIG_CTR_AS_LOCAL) && defined(PPC_CONFIG_NON_VOLATILE_AS_LOCAL)
    // ctr and r31 are the translated functions' locals (docs/tools/toolchain/xenonrecomp.md): not in the context.
    fprintf(stderr, "[crash] where: guest thread %u, lr 0x%08X, r1 0x%08X, r3 0x%08X\n", threadId, uint32_t(ctx->lr),
        ctx->r1.u32, ctx->r3.u32);
#else
    fprintf(stderr, "[crash] where: guest thread %u, lr 0x%08X, ctr 0x%08X, r1 0x%08X, r3 0x%08X, r31 0x%08X\n",
        threadId, uint32_t(ctx->lr), ctx->ctr.u32, ctx->r1.u32, ctx->r3.u32, ctx->r31.u32);
#endif
}
