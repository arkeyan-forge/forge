// Skylanders: Giants: the guest functions the runtime hooks and the guest data it reads. Same names as
// games/ssa/addresses.h (docs/giants/ssa_hooks.json). It starts empty: no hook exists in a Giants build until its line
// is added here, e.g.
//     #define GAME_HOOK_D3DDevice_DrawVertices sub_XXXXXXXX
// A data global is a constexpr in namespace game, 0 while the address is not known (the hooks that read it need
// their GAME_HOOK_ line too, so they stay out of the build until both are there).
#pragma once

// D3D (runtime/d3d/*.cpp): XDK D3D9LTCG 2.0.21076, found with tools/xmatch.py (games/giants/docs/boot.md). No command buffers: Alchemy
// does not record any. No Create*Shader: Alchemy calls the shader Init functions on objects it builds itself.
#define GAME_HOOK_D3DDevice_NextRingSegment sub_823D0DB8
#define GAME_HOOK_D3DDevice_KickOff sub_823D1C10
#define GAME_HOOK_D3DDevice_BlockOnFence sub_823D1D10
#define GAME_HOOK_D3DDevice_InsertCallback sub_823D1E08
#define GAME_HOOK_D3DDevice_DrawVertices sub_823E03E0
#define GAME_HOOK_D3DDevice_DrawIndexedVertices sub_823E07F8
#define GAME_HOOK_D3DDevice_Resolve sub_823DED00
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_823DDF38
#define GAME_HOOK_D3DDevice_Swap sub_823DA640
#define GAME_HOOK_D3DVertexShader_Init sub_823DC7E8
#define GAME_HOOK_D3DPixelShader_Init sub_823DC5F0

// XMA (runtime/kernel/xma.cpp): the same two engines as SSA (shape similarity 1.00). Engine 2 has no init of its own: its
// read of the context array register is inline in sub_823561C0 (a voice setup), hooked as its init.
#define GAME_HOOK_Xma1_Init sub_82436480
#define GAME_HOOK_Xma2_Init sub_823561C0
#define GAME_HOOK_Xma1_VoiceClear sub_824364E0
#define GAME_HOOK_Xma1_VoiceKick sub_82436E78
#define GAME_HOOK_Xma2_VoiceClear sub_823CD048
#define GAME_HOOK_Xma2_VoiceLock sub_823CD1F0
#define GAME_HOOK_Xma2_VoiceKick sub_823CD4A0
#define GAME_HOOK_Xma1_LockAll sub_82436D08

// Ultrawide (engines/alchemy/ultrawide.cpp, games/giants/adapter.cpp): sub_8219B900 reads the video mode and stores the
// display's aspect (game::UltrawideDisplayAspect); sub_8218ED90 gives the 3D scene's width (the back buffer's, at most
// 1000; sub_8218EDD8 the height, at most 600).
// Two more callers cap the width at 1000 inline and pass it on: sub_821939E0 to sub_82196150 (r4), sub_8219E0A8 to
// sub_8219EC20 (r4).
#define GAME_HOOK_Ultrawide_CameraAspect sub_8219B900
#define GAME_HOOK_Ultrawide_SceneWidth sub_8218ED90
#define GAME_HOOK_Ultrawide_SceneHeight sub_8218EDD8
#define GAME_HOOK_Ultrawide_SceneTargets sub_82196150
#define GAME_HOOK_Ultrawide_ScenePasses sub_8219EC20

// LZMA (engines/alchemy/lzma.cpp): the LZMA SDK's LzmaDec_DecodeReal (9.x), the inner loop of every archive chunk's
// decode, found by its constants (kTopValue lis 256, the literal table at probs + 3692 bytes, kMatchSpecLenStart 274);
// the same code, byte for byte in its asm, in Giants and Swap Force.
#define GAME_HOOK_LzmaDec_DecodeReal sub_822BBA60

