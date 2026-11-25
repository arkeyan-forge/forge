// Skylanders SuperChargers Racing (Wii): the guest functions the Wii host hooks. They are the Wii SDK's (RVL_SDK, the
// August 2010 build), found by name in the disc's ELF (its symbol table); the other Wii games, when they come, have
// the same functions at their own addresses. A hook the game does not define is compiled out.
#pragma once
#include <cstdint>

// The operating system's messages and errors (runtime/wii/os.cpp).
#define GAME_HOOK_OSReport sub_8028DF20
#define GAME_HOOK_OSPanic sub_8028DFB0
#define GAME_HOOK_OSFatal sub_80290430

// The processor's interrupt switch (the MSR's EE bit, which the translation does not keep).
#define GAME_HOOK_OSDisableInterrupts sub_80291900
#define GAME_HOOK_OSEnableInterrupts sub_80291920
#define GAME_HOOK_OSRestoreInterrupts sub_80291940

// The context switches the scheduler (SelectThread) makes: the host's (runtime/wii/threads.cpp).
#define GAME_HOOK_OSSaveContext sub_8028D930
#define GAME_HOOK_OSLoadContext sub_8028D9B0
#define GAME_HOOK_OSCreateThread sub_80294F00

namespace game
{
    // gamelogic.rso, the game's module: the host gives its file and its BSS from this area past MEM1 (within reach of
    // the engine's branches), where its translation was linked (tools/rso_link.py 0x81800000, then the BSS after the
    // file); the engine's two allocations for it, and its shrinking after the link, by their return addresses in
    // GEEngineMgrImp::InitGL.
    constexpr uint32_t ModuleArea = 0x81800000;
    constexpr uint32_t ModuleAreaSize = 0x500000;
    constexpr uint32_t ModuleFileAllocated = 0x8022EB18;
    constexpr uint32_t ModuleBssAllocated = 0x8022EBD8;
    // The scheduler's ready priorities (RunQueueBits, a bit for each priority with a thread ready): 0 while it idles.
    constexpr uint32_t RunQueueBits = 0x807DA970;
    constexpr uint32_t IdleContext = 0x8065C970; // the scheduler's
    constexpr uint32_t Reschedule = 0x80294EA0;  // __OSReschedule
    constexpr uint32_t DecrementerHandler = 0x8028C1D0; // DecrementerExceptionCallback
    constexpr uint32_t RetraceHandler = 0x802ED2A0;     // __VIRetraceHandler
}

// Hardware the host answers later, skipped at the start: the EXI and serial buses, the SRAM behind EXI, the DSP.
#define GAME_HOOK_EXIInit sub_802F4F70
#define GAME_HOOK_SIInit sub_802F3390
#define GAME_HOOK___OSInitSram sub_80293AD0
#define GAME_HOOK___OSInitAudioSystem sub_8028CCB0
// The Bluetooth stack's start (the remotes' library, WPAD, under it): its HCI over USB is not served; the remotes
// come with the host's input.
#define GAME_HOOK_WUDInit sub_802B58B0
#define GAME_HOOK_WUDGetStatus sub_802B5C20

// The timers: the decrementer the alarms set, and VI's start, after which its retrace interrupt runs.
#define GAME_HOOK_PPCMtdec sub_8027AA20
// The write-gather pipe's address register, which the translation does not read or write (runtime/wii/gx_fifo.h).
#define GAME_HOOK_PPCMfwpar sub_8027AB30
#define GAME_HOOK_PPCMtwpar sub_8027AB40
#define GAME_HOOK_VIInit sub_802EDE70
// The first frames: logged.
#define GAME_HOOK_VISetNextFrameBuffer sub_802EF4A0
// The retrace count, which the engine's Flip polls in a loop until it changes: a poll point (the retrace's event runs).
#define GAME_HOOK_VIGetRetraceCount sub_802EF590

// The graphics processor's "draw done", which waits for its finish interrupt: done at once until the host draws.
#define GAME_HOOK_GXDrawDone sub_8027DBE0
#define GAME_HOOK_GXWaitDrawDone sub_8027DB90
// The audio hardware: the audio interface's DMA (its start skipped: it times the interface's sample counter) and the
// DSP's task, AX's, run by the host (runtime/wii/audio.cpp).
#define GAME_HOOK_AIInit sub_802F1C90
#define GAME_HOOK_AIRegisterDMACallback sub_802F1B30
#define GAME_HOOK_AIInitDMA sub_802F1B80
#define GAME_HOOK_AIStartDMA sub_802F1C00
#define GAME_HOOK_AIStopDMA sub_802F1C20
#define GAME_HOOK_AIGetDMABytesLeft sub_802F1C40
#define GAME_HOOK_DSPInit sub_802F2170
#define GAME_HOOK_DSPAddTask sub_802F2240
#define GAME_HOOK_DSPSendMailToDSP sub_802F2150
#define GAME_HOOK_DSPCheckMailToDSP sub_802F2110
#define GAME_HOOK_DSPAssertTask sub_802F22F0

// The draw sync token, which the graphics processor writes back once it gets there: written back at once.
#define GAME_HOOK_GXSetDrawSync sub_8027DA10

// The engine's heap (its allocations for the module), and the SDK's RSO linker (the module's link checked after it).
#define GAME_HOOK_MMMemoryMgr_Alloc sub_8017D1C0
#define GAME_HOOK_MMMemoryMgr_Free sub_801EBE30
#define GAME_HOOK_MMHeap_Realloc sub_8022F860
#define GAME_HOOK_RSOLinkListFixed sub_80329490

// IOS's client calls: the host's IOS answers them (runtime/wii/ios_calls.cpp).
#define GAME_HOOK_IOS_Open sub_8026CAF0
#define GAME_HOOK_IOS_OpenAsync sub_8026C9D0
#define GAME_HOOK_IOS_Close sub_8026CCE0
#define GAME_HOOK_IOS_CloseAsync sub_8026CC20
#define GAME_HOOK_IOS_Read sub_8026CE90
#define GAME_HOOK_IOS_ReadAsync sub_8026CD90
#define GAME_HOOK_IOS_Write sub_8026D0A0
#define GAME_HOOK_IOS_WriteAsync sub_8026CFA0
#define GAME_HOOK_IOS_SeekAsync sub_8026D1B0
#define GAME_HOOK_IOS_Ioctl sub_8026D3D0
#define GAME_HOOK_IOS_IoctlAsync sub_8026D290
#define GAME_HOOK_IOS_Ioctlv sub_8026D730
#define GAME_HOOK_IOS_IoctlvAsync sub_8026D640
