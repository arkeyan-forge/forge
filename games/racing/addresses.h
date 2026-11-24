// Skylanders SuperChargers Racing (Wii): the guest functions the Wii host hooks. They are the Wii SDK's (RVL_SDK, the
// August 2010 build), found by name in the disc's ELF (its symbol table); the other Wii games, when they come, have
// the same functions at their own addresses. A hook the game does not define is compiled out.
#pragma once

// The operating system's messages and errors (runtime/wii/os.cpp).
#define GAME_HOOK_OSReport sub_8028DF20
#define GAME_HOOK_OSPanic sub_8028DFB0
#define GAME_HOOK_OSFatal sub_80290430

// The processor's interrupt switch (the MSR's EE bit, which the translation does not keep).
#define GAME_HOOK_OSDisableInterrupts sub_80291900
#define GAME_HOOK_OSEnableInterrupts sub_80291920
#define GAME_HOOK_OSRestoreInterrupts sub_80291940

// Hardware the host answers later, skipped at the start: the EXI and serial buses, the SRAM behind EXI, the DSP.
#define GAME_HOOK_EXIInit sub_802F4F70
#define GAME_HOOK_SIInit sub_802F3390
#define GAME_HOOK___OSInitSram sub_80293AD0
#define GAME_HOOK___OSInitAudioSystem sub_8028CCB0

// The first request to IOS, where the boot stops for now.
#define GAME_HOOK_IOS_Open sub_8026CAF0
#define GAME_HOOK_IOS_OpenAsync sub_8026C9D0
