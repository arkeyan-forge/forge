// The operating system's context switches on the host (docs/runtime/wii/threads.md). The SDK's scheduler runs as the
// game's code: it keeps its queues, picks the next thread by priority and calls OSSaveContext on the current one, then
// OSLoadContext on the next. Those two are the host's: a context is a host thread, and loading one hands it the
// processor (wii::Processor). A context loaded for the first time (a new thread) gets a host thread that starts at its
// saved address with its saved registers.
#include "wii/os.h"
#include "wii/processor.h"
#include "ppc_recomp_shared.h"
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <utility>

namespace wii::os
{
    namespace
    {
        // The OSContext's fields [the SDK's own: OSInitContext and OSCreateThread write them there].
        constexpr uint32_t ContextGpr = 0x000, ContextCr = 0x080, ContextLr = 0x084, ContextCtr = 0x088,
                           ContextXer = 0x08C, ContextFpr = 0x090, ContextSrr0 = 0x198, ContextGqr = 0x1A4,
                           ContextPs1 = 0x1C8;
        constexpr size_t GuestThreadStack = 8 << 20; // the host's, for the translated code's calls
        constexpr uint32_t CurrentContext = 0x800000D4; // the operating system's, in low memory
        thread_local PPCContext* t_interrupted = nullptr; // the holder's registers while its events run
        thread_local uint32_t t_interruptContext = 0;     // while CallGuest runs a handler: the context it was given

        // An exception handler returns by loading the context it interrupted (OSLoadContext): here, the unwinding of
        // its frames back to CallGuest.
        struct InterruptReturn
        {};
        thread_local bool t_ranGuest = false;

        Processor& TheProcessor()
        {
            static Processor& processor = *new Processor; // outlives every host thread waiting on it
            return processor;
        }

        // Only the holder touches these.
        std::unordered_map<uint32_t, Processor::Thread*> g_contexts; // OSContext -> its host thread
        thread_local uint32_t t_context = 0; // the context the calling host thread runs as

        uint32_t Read32(const uint8_t* base, uint32_t address)
        {
            const uint8_t* p = base + address;
            return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
        }

        double ReadDouble(const uint8_t* base, uint32_t address)
        {
            const uint64_t bits = uint64_t(Read32(base, address)) << 32 | Read32(base, address + 4);
            double value;
            memcpy(&value, &bits, sizeof(value));
            return value;
        }

        // A new thread's registers, from its OSContext.
        void LoadRegisters(PPCContext& ctx, const uint8_t* base, uint32_t context)
        {
            PPCRegister* gpr[32] = { &ctx.r0, &ctx.r1, &ctx.r2, &ctx.r3, &ctx.r4, &ctx.r5, &ctx.r6, &ctx.r7, &ctx.r8,
                &ctx.r9, &ctx.r10, &ctx.r11, &ctx.r12, &ctx.r13, &ctx.r14, &ctx.r15, &ctx.r16, &ctx.r17, &ctx.r18,
                &ctx.r19, &ctx.r20, &ctx.r21, &ctx.r22, &ctx.r23, &ctx.r24, &ctx.r25, &ctx.r26, &ctx.r27, &ctx.r28,
                &ctx.r29, &ctx.r30, &ctx.r31 };
            for (uint32_t i = 0; i < 32; i++)
                gpr[i]->u64 = Read32(base, context + ContextGpr + 4 * i);
            PPCRegister* fpr = &ctx.f0;
            for (uint32_t i = 0; i < 32; i++)
            {
                fpr[i].f64 = ReadDouble(base, context + ContextFpr + 8 * i);
                ctx.ps1[i] = ReadDouble(base, context + ContextPs1 + 8 * i);
            }
            for (uint32_t i = 0; i < 8; i++)
                ctx.gqr[i] = Read32(base, context + ContextGqr + 4 * i);
            const uint32_t cr = Read32(base, context + ContextCr);
            PPCCRRegister* fields = &ctx.cr0;
            for (uint32_t i = 0; i < 8; i++)
            {
                const uint32_t bits = cr >> (28 - 4 * i);
                fields[i].lt = (bits >> 3) & 1;
                fields[i].gt = (bits >> 2) & 1;
                fields[i].eq = (bits >> 1) & 1;
                fields[i].so = bits & 1;
            }
            ctx.lr = Read32(base, context + ContextLr);
            ctx.ctr.u64 = Read32(base, context + ContextCtr);
            const uint32_t xer = Read32(base, context + ContextXer);
            ctx.xer.so = (xer >> 31) & 1;
            ctx.xer.ov = (xer >> 30) & 1;
            ctx.xer.ca = (xer >> 29) & 1;
        }

