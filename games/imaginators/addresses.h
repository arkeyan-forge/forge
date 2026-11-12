// Skylanders Imaginators: the guest functions the runtime hooks and the guest data it reads. Same names as the other
// games' addresses.h. A hook the game does not define is compiled out. A data global is a constexpr in namespace game, 0
// while the address is not known. Never reuse another game's address: every one here was re-found in Imaginators' own
// binary (tools/sigmatch against SuperChargers', Trap Team's and Swap Force's inventories, tools/xmatch.py as the second
// witness; games/imaginators/docs/research.md has the numbers).
#pragma once

// D3D (runtime/d3d/*.cpp): XDK D3D9 2.0.21256, Trap Team's and SuperChargers'. All eleven are strict matches (1.0) of
// SuperChargers' functions, Resolve included (Trap Team's: 0.62, shape 0.87); tools/xmatch.py offsets against Trap Team's
// ten others finds no device offset that moved (the displacements that differ are global addresses) [V].
#define GAME_HOOK_D3DDevice_NextRingSegment sub_82876E38
#define GAME_HOOK_D3DDevice_KickOff sub_82877C90
#define GAME_HOOK_D3DDevice_BlockOnFence sub_82877D90
#define GAME_HOOK_D3DDevice_InsertCallback sub_82877E88
#define GAME_HOOK_D3DDevice_DrawVertices sub_828840D0
#define GAME_HOOK_D3DDevice_DrawIndexedVertices sub_828844E8
#define GAME_HOOK_D3DDevice_Resolve sub_828829C0
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_82881BF8
#define GAME_HOOK_D3DDevice_Swap sub_8287B638
#define GAME_HOOK_D3DVertexShader_Init sub_828804A8
#define GAME_HOOK_D3DPixelShader_Init sub_828802B0

// XMA (runtime/kernel/xma.cpp): the same two engines. Xma1_Init, Xma1_VoiceKick, Xma2_VoiceLock, Xma2_VoiceKick and
// Xma1_LockAll are strict matches of SuperChargers', Trap Team's and Swap Force's; Xma2_Init, Xma1_VoiceClear and
// Xma2_VoiceClear are loose (0.9), the same candidate from each inventory [?].
#define GAME_HOOK_Xma1_Init sub_8366CCD8
#define GAME_HOOK_Xma2_Init sub_827F64A0
#define GAME_HOOK_Xma1_VoiceClear sub_8366CD38
#define GAME_HOOK_Xma1_VoiceKick sub_8366D6D0
#define GAME_HOOK_Xma2_VoiceClear sub_82870E10
#define GAME_HOOK_Xma2_VoiceLock sub_82870FB8
#define GAME_HOOK_Xma2_VoiceKick sub_82871268
#define GAME_HOOK_Xma1_LockAll sub_8366D560

// LZMA (engines/alchemy/lzma.cpp): the LZMA SDK's LzmaDec_DecodeReal, strict against all three inventories.
#define GAME_HOOK_LzmaDec_DecodeReal sub_8253F2C0

// Bink 2 (runtime/d3d/bink_display.cpp): BinkOpen and BinkClose loose (0.9) against SuperChargers' (Trap Team's: 0.58 and
// 0.74, the same candidates) [?].
#define GAME_HOOK_BinkOpen sub_8329F798
#define GAME_HOOK_BinkClose sub_832A04B8

// SKY_LEVEL (engines/alchemy/zone_request.cpp): the zone list's lookup by name, (list, const char* name) -> the zone or 0, a
// strict shape match (1.00, 70 instructions) of SuperChargers' sub_82DF1C58 (tools/xmatch.py shape). Entering the zone the game
// asked for next: SuperChargers' sub_82DF9528 matches sub_82D82B50 (0.66), which reads the request global +44 and calls the
// lookup above (the call at 0x82D82C1C), as SuperChargers' does [V].
#define GAME_HOOK_Zone_Find sub_82D7B450
#define GAME_HOOK_Zone_Enter sub_82D82B50

