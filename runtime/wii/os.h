#pragma once

// The Wii operating system's functions the host answers in place of the game's own (games/<id>/addresses.h binds them
// to the game's addresses: games/<id>/hooks.cpp). Each takes the guest's registers as the translated code passes them.
#include <chrono>
#include <cstdint>
#include <functional>

struct PPCContext;

namespace wii::os
{
    void Report(PPCContext& ctx, uint8_t* base);      // OSReport(format, ...): a log line
    void Panic(PPCContext& ctx, uint8_t* base);       // OSPanic(file, line, format, ...): logged, the boot stops
    void Fatal(PPCContext& ctx, uint8_t* base);       // OSFatal(foreground, background, message): likewise
    // The interrupt switch: the previous state back in r3. Turning interrupts on is a safe point: the posted events
    // run there (as interrupts would), and with no guest thread ready the processor waits for one.
    void DisableInterrupts(PPCContext& ctx);
    void EnableInterrupts(PPCContext& ctx, uint8_t* base);
    void RestoreInterrupts(PPCContext& ctx, uint8_t* base); // r3: the state to restore
    // A hardware start the host does not need yet: r3 = `result` (0, or a start's success), logged once.
    void Skip(PPCContext& ctx, const char* function, uint32_t result = 0);
    // The interrupt switch's state (the MSR's EE bit), which each thread has its own of: a switch sets it as the thread
    // resumed had it (threads.cpp), with no safe point.
    bool InterruptsEnabled();
    void SetInterrupts(bool on);

    // The context switches (threads.cpp): OSSaveContext(context) and OSLoadContext(context).
    void SaveContext(PPCContext& ctx, uint8_t* base);
    void LoadContext(PPCContext& ctx, uint8_t* base);
    void ThreadCreated(PPCContext& ctx); // OSCreateThread(thread, ...), before the SDK's own: its context is new
    // The calling host thread holds the processor, as the game's first thread (the boot).
    void StartProcessor();
    // An event for the processor's holder, from any host thread (an IOS reply, an alarm). The holder runs them at
    // its safe points, `ctx` its registers there; CallGuest, from an event, runs a guest function on a copy of them,
    // as an interrupt handler (the operating system's current context the idle one meanwhile: no switch inside).
    void PostEvent(std::function<void()> event);
    void RunPendingEvents(PPCContext& ctx, uint8_t* base);
    // A loop that waits for an interrupt by polling (VIGetRetraceCount until it changes): the events run there, and
    // with none pending the processor waits up to 1 ms for one, as the loop would spin until the interrupt.
    void Poll(PPCContext& ctx, uint8_t* base);
    void PollEvents(PPCContext& ctx, uint8_t* base);
    bool WaitForEvent(PPCContext& ctx); // false when none came for 10 s
    void LogThreads(const uint8_t* base); // each guest thread the host knows: its state and what it waits on
    void CallGuest(uint8_t* base, uint32_t function, uint32_t r3, uint32_t r4);
    // True while CallGuest runs a handler: interrupts turned on inside it are no safe point (no event nests in one).
    bool InHandler();
    // CallGuest's r4 for a handler that takes the context it interrupted (an exception handler, which returns by
    // loading it): the idle context, which the handler's return then unwinds to CallGuest.
    constexpr uint32_t InterruptedContext = 0xFFFFFFFF;

    // The timers (timers.cpp): PPCMtdec(ticks), the decrementer, whose exception runs the alarms; and the video
    // interface's retrace, 59.94 times a second from VIInit on (its interrupt handler, as the hardware would).
    void SetDecrementer(PPCContext& ctx, uint8_t* base);
    void FrameBuffer(PPCContext& ctx); // VISetNextFrameBuffer(address): the first ones logged
    // GXSetDrawSync's token as the graphics processor would write it back once its commands reach it (the pixel
    // engine's token register, which GXReadDrawSync reads): at once, until the host draws.
    void DrawSyncReached(uint8_t* base, uint16_t token);
    void StartRetrace(uint8_t* base);
    // The remotes (remote.h): WPADProbe(remote, type) finds remote 1 alone, a remote with no extension;
    // KPADRead(remote, statuses, length) gives remote 1's one sample, none for the others.
    void WpadProbe(PPCContext& ctx, uint8_t* base);
    void KpadRead(PPCContext& ctx, uint8_t* base);
    // The audio interface's interrupt, every `period` from StartAudio to StopAudio (AudioInterrupt, an event).
    void StartAudio(uint8_t* base, std::chrono::nanoseconds period);
    void StopAudio();

    // The audio hardware (audio.cpp): the audio interface's DMA and the DSP running AX's task.
    void AiRegisterDmaCallback(PPCContext& ctx); // AIRegisterDMACallback(callback): the previous one back
    void AiInitDma(PPCContext& ctx); // AIInitDMA(address, length): the buffer the next DMA plays
    void AiStartDma(uint8_t* base);
    void AiStopDma();
    void AudioInterrupt(uint8_t* base); // the DMA's end: its callback
    void DspAddTask(PPCContext& ctx, uint8_t* base); // DSPAddTask(task): the task started at once
    void DspSendMail(PPCContext& ctx, uint8_t* base); // DSPSendMailToDSP(mail): AX's command lists run

    // The IOS calls (ios_calls.cpp), on the host's IOS (Hosted, its devices in ios_devices.cpp).
    void IosOpen(PPCContext& ctx, uint8_t* base, bool async);
    void IosClose(PPCContext& ctx, uint8_t* base, bool async);
    void IosRead(PPCContext& ctx, uint8_t* base, bool async);
    void IosWrite(PPCContext& ctx, uint8_t* base, bool async);
    void IosSeek(PPCContext& ctx, uint8_t* base, bool async);
    void IosIoctl(PPCContext& ctx, uint8_t* base, bool async);
    void IosIoctlv(PPCContext& ctx, uint8_t* base, bool async);

    // What the host reads of the game's operating system, at the game's addresses (games/<id>/hooks.cpp).
    struct GameAddresses
    {
        uint32_t runQueueBits = 0; // the scheduler's ready priorities: none while it waits idle
        uint32_t idleContext = 0; // its idle context, the current one while the scheduler waits
        uint32_t reschedule = 0; // __OSReschedule: switches to a thread an event made ready, when it should
        uint32_t decrementerHandler = 0; // DecrementerExceptionCallback(exception, context): the alarms
        uint32_t retraceHandler = 0; // __VIRetraceHandler(interrupt, context)
        // The area the host keeps for the game's module (wii/module.h), mapped past MEM1: none when 0.
        uint32_t moduleArea = 0;
        uint32_t moduleAreaSize = 0;
    };
    extern const GameAddresses g_game;
}

namespace wii::ios
{
    class Ios;
}

namespace disc
{
    class WiiDisc;
}

namespace wii::nand
{
    class FileSystem;
}

namespace wii
{
    ios::Ios& Hosted(); // the host's IOS, its devices registered (ios_devices.cpp)
    void SetDisc(const disc::WiiDisc& disc); // the disc /dev/di reads, before the first IOS call
    void SetNand(nand::FileSystem& nand, uint64_t titleId); // the NAND /dev/fs serves, the title /dev/es tells

    // Ends the boot: the host's main returns `code` once the log is written. The calling guest thread never returns.
    [[noreturn]] void StopBoot(int code);
}