        void RunThread(uint8_t* base, uint32_t context)
        {
            t_context = context;
            static_assert(alignof(PPCContext) >= 0x40);
            auto ctx = std::make_unique<PPCContext>();
            LoadRegisters(*ctx, base, context);
            const uint32_t entry = Read32(base, context + ContextSrr0);
            const uint32_t exit = uint32_t(ctx->lr);
            fprintf(stderr, "[os] thread 0x%08X starts at 0x%08X, priority %d\n", context, entry,
                int32_t(Read32(base, context + 0x2D0)));
            PPCFunc* function = PPC_LOOKUP_FUNC(base, entry);
            function(*ctx, base);
            // The thread's function returned: to its link register, OSExitThread with the result, as on the console.
            PPCFunc* exitThread = PPC_LOOKUP_FUNC(base, exit);
            exitThread(*ctx, base);
            fprintf(stderr, "[os] a thread's exit at 0x%08X returned\n", exit);
            StopBoot(1);
        }
    } // namespace

    void StartProcessor() { TheProcessor().Start(); }

    void LogThreads(const uint8_t* base)
    {
        // The OSThread's fields after its context [OSCreateThread writes them]: state (1 ready, 2 running, 4 waiting,
        // 8 dead), priority, the queue a waiting thread sleeps on.
        for (const auto& [context, thread] : g_contexts)
            fprintf(stderr, "[os] thread 0x%08X: state %u, priority %d, waiting on 0x%08X\n", context,
                Read32(base, context + 0x2C8) >> 16, int32_t(Read32(base, context + 0x2D0)),
                Read32(base, context + 0x2DC));
    }

    void SaveContext(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t context = ctx.r3.u32;
        g_contexts[context] = TheProcessor().Current();
        t_context = context;
        // The stack pointer, which the SDK's checks read; the other registers stay with the host thread.
        const uint32_t sp = ctx.r1.u32;
        const uint8_t bytes[4] = { uint8_t(sp >> 24), uint8_t(sp >> 16), uint8_t(sp >> 8), uint8_t(sp) };
        memcpy(base + context + ContextGpr + 4, bytes, 4);
        ctx.r3.u64 = 0; // saved: resuming it returns from OSLoadContext (the scheduler then returns too)
    }

    void LoadContext(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t context = ctx.r3.u32;
        if (context != 0 && context == t_interruptContext)
            throw InterruptReturn{}; // back from the handler CallGuest runs
        if (context == t_context)
            return; // the scheduler chose the thread that saved: it carries on
        Processor& processor = TheProcessor();
        auto found = g_contexts.find(context);
        Processor::Thread* target = found != g_contexts.end() ? found->second : nullptr;
        if (target == nullptr)
        {
            target = processor.Spawn(GuestThreadStack, [base, context] { RunThread(base, context); });
            g_contexts[context] = target;
        }
        processor.HandTo(target);
        // Handed back: the scheduler loaded this thread's context again.
    }

    namespace
    {

        void Reschedule(PPCContext& ctx, uint8_t* base)
        {
            // An event's callback may have readied a thread above the current one: switch now, as the return from an
            // interrupt would. (Idle, the scheduler's own loop picks it.)
            if (t_ranGuest && Read32(base, 0x800000E4) != 0 && g_game.reschedule != 0)
            {
                t_ranGuest = false;
                PPCFunc* reschedule = PPC_LOOKUP_FUNC(base, g_game.reschedule);
                PPCContext scratch = ctx;
                reschedule(scratch, base);
            }
            t_ranGuest = false;
        }
    } // namespace

    bool WaitForEvent(PPCContext& ctx)
    {
        PPCContext* previous = std::exchange(t_interrupted, &ctx);
        const bool ran = TheProcessor().WaitForEvent(std::chrono::seconds(10));
        t_interrupted = previous;
        t_ranGuest = false; // idle: the scheduler's loop picks the thread
        return ran;
    }

    void RunPendingEvents(PPCContext& ctx, uint8_t* base)
    {
        PPCContext* previous = std::exchange(t_interrupted, &ctx);
        TheProcessor().RunPending();
        t_interrupted = previous;
        Reschedule(ctx, base);
    }

    bool InHandler() { return t_interruptContext != 0; }

    void CallGuest(uint8_t* base, uint32_t function, uint32_t r3, uint32_t r4)
    {
        if (t_interrupted == nullptr)
            return;
        PPCContext scratch = *t_interrupted;
        scratch.r3.u64 = r3;
        scratch.r4.u64 = r4 != InterruptedContext ? r4 : g_game.idleContext;
        // The operating system's current context: the idle one meanwhile, so the scheduler switches nothing inside.
        const uint32_t saved = Read32(base, CurrentContext);
        const uint32_t idle = g_game.idleContext;
        const uint8_t idleBytes[4] = { uint8_t(idle >> 24), uint8_t(idle >> 16), uint8_t(idle >> 8), uint8_t(idle) };
        const uint8_t savedBytes[4] = { uint8_t(saved >> 24), uint8_t(saved >> 16), uint8_t(saved >> 8),
            uint8_t(saved) };
        memcpy(base + CurrentContext, idleBytes, 4);
        PPCFunc* callback = PPC_LOOKUP_FUNC(base, function);
        const uint32_t outer = std::exchange(t_interruptContext, idle);
        try
        {
            callback(scratch, base);
        }
        catch (const InterruptReturn&)
        {}
        t_interruptContext = outer;
        memcpy(base + CurrentContext, savedBytes, 4);
        t_ranGuest = true;
    }

    void PostEvent(std::function<void()> event) { TheProcessor().Post(std::move(event)); }
}