// Languages (games/imaginators/language.cpp, games/imaginators/docs/boot.md "Text and voice language"): Language_Get, XGetLanguage
// (sub_8363D968, ExGetXConfigSetting 3, 9) to an IG_CORE_LANGUAGE value by a jump table, SuperChargers' sub_824D7E10 case for
// case (French 4 -> 8, Dutch 16 -> 5), the same byte global after it. The two users of "%s/%s_%s.pak" (0x823CA3C0) open a
// sound set's localized package, each asking Language_Get. Movie_Open is BinkOpen's only caller (the description at +32,
// the language's Bink track id at +56, passed to BinkSetSoundTrack), SuperChargers' shape.
#define GAME_HOOK_Language_Get sub_8250C898
#define GAME_HOOK_Language_SoundPak sub_8277EB80
#define GAME_HOOK_Language_SoundPak2 sub_8277F000
#define GAME_HOOK_Movie_Open sub_8270DAE0

// Ultrawide (games/imaginators/ultrawide.cpp, SuperChargers'; docs/runtime/gpu/ultrawide.md): SuperChargers' sub_826E8718
// is sub_826F96D0 (shape 0.87; tools/xmatch.py offsets: no structure displacement differs, +32/+36, +40/+44, +92/+96
// kept), sub_82600C10 (the scene's tiles) is sub_82614310 (1.00), the world camera sub_82B0BC00 is sub_82BA8430 (1.00),
// whose call at the same place (0x82BA84D0) goes to the projection builder sub_82547C30 (its frustum code inlined, hence
// shape 0.30; the same arguments).
#define GAME_HOOK_Ultrawide_ScenePass sub_826F96D0
#define GAME_HOOK_Ultrawide_Projection sub_82547C30
#define GAME_HOOK_Scene_Tiles sub_82614310
#define GAME_ULTRAWIDE_COMPOSITE_INDEXED

// Nightmare difficulty (engines/alchemy/nightmare.cpp): (storage) -> whether the save's "nightmare_difficulty_unlocked"
// property is 1. Swap Force's sub_829CC788 instruction for instruction (the slot's data at +44, not +40); the one reader of
// the property (its writer, sub_829C4350, is the script's unlockNightmareDifficultyLevel). Its two callers are the
// difficulty menu's (sub_8317D680, sub_8317D888): the fourth portrait is locked while it answers 0.
#define GAME_HOOK_NightmareUnlocked sub_829BE430

// "Press Start" with the Start button's picture (engines/alchemy/prompt_text.cpp, games/imaginators/docs/art.md,
// "Button pictures"). The string table's intern (r3 the table, *0x83884D98; r4 a C string; returns the shared copy):
// the IGZ loader's TSTR helper sub_824E3180 is Swap Force's sub_82433D90 (shape 1.00) and calls it; the intern itself
// was rewritten (shape 0.19: a refcounted header before each copy) with the same arguments and result. The font's
// character lookup: Swap Force's sub_82553E48 (shape 0.82, the same lists at +0x40 and +0x44; a missing character falls
// back to a global, not "_").
#define GAME_HOOK_String_Intern sub_82485F28
#define GAME_HOOK_Font_CharMetrics sub_826AE8E0

#include <cstdint>
#include <string_view>

