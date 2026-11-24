#pragma once

// The Wii operating system's functions the host answers in place of the game's own (games/<id>/addresses.h binds them
// to the game's addresses: games/<id>/hooks.cpp). Each takes the guest's registers as the translated code passes them.
#include <cstdint>

struct PPCContext;

namespace wii::os
{
    void Report(PPCContext& ctx, uint8_t* base);      // OSReport(format, ...): a log line
    void Panic(PPCContext& ctx, uint8_t* base);       // OSPanic(file, line, format, ...): logged, the boot stops
    void Fatal(PPCContext& ctx, uint8_t* base);       // OSFatal(foreground, background, message): likewise
    void DisableInterrupts(PPCContext& ctx);          // the previous state in r3, interrupts off
    void EnableInterrupts(PPCContext& ctx);           // the previous state in r3, interrupts on
    void RestoreInterrupts(PPCContext& ctx);          // r3: the state to restore; the previous one back in r3
    void Skip(
        PPCContext& ctx, const char* function); // a hardware start the host does not need yet: r3 = 0, logged once
    void OpenDevice(PPCContext& ctx, uint8_t* base);  // IOS_Open(path, mode): logged, the boot stops (for now)
}

namespace wii
{
    // Ends the boot: the host's main returns `code` once the log is written. The calling guest thread never returns.
    [[noreturn]] void StopBoot(int code);
}
