// Skylanders: Trap Team: the guest functions the runtime hooks and the guest data it reads. Same names as
// games/ssa/addresses.h, games/giants/addresses.h and games/swapforce/addresses.h. A hook the game does not define is
// compiled out. A data global is a constexpr in namespace game, 0 while the address is not known. Never reuse another
// game's address: every one here was re-found in Trap Team's own binary (tools/sigmatch against Giants' inventory, or the
// loose shape search against Swap Force's functions; docs/porting/day1.md), games/trapteam/docs/status.md has the evidence.
#pragma once

// D3D (runtime/d3d/*.cpp): XDK D3D9 2.0.21256 (Swap Force 21250, Giants 21076). All eleven are strict matches (1.0) of
// Giants' functions, offsets included, so the device layout is Giants' until a boot says otherwise [?].
#define GAME_HOOK_D3DDevice_NextRingSegment sub_8252FA00
#define GAME_HOOK_D3DDevice_KickOff sub_82530858
#define GAME_HOOK_D3DDevice_BlockOnFence sub_82530958
#define GAME_HOOK_D3DDevice_InsertCallback sub_82530A50
#define GAME_HOOK_D3DDevice_DrawVertices sub_8253F0B0
#define GAME_HOOK_D3DDevice_DrawIndexedVertices sub_8253F4C8
#define GAME_HOOK_D3DDevice_Resolve sub_8253D9D0
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_8253CC08
#define GAME_HOOK_D3DDevice_Swap sub_8253A948
#define GAME_HOOK_D3DVertexShader_Init sub_82531C70
#define GAME_HOOK_D3DPixelShader_Init sub_82531A78

// XMA (runtime/kernel/xma.cpp): the same two engines as the other games. Xma1_Init, Xma1_VoiceKick, Xma2_VoiceLock,
// Xma2_VoiceKick and Xma1_LockAll are strict matches; Xma2_Init, Xma1_VoiceClear and Xma2_VoiceClear are loose (0.9) [?].
#define GAME_HOOK_Xma1_Init sub_825C5810
#define GAME_HOOK_Xma2_Init sub_824AEAD0
#define GAME_HOOK_Xma1_VoiceClear sub_825C5870
#define GAME_HOOK_Xma1_VoiceKick sub_825C6208
#define GAME_HOOK_Xma2_VoiceClear sub_8252BC90
#define GAME_HOOK_Xma2_VoiceLock sub_8252BE38
#define GAME_HOOK_Xma2_VoiceKick sub_8252C0E8
#define GAME_HOOK_Xma1_LockAll sub_825C6098

// LZMA (engines/alchemy/lzma.cpp): the LZMA SDK's LzmaDec_DecodeReal, found by an 80-instruction loose match (branch targets
// and 16-bit immediates masked) of Swap Force's, unique in the image.
#define GAME_HOOK_LzmaDec_DecodeReal sub_8239F0C0

// Bink 2 (runtime/d3d/bink_display.cpp): the same 80-instruction loose match of Swap Force's BinkOpen and BinkClose; the
// sigmatch against Giants gives the same BinkOpen (0.9).
#define GAME_HOOK_BinkOpen sub_821A52C8
#define GAME_HOOK_BinkClose sub_821A5FE8

// The language (games/trapteam/language.cpp, boot.md "Language"): the title's one reader of the console language, XGetLanguage
// mapped to an IG_CORE_LANGUAGE value in r3 (17 cases; the same switch as Swap Force's); sigmatch 0.61, shape 0.83, read by
// hand: its three callers store the value in the objects that choose the language pack and the movie's voice track.
#define GAME_HOOK_Language_Get sub_823229A0
// The console language (XGetLanguage: ExGetXConfigSetting 3/9, then the game region when it is out of range), whose callers are
// Language_Get and the chooser sub_821D29B8: that one tests 3 (German), 4 (French) and 16 (Dutch) and which language files the disc
// has, and falls back to English, or to French where the disc has French and Dutch.
#define GAME_HOOK_Console_Language sub_8258A230
// sub_821CD058's pack-loading helper: r4 path, r5 directory, boolean result (unlike Giants' zero-on-success loader).
#define GAME_HOOK_Language_LoadPack sub_821CCFC0
// Movie worker: description at +32; track +56, audio mode +40. Calls BinkSetSoundTrack then BinkOpen.
#define GAME_HOOK_Movie_Open sub_824A2E40