namespace game
{
    // D3D: the offsets the runtime reads, Trap Team's (Giants'), carried by the strict matches above [?].
    constexpr uint32_t DeviceVertexDeclaration = 0x2FD0;
    constexpr bool DrawIndexedBaseVertex = false; // DrawIndexedVertices: (device, primitive, start, count)
    constexpr bool ExpandTiledTargets = true;
    constexpr bool ResolveHasLevel = false; // Resolve: slice r8, color r9
    constexpr uint32_t DeviceStateShift = 0x80;
    constexpr uint32_t LanguageIndex = 0;
    constexpr uint32_t LanguageCodes = 0;
    constexpr uint32_t NglCurrentScene = 0;
    constexpr uint32_t Engine1CriticalSection =
        0x837AA970; // sigmatch 1.0 (Trap Team's and Swap Force's), via Xma1_LockAll
    constexpr uint32_t DeviceGlobal = 0;
    constexpr uint32_t UltrawideDisplayAspect = 0;
    // The scene's 1040x620, SuperChargers' (its root pass's explicit size) [?].
    constexpr uint32_t UltrawideSceneWidth = 1040, UltrawideSceneHeight = 620;
    constexpr bool UltrawideSceneKeepsArea = true;
    constexpr uint32_t UltrawideCompositeVertices = 6;
    constexpr int UltrawideCompositeSlot = 0;
    constexpr bool UltrawideCompositeVpos = false;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts (Alchemy: c0-c3, row vectors, as
    // Giants'; checked by the camera vote's "[upscale] last 900 frames" line).
    constexpr int32_t UpscalePositionMatrix = 0;
    constexpr uint32_t UltrawideWorldCameraCall = 0x82BA84D4; // the return of sub_82BA8430's projection call
    constexpr uint64_t HostKernelPixelShader = 0;
    constexpr uint32_t HostKernelThreshold = 0;
    // SKY_LEVEL: the return address of the zone lookup that enters a zone; the request global (its object holds the next
    // zone's name at +44, a string object, as SuperChargers'); the first zone a new game enters after the menus.
    constexpr uint32_t ZoneEnterReturn = 0x82D82C20;
    constexpr uint32_t ZoneRequest = 0x837E7A64, ZoneRequestName = 44;
    constexpr std::string_view ZoneAfterIntro = "2016/level_508/level_508"; // the first level, its opening inside
    constexpr std::string_view ZonePrefix =
        "2016/"; // zones are 2016/<map>/<map>: 2016/hub_2016/hub_2016, 2016/level_508/level_508
}

// The portal library's signature check (games/imaginators/docs/toybox.md, "Signature"): sub_831FD948(work, data, 96, key)
// verifies a 2016 toy's factory signature; `data` is blocks 2, 4, 34 and 62 (the signature) then blocks 0 and 1, and 0 means
// verified. Two callers: the tag check when the toy arrives (sub_831F27C0: result+8 bit 0x80000000) and the check before
// a write (sub_831F4238); without it the toy is "maybe corrupted, repair it". Found by the signature's words read in a run
// (a load watch: sub_831FC850, the hash under it), then up the calls [V].
#define GAME_HOOK_Portal_TagSignatureVerify sub_831FD948

// A created character's HUD portrait (#47, games/imaginators/docs/research.md, "The created character's portrait"):
// sub_82A328F8 (the job's "cachedScene", "standard_renderer") draws the character into the frame, resolves it to a
// 1280x720 texture, then copies a 480x480 crop of that texture's memory into an igImage2 with sub_8268CB80 (source,
// destination: the engine's texture objects, their D3D texture at +4; r5 the rectangle), shrunk to 128x128 on the CPU
// (sub_82642188) for the HUD's material. Its only caller is sub_8268AD50, called there [V: runs of 2026-11-11].
#define GAME_HOOK_Texture_CpuCopy sub_8268CB80

// The XDK's resource lock (runtime/d3d/resource_lock.cpp): (resource, ..., address, ..., flags in r8) -> the CPU address;
// with D3DLOCK_READONLY (0x10) it returns 0xC0000000 + the physical address (the cached view, `addis r25, r11, -0x4000` at
// 0x828757A4), which does not alias the allocation here. Called by the texture lock sub_828799B0 (flags 0x10 from the
// portrait's texture-to-image download, sub_8268C860) and seven other engine functions [V: run of 2026-11-11, #47].
#define GAME_HOOK_D3D_LockResource sub_828754C0