// Bink 2 (runtime/d3d/bink_display.cpp): BinkOpen calls the header reader sub_82153BE8 (tsapZ/GiantsRecomp: 0x82153DB0).
#define GAME_HOOK_BinkOpen sub_821550A0
#define GAME_HOOK_BinkClose sub_82155B08 // tsapZ/GiantsRecomp: 0x82155B08; sets HBINK+76, then frees

// Voice language (games/giants/language.cpp): the resource loader that sub_8217A0C0 calls for each archive's language
// pack ("<archive>.bld:/<LANGUAGE>.pak") and level.bld; the movie start that sets the Bink voice track (sub_823344B0,
// BinkSetSoundTrack sub_82150BF0 then BinkOpen).
#define GAME_HOOK_Language_LoadPack sub_822A8590
// The text language (games/giants/language.cpp): sub_8217F2D8 sets the app's _appLanguage from the console language
// (sub_8223C248) and then keeps it only if the region allows it (a bitmask of IG_CORE_LANGUAGE values: with an NTSC AV
// region English, French, Latin-American Spanish and Portuguese; with PAL ten others), else English.
#define GAME_HOOK_Language_Region sub_8217F2D8
#define GAME_HOOK_Movie_Open sub_823344B0

#include <cstdint>

namespace game
{
    // D3D device fields from the vertex declaration on sit this many bytes later than in SSA's XDK (d3d/device.h).
    constexpr uint32_t DeviceVertexDeclaration = 0x2FD0; // the current vertex declaration on the device
    // D3DDevice_DrawIndexedVertices takes a base vertex (r5) before the start index and count.
    constexpr bool DrawIndexedBaseVertex = false;
    // Render targets of a predicated-tiling pass cover all the tiles (d3d/render.cpp TargetFor). SSA renders without.
    constexpr bool ExpandTiledTargets = true;
    // D3DDevice_Resolve takes a destination level (r8) before the slice and the clear color.
    constexpr bool ResolveHasLevel = false;
    constexpr uint32_t DeviceStateShift = 0x80;
    constexpr uint32_t LanguageIndex = 0;
    // The app object (sub_8217BF48 returns it; set by sub_8223C248) and its _appLanguage (an IG_CORE_LANGUAGE value,
    // from the console language by sub_82297910): text, voices and movie tracks all follow it.
    constexpr uint32_t AppObject = 0x826F4258;
    constexpr uint32_t AppLanguageOffset = 56;
    constexpr uint32_t LanguageCodes = 0;
    constexpr uint32_t NglCurrentScene = 0;
    constexpr uint32_t Engine1CriticalSection = 0x82627B90; // sigmatch, via Xma1_LockAll
    constexpr uint32_t DeviceGlobal = 0; // holds the device pointer
    constexpr uint32_t UltrawideDisplayAspect = 0x826565C0; // float: the display's aspect, which every camera reads
    // Ultrawide (d3d/ultrawide.h): the 3D scene's 16:9 size, and its composite into the frame: one non-indexed quad in
    // pixels (vertex constant c0 = 2 / 1280), so it fills a wider viewport as it is. The scene is in slot 0 (the HUD's
    // quads keep stale scene-sized textures in other slots).
    constexpr uint32_t UltrawideSceneWidth = 1000, UltrawideSceneHeight = 600;
    constexpr bool UltrawideSceneKeepsArea = true; // d3d/ultrawide.h
    constexpr uint32_t UltrawideCompositeVertices = 4;
    constexpr int UltrawideCompositeSlot = 0;
    constexpr bool UltrawideCompositeVpos = false;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts. The scene's vertex shaders keep
    // the object's world x view x projection in c0-c3 as row vectors: clip = x c0 + y c1 + z c2 + c3 (a level frame).
    constexpr int32_t UpscalePositionMatrix = 0;
    // The post pass whose constants and fetch offsets give its kernel in guest pixels: at scale N, in host pixels.
    constexpr uint64_t HostKernelPixelShader = 0; // no such pass
    // Where the pass keeps its contrast threshold in the pixel constants (a float index; the minimum follows): the edge smoothing setting.
    constexpr uint32_t HostKernelThreshold = 0; // no known FXAA pass
}
