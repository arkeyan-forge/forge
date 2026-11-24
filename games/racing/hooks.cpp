// SuperChargers Racing's hooks: the Wii host's answers (runtime/wii/os.h) bound to this game's SDK functions
// (addresses.h).
#include "game_hooks.h"
#include "ppc_recomp_shared.h"
#include "wii/os.h"

PPC_FUNC(GAME_HOOK(OSReport)) { wii::os::Report(ctx, base); }
PPC_FUNC(GAME_HOOK(OSPanic)) { wii::os::Panic(ctx, base); }
PPC_FUNC(GAME_HOOK(OSFatal)) { wii::os::Fatal(ctx, base); }
PPC_FUNC(GAME_HOOK(OSDisableInterrupts)) { wii::os::DisableInterrupts(ctx); }
PPC_FUNC(GAME_HOOK(OSEnableInterrupts)) { wii::os::EnableInterrupts(ctx); }
PPC_FUNC(GAME_HOOK(OSRestoreInterrupts)) { wii::os::RestoreInterrupts(ctx); }
PPC_FUNC(GAME_HOOK(EXIInit)) { wii::os::Skip(ctx, "EXIInit"); }
PPC_FUNC(GAME_HOOK(SIInit)) { wii::os::Skip(ctx, "SIInit"); }
PPC_FUNC(GAME_HOOK(__OSInitSram)) { wii::os::Skip(ctx, "__OSInitSram"); }
PPC_FUNC(GAME_HOOK(__OSInitAudioSystem)) { wii::os::Skip(ctx, "__OSInitAudioSystem"); }
PPC_FUNC(GAME_HOOK(IOS_Open)) { wii::os::OpenDevice(ctx, base); }
PPC_FUNC(GAME_HOOK(IOS_OpenAsync)) { wii::os::OpenDevice(ctx, base); }