// The graphics set-up that reads the config's "Gfx/@enableVramCopy" (default 1) into three copies of the flag, and "Gfx/@dumpShaderConstants"
// (games/trapteam/vram_copy.cpp). Found from the string; the first level's vertex buffers fill the 298 MB video pool unless it is 0.
#define GAME_HOOK_Gfx_ConfigInit sub_823ED808

// Ultrawide (games/trapteam/ultrawide.cpp, docs/runtime/gpu/ultrawide.md): Swap Force's engine again. sub_8243F650 computes a
// render-target pass's size (explicit +32 and +36, scales +40 and +44, result +92 and +96; sigmatch 0.60, asm read). The world
// camera's matrices are built by sub_823B1AF0 (shape 1.00 against Swap Force's sub_824906D8, found through the callers' aspect in
// a log): a horizontal field of view and an aspect, from sub_8243C700 (the scene target's shape) and sub_8230F3D8 (a 16:9 global).
#define GAME_HOOK_Ultrawide_ScenePass sub_8243F650
#define GAME_HOOK_Ultrawide_Projection sub_823B1AF0
// The scene at full resolution (game::Adapter::fullScene): sub_823EBCD0 splits a set of targets into tiles past the console's 2048
// EDRAM tiles, sub_823EF930 counts a surface's tiles (the plan, sub_82443DF8, calls it; sub_823EC8E0 sets the targets). Both
// read against Swap Force's (sigmatch 1.0 and 0.9, shape 1.00, the same offsets).
#define GAME_HOOK_Scene_Tiles sub_823EBCD0
#define GAME_HOOK_Scene_SurfaceTiles sub_823EF930

#include <cstdint>

namespace game
{
    // D3D: the offsets the runtime reads, Giants' (the strict matches above carry them); the vertex declaration's 0x2FD0 is
    // stored by the device reset sub_82546B18 [?]. Not yet checked in a draw.
    constexpr uint32_t DeviceVertexDeclaration = 0x2FD0;
    constexpr bool DrawIndexedBaseVertex = false; // DrawIndexedVertices is Giants': (device, primitive, start, count)
    constexpr bool ExpandTiledTargets = true;
    constexpr bool ResolveHasLevel = false; // Resolve is Giants': slice r8, color r9
    constexpr uint32_t DeviceStateShift = 0x80;
    constexpr uint32_t LanguageIndex = 0;
    constexpr uint32_t LanguageCodes = 0;
    constexpr uint32_t LanguageAppObject = 0x828940DC; // sub_821CF530; app language at +12
    constexpr uint32_t NglCurrentScene = 0;
    constexpr uint32_t Engine1CriticalSection = 0x827F8640; // sigmatch (Giants' inventory), via Xma1_LockAll
    constexpr uint32_t DeviceGlobal = 0;
    constexpr uint32_t UltrawideDisplayAspect = 0;
    constexpr uint32_t UltrawideSceneWidth = 1040, UltrawideSceneHeight = 620;
    constexpr bool UltrawideSceneKeepsArea = true;
    constexpr uint32_t UltrawideCompositeVertices = 3; // one screen triangle, vertex constant c0 = 2/1280
    constexpr int UltrawideCompositeSlot = 0;
    constexpr bool UltrawideCompositeVpos = false;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts: c0-c3, row vectors (Alchemy, as
    // Giants'; the camera vote's "[upscale] last 900 frames" line checks it).
    constexpr int32_t UpscalePositionMatrix = 0;
    // The post pass whose constants and fetch offsets give its kernel in guest pixels: at scale N, in host pixels.
    constexpr uint64_t HostKernelPixelShader =
        0x6BD91E6572C6581Cull; // tone map + FXAA 3.11 in one pass (c0, c1 = +-0.5, 8, -4 / size)
    // Where the pass keeps its contrast threshold in the pixel constants (a float index; the minimum follows): the edge smoothing setting.
    constexpr uint32_t HostKernelThreshold = 12; // the threshold (c3) and the minimum (c4), after c2 (sharpness)
    // The three bytes sub_823ED808 sets from "Gfx/@enableVramCopy" (the three stb after its config read, at offsets -11263..-11261 from 0x82860000; sub_823F0128 reads the last).
    constexpr uint32_t VramCopyFlags[] = { 0x8285D401, 0x8285D402, 0x8285D403 };
}
