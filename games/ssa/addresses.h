// Spyro's Adventure: the guest functions the runtime hooks and the guest data it reads. Names are those of
// docs/giants/ssa_hooks.json. A hook exists in a build only if its GAME_HOOK_<name> macro is defined here
// (runtime/game_hooks.h); a game that lacks the macro has no such hook.
#pragma once

#include <cstdint>

// zlib (runtime/inflate.cpp)
#define GAME_HOOK_inflateInit2_ sub_825F2298
#define GAME_HOOK_inflate sub_825F24F8
#define GAME_HOOK_inflateEnd sub_825F3B80

// Language (games/ssa/language.cpp)
#define GAME_HOOK_Audio_SetLanguage sub_82864B90
#define GAME_HOOK_Movie_VoiceTrack sub_827F4400

// NGL engine and heap (engines/lux/patches.cpp)
#define GAME_HOOK_Ngl_SceneTraversal sub_82631098
#define GAME_HOOK_Ngl_ListSubmit sub_822895E0
#define GAME_HOOK_Heap_Free sub_828A25F8

// XMA (runtime/kernel/xma.cpp)
#define GAME_HOOK_Xma1_Init sub_82503F40
#define GAME_HOOK_Xma2_Init sub_82616A80
#define GAME_HOOK_Xma1_VoiceClear sub_82503FA0
#define GAME_HOOK_Xma1_VoiceKick sub_82504938
#define GAME_HOOK_Xma2_VoiceClear sub_82616B50
#define GAME_HOOK_Xma2_VoiceLock sub_82616D10
#define GAME_HOOK_Xma2_VoiceKick sub_82616EF8
#define GAME_HOOK_Xma1_LockAll sub_825047C8

// Portal (runtime/kernel/portal.cpp)
#define GAME_HOOK_Portal_Log sub_826434D8

// D3D (runtime/d3d/*.cpp)
#define GAME_HOOK_D3DDevice_BeginCommandBuffer sub_822A6BC8
#define GAME_HOOK_D3DDevice_EndCommandBuffer sub_822A6FB8
#define GAME_HOOK_D3DDevice_RunCommandBuffer sub_822A73D8
#define GAME_HOOK_D3DDevice_NextRingSegment sub_822A2D78
#define GAME_HOOK_D3DDevice_CreateVertexShader sub_82290508
#define GAME_HOOK_D3DDevice_CreatePixelShader sub_82290160
#define GAME_HOOK_D3DDevice_KickOff sub_822A3BB0
#define GAME_HOOK_D3DDevice_BlockOnFence sub_822A3CB8
#define GAME_HOOK_D3DDevice_InsertCallback sub_822A3DB0
#define GAME_HOOK_D3DDevice_DrawVertices sub_8229AD58
#define GAME_HOOK_D3DDevice_DrawIndexedVertices sub_8229B170
#define GAME_HOOK_D3DDevice_BeginVertices sub_8229A850
#define GAME_HOOK_D3DDevice_Resolve sub_822A1560
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_822ADDE0
#define GAME_HOOK_D3DDevice_Swap sub_822A95C0
#define GAME_HOOK_LightTile_ClassifyJob sub_821404D8

// Ultrawide (engines/lux/ultrawide.cpp, games/ssa/adapter.cpp)
#define GAME_HOOK_Ultrawide_CameraAspect sub_827F56A0
#define GAME_HOOK_Ultrawide_CreateTexture sub_829611D8
#define GAME_HOOK_Ultrawide_TextureHeader sub_823B38E0
#define GAME_HOOK_Ultrawide_LightManagerSetup sub_828924B8

// Bink (runtime/d3d/bink_display.cpp, engines/lux/movie.cpp)
#define GAME_HOOK_BinkDoFrame sub_829EDCF8
#define GAME_HOOK_Movie_Update sub_82852D90

// Text (games/ssa/stick_text.cpp): Text_Lookup(r3 the output string handle, r4 the text database, r5 the CRC-32 key)
// finds the raw UTF-8 text in three maps (db+32, db+16 the level's, db+0 the language's global text), then
// Text_Compile(r3 the raw text) copies it, expands $NAME$ and compiles the &ICON(...) markup (sub_82860798). Found from
// the "MissingText: %08x" string by hand.
#define GAME_HOOK_Text_Lookup sub_82861060
#define GAME_HOOK_Text_Compile sub_82860FC8

// Guest data
namespace game
{
    // D3D device fields from the vertex declaration on sit this many bytes later than in SSA's XDK (d3d/device.h).
    constexpr uint32_t DeviceVertexDeclaration = 0x2F58; // the current vertex declaration on the device
    // D3DDevice_DrawIndexedVertices takes a base vertex (r5) before the start index and count.
    constexpr bool DrawIndexedBaseVertex = true;
    // Render targets of a predicated-tiling pass cover all the tiles (d3d/render.cpp TargetFor). SSA renders without.
    constexpr bool ExpandTiledTargets = false;
    // D3DDevice_Resolve takes a destination level (r8) before the slice and the clear color.
    constexpr bool ResolveHasLevel = true;
    constexpr uint32_t DeviceStateShift = 0;
    constexpr uint32_t LanguageIndex = 0x82AA7048;
    constexpr uint32_t LanguageCodes = 0x82AA5EBC;
    constexpr uint32_t NglCurrentScene = 0x82BB4F4C; // scene list nglListSend handed to ListSendModule
    constexpr uint32_t Engine1CriticalSection = 0x82A3CC30;
    constexpr uint32_t UltrawideDisplayAspect = 0x82BB45EC;     // float: the video mode's width over its height
    constexpr uint32_t UltrawideSceneHeaderReturn = 0x82929FCC; // sub_82929EE8's call of the texture header setup
    // Ultrawide (d3d/ultrawide.h): the 3D scene's 16:9 size, and the pass that copies it into the frame (the composite):
    // one non-indexed triangle whose pixel constant c0 (VposParams) maps 1280 pixels to the scene, from any texture slot.
    constexpr uint32_t UltrawideSceneWidth = 1120, UltrawideSceneHeight = 704;
    constexpr bool UltrawideSceneKeepsArea = false; // d3d/ultrawide.h
    constexpr uint32_t UltrawideCompositeVertices = 3;
    constexpr int UltrawideCompositeSlot = -1;
    constexpr bool UltrawideCompositeVpos = true;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts: c0-c3, row vectors (Lux), checked
    // by the camera vote agreeing in 900 of 900 frames of level_008.
    constexpr int32_t UpscalePositionMatrix = 0;
    // The post pass whose constants and fetch offsets give its kernel in guest pixels: at scale N, in host pixels.
    constexpr uint64_t HostKernelPixelShader = 0; // no such pass
    // Where the pass keeps its contrast threshold in the pixel constants (a float index; the minimum follows): the edge smoothing setting.
    constexpr uint32_t HostKernelThreshold = 0; // no known FXAA pass
    constexpr uint32_t DeviceGlobal = 0x82BB4F94; // holds the device pointer
}
