// SuperChargers Racing's hooks: the Wii host's answers (runtime/wii/os.h) bound to this game's SDK functions
// (addresses.h).
#include "game_hooks.h"
#include "ppc_recomp_shared.h"
#include "wii/os.h"

PPC_FUNC(GAME_HOOK(OSReport)) { wii::os::Report(ctx, base); }
PPC_FUNC(GAME_HOOK(OSPanic)) { wii::os::Panic(ctx, base); }
PPC_FUNC(GAME_HOOK(OSFatal)) { wii::os::Fatal(ctx, base); }
PPC_FUNC(GAME_HOOK(OSDisableInterrupts)) { wii::os::DisableInterrupts(ctx); }
PPC_FUNC(GAME_HOOK(OSEnableInterrupts)) { wii::os::EnableInterrupts(ctx, base); }
PPC_FUNC(GAME_HOOK(OSRestoreInterrupts)) { wii::os::RestoreInterrupts(ctx, base); }
PPC_FUNC(GAME_HOOK(OSSaveContext)) { wii::os::SaveContext(ctx, base); }
PPC_FUNC(GAME_HOOK(OSLoadContext)) { wii::os::LoadContext(ctx, base); }

PPC_FUNC(GAME_HOOK(PPCMtdec)) { wii::os::SetDecrementer(ctx, base); }
PPC_FUNC_IMPL(GAME_IMP(VIInit));
PPC_FUNC(GAME_HOOK(VIInit))
{
    GAME_IMP(VIInit)(ctx, base);
    wii::os::StartRetrace(base);
}
PPC_FUNC_IMPL(GAME_IMP(VISetNextFrameBuffer));
PPC_FUNC(GAME_HOOK(VISetNextFrameBuffer))
{
    wii::os::FrameBuffer(ctx);
    GAME_IMP(VISetNextFrameBuffer)(ctx, base);
}

const wii::os::GameAddresses wii::os::g_game = { game::RunQueueBits, game::IdleContext, game::Reschedule,
    game::DecrementerHandler, game::RetraceHandler };
PPC_FUNC(GAME_HOOK(EXIInit)) { wii::os::Skip(ctx, "EXIInit"); }
PPC_FUNC(GAME_HOOK(SIInit)) { wii::os::Skip(ctx, "SIInit"); }
PPC_FUNC(GAME_HOOK(__OSInitSram)) { wii::os::Skip(ctx, "__OSInitSram"); }
PPC_FUNC(GAME_HOOK(__OSInitAudioSystem)) { wii::os::Skip(ctx, "__OSInitAudioSystem"); }
// Started, for WPADInit to go on; as the stack's own start, only the first time: WPADInit then sets up its alarm, and a
// second WPADInit (Wwise's) must not set it again while it is queued.
PPC_FUNC(GAME_HOOK(WUDInit))
{
    static bool started = false;
    wii::os::Skip(ctx, "WUDInit", started ? 0 : 1);
    started = true;
}
PPC_FUNC(GAME_HOOK(WUDGetStatus)) { ctx.r3.u64 = 3; } // set up: the remotes' library ready, none connected
PPC_FUNC(GAME_HOOK(GXDrawDone)) { wii::os::Skip(ctx, "GXDrawDone"); }
PPC_FUNC(GAME_HOOK(GXWaitDrawDone)) { wii::os::Skip(ctx, "GXWaitDrawDone"); }
PPC_FUNC(GAME_HOOK(AIInit)) { wii::os::Skip(ctx, "AIInit"); }
PPC_FUNC(GAME_HOOK(AIRegisterDMACallback)) { wii::os::AiRegisterDmaCallback(ctx); }
PPC_FUNC_IMPL(GAME_IMP(AIInitDMA));
PPC_FUNC(GAME_HOOK(AIInitDMA))
{
    wii::os::AiInitDma(ctx);
    GAME_IMP(AIInitDMA)(ctx, base); // the interface's registers too, which AIGetDMAStartAddr reads
}
PPC_FUNC(GAME_HOOK(AIStartDMA)) { wii::os::AiStartDma(base); }
PPC_FUNC(GAME_HOOK(AIStopDMA)) { wii::os::AiStopDma(); }
PPC_FUNC(GAME_HOOK(AIGetDMABytesLeft)) { ctx.r3.u64 = 0; } // the host plays each buffer at its interrupt
PPC_FUNC(GAME_HOOK(DSPInit)) { wii::os::Skip(ctx, "DSPInit"); }
PPC_FUNC(GAME_HOOK(DSPAddTask)) { wii::os::DspAddTask(ctx, base); }
PPC_FUNC(GAME_HOOK(DSPSendMailToDSP)) { wii::os::DspSendMail(ctx, base); }
PPC_FUNC(GAME_HOOK(DSPCheckMailToDSP)) { ctx.r3.u64 = 0; } // the host takes each mail at once
PPC_FUNC(GAME_HOOK(DSPAssertTask)) { ctx.r3.u64 = 0; } // the host's task never keeps the DSP
PPC_FUNC_IMPL(GAME_IMP(GXSetDrawSync));
PPC_FUNC(GAME_HOOK(GXSetDrawSync))
{
    const uint16_t token = uint16_t(ctx.r3.u32);
    GAME_IMP(GXSetDrawSync)(ctx, base);
    wii::os::DrawSyncReached(base, token);
}
PPC_FUNC(GAME_HOOK(IOS_Open)) { wii::os::IosOpen(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_OpenAsync)) { wii::os::IosOpen(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_Close)) { wii::os::IosClose(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_CloseAsync)) { wii::os::IosClose(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_Read)) { wii::os::IosRead(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_ReadAsync)) { wii::os::IosRead(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_Write)) { wii::os::IosWrite(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_WriteAsync)) { wii::os::IosWrite(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_SeekAsync)) { wii::os::IosSeek(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_Ioctl)) { wii::os::IosIoctl(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_IoctlAsync)) { wii::os::IosIoctl(ctx, base, true); }
PPC_FUNC(GAME_HOOK(IOS_Ioctlv)) { wii::os::IosIoctlv(ctx, base, false); }
PPC_FUNC(GAME_HOOK(IOS_IoctlvAsync)) { wii::os::IosIoctlv(ctx, base, true); }
