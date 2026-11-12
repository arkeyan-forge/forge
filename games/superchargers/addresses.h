// Skylanders: SuperChargers: the guest functions the runtime hooks and the guest data it reads. Same names as the other
// games' addresses.h. A hook the game does not define is compiled out. A data global is a constexpr in namespace game, 0
// while the address is not known. Never reuse another game's address: every one here was re-found in SuperChargers' own
// binary (tools/sigmatch against Trap Team's and Swap Force's inventories, tools/xmatch.py shape as the second witness;
// games/superchargers/docs/research.md has the numbers).
#pragma once

// D3D (runtime/d3d/*.cpp): XDK D3D9 2.0.21256, Trap Team's. Ten are strict matches (1.0) of Trap Team's functions, and
// tools/xmatch.py offsets finds no device offset that moved (the few displacements that differ are global addresses);
// Resolve is sigmatch 0.62, shape 0.87 (1060 instructions against 1048), the only candidate above 0.2 [?].
#define GAME_HOOK_D3DDevice_NextRingSegment sub_82873EC0
#define GAME_HOOK_D3DDevice_KickOff sub_82874D18
#define GAME_HOOK_D3DDevice_BlockOnFence sub_82874E18
#define GAME_HOOK_D3DDevice_InsertCallback sub_82874F10
#define GAME_HOOK_D3DDevice_DrawVertices sub_82881158
#define GAME_HOOK_D3DDevice_DrawIndexedVertices sub_82881570
#define GAME_HOOK_D3DDevice_Resolve sub_8287FA48
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_8287EC80
#define GAME_HOOK_D3DDevice_Swap sub_828786C0
#define GAME_HOOK_D3DVertexShader_Init sub_8287D530
#define GAME_HOOK_D3DPixelShader_Init sub_8287D338

// XMA (runtime/kernel/xma.cpp): the same two engines. Xma1_Init, Xma1_VoiceKick, Xma2_VoiceLock, Xma2_VoiceKick and
// Xma1_LockAll are strict matches of Trap Team's (and of Swap Force's); Xma2_Init, Xma1_VoiceClear and Xma2_VoiceClear are
// loose (0.9) [?].
#define GAME_HOOK_Xma1_Init sub_835967D8
#define GAME_HOOK_Xma2_Init sub_827EFD18
#define GAME_HOOK_Xma1_VoiceClear sub_83596838
#define GAME_HOOK_Xma1_VoiceKick sub_835971D0
#define GAME_HOOK_Xma2_VoiceClear sub_8286DE98
#define GAME_HOOK_Xma2_VoiceLock sub_8286E040
#define GAME_HOOK_Xma2_VoiceKick sub_8286E2F0
#define GAME_HOOK_Xma1_LockAll sub_83597060

// The game's TLSF allocator (engines/alchemy/heap_guard.cpp), read from the code: add_pool makes the pool one free block
// and its end sentinel, free merges through the previous-block pointer and sub_82505C10 (the unlink), malloc and memalign
// search the segregated lists. Header: +0 previous block, +4 size (bit 0 free, bit 1 previous free), +8/+12 links.
#define GAME_HOOK_TlsfAddPool sub_82506260
#define GAME_HOOK_TlsfMalloc sub_82506330
#define GAME_HOOK_TlsfMemalign sub_825063A8
#define GAME_HOOK_TlsfFree sub_82506550

// LZMA (engines/alchemy/lzma.cpp): the LZMA SDK's LzmaDec_DecodeReal, strict against Trap Team's and Swap Force's.
#define GAME_HOOK_LzmaDec_DecodeReal sub_82506808

// Bink 2 (runtime/d3d/bink_display.cpp): BinkOpen sigmatch 0.58, shape 0.74 (38 instructions; the next candidate 0.31);
// BinkClose sigmatch 0.74, shape 0.91 (296 instructions) [?].
#define GAME_HOOK_BinkOpen sub_831C9218
#define GAME_HOOK_BinkClose sub_831C9F38

// SKY_LEVEL (engines/alchemy/zone_request.cpp): the zone list's lookup by name, (list, const char* name) -> the zone or 0. It copies
// the name into its own buffer, turns its backslashes into slashes and hashes it; "academy/academy" is looked up through it
// (sub_829C2078). Two callers matter: sub_82DF9528 enters the zone the game asked for next (a global string object, +44) and
// looks it up, returning to 0x82DF95E8; sub_82A229F0 looks up the zone being left (0x82A22CC0).
#define GAME_HOOK_Zone_Find sub_82DF1C58
// Entering the zone the game asked for next, (enter object) -> ...: it copies the request (game::ZoneRequest) into its object
// and looks the zone up (above); the loader then names the zone's archive and package from that name.
#define GAME_HOOK_Zone_Enter sub_82DF9528

// Languages (games/superchargers/language.cpp, boot.md "Text and voice language"): Language_Get is the title's one reader of the
// console language, XGetLanguage (sub_83567248) mapped to an IG_CORE_LANGUAGE value (6 English, 9 German) by a jump table,
// returned in r3: Swap Force's sub_823EFC68 instruction for instruction (sigmatch 0.62, fuzzy only for the table's
// addresses). Language_SoundPak opens one sound package of a sound set, "%s/%s_%s.pak" (the string's only user, and a
// caller of Language_Get). Movie_Open starts a movie (r3: the movie; its description at +32, the language's Bink track id at
// +56), calling BinkOpen: BinkOpen's only caller, Swap Force's shape (Trap Team's sigmatch 0.76 points at it too).
#define GAME_HOOK_Language_Get sub_824D7E10
#define GAME_HOOK_Language_SoundPak sub_8276BA98
#define GAME_HOOK_Movie_Open sub_826FB690

// Ultrawide (games/superchargers/ultrawide.cpp, docs/runtime/gpu/ultrawide.md): sub_826E8718 computes a render-target pass's
// size, Trap Team's layout (explicit +32/+36, scales +40/+44, result +92/+96; the only user of the targets' format names that
// Swap Force's sub_8258EBE8 has, "b8g8r8_tile_big_wii"...; Trap Team's sigmatch 0.53 put its candidate 0x38 before it). The
// projection builder sub_8250E610 (Trap Team's sigmatch 0.9): its world camera caller sub_82B0BC00 is Swap Force's
// sub_82B992A8 (shape 0.62, the next 0.32).
#define GAME_HOOK_Ultrawide_ScenePass sub_826E8718
#define GAME_HOOK_Ultrawide_Projection sub_8250E610
// The scene at full resolution (game::Adapter::fullScene): sub_82600C10 splits a set of targets into tiles past the console's
// 2048 EDRAM tiles: Swap Force's sub_825138F8 and Trap Team's sub_823EBCD0 instruction for instruction (shape 1.00).
#define GAME_HOOK_Scene_Tiles sub_82600C10
// The final composite (the scene into the frame) is an indexed quad, as Swap Force's.
#define GAME_ULTRAWIDE_COMPOSITE_INDEXED

// "Press Start" with the Start button's picture (engines/alchemy/prompt_text.cpp, games/superchargers/docs/art.md,
// "Button pictures"): Imaginators' pair, found the same way. The intern (r3 the table, *0x837DF7C4) is called by the
// TSTR helper sub_824AD660 (Swap Force's sub_82433D90, shape 1.00) and is Imaginators' sub_82485F28 instruction for
// instruction; the font's character lookup is Swap Force's sub_82553E48 (shape 0.82).
#define GAME_HOOK_String_Intern sub_82455A88
#define GAME_HOOK_Font_CharMetrics sub_826A4548

// The player's offline owner id (engines/alchemy/owner_id.cpp, docs/runtime/host/figures.md "Owner"), read from the code
// (the function map from .pdata): Owner_Fetch, the only user of the key "offline_owner_id", looks it up in the options
// save's key-value store and keeps it at +0xFC of the owner object (game::OwnerObject), else makes one (+0xF8's generator)
// and stores it; its one caller is the start flow's sub_82EF6D00. The toy checks read +0xFC (sub_82D78ED8 "is this toy
// mine": owner kind 1 against it, kind 0x80 against the Xbox Live id). Options_MostRecentSlot looks up the key
// "most_recent_save_slot" (sub_8299BF58 is its setter); its one caller is the start flow's sub_82EF6B40, after the load.
#define GAME_HOOK_Owner_Fetch sub_82D789B0
#define GAME_HOOK_Options_MostRecentSlot sub_8299BF10

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
        0x8370CE70; // sigmatch 1.0 (Trap Team's and Swap Force's), via Xma1_LockAll
    constexpr uint32_t DeviceGlobal = 0;
    constexpr uint32_t UltrawideDisplayAspect = 0;
    // The scene's 1040x620 (81 % of the frame, its root pass's explicit size: run scU1), as Swap Force's and Trap Team's.
    constexpr uint32_t UltrawideSceneWidth = 1040, UltrawideSceneHeight = 620;
    constexpr bool UltrawideSceneKeepsArea = true;
    constexpr uint32_t UltrawideCompositeVertices = 6;
    constexpr int UltrawideCompositeSlot = 0;
    constexpr bool UltrawideCompositeVpos = false;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts (Alchemy: c0-c3, row vectors, as
    // Giants'; checked by the camera vote's "[upscale] last 900 frames" line).
    constexpr int32_t UpscalePositionMatrix = 0;
    // The return address of the world camera's projection call (sub_82B0BC00), the one Ultrawide_Projection widens.
    constexpr uint32_t UltrawideWorldCameraCall = 0x82B0BCA4;
    // The post pass whose constants and fetch offsets give its kernel in guest pixels: at scale N, in host pixels. FXAA 3.11
    // into the 1040x620 scene (c0, c1 = +-0.5, 8, -4 over the size; c2 sharpness 8, c3 threshold 0.125, c4 minimum 0.05),
    // the layout of Trap Team's.
    constexpr uint64_t HostKernelPixelShader = 0x732669B14C8E3386ull;
    constexpr uint32_t HostKernelThreshold = 12; // the threshold (c3) and the minimum (c4), after c2 (sharpness)
    // SKY_LEVEL: the return address of the zone lookup that enters a zone, and the zone a new game enters after its intro
    // and the rift tunnel ride (the opening is intro/intro; the menus are ui/mainmenubackground/mainmenubackground).
    constexpr uint32_t ZoneEnterReturn = 0x82DF95E8;
    // The next zone the game asks for: the object at this global's pointer holds it at +44, a string object (one word, the
    // text, interned in the engine's string pool when copied; sub_824AD560 copies one).
    constexpr uint32_t ZoneRequest = 0x83764394, ZoneRequestName = 44;
    constexpr std::string_view ZoneAfterIntro = "intro1/intro1";
    constexpr std::string_view ZonePrefix = ""; // zones are <map>/<map>
    // The owner object's pointer and the id's offset in it (Owner_Fetch above).
    constexpr uint32_t OwnerObject = 0x83762794, OwnerIdOffset = 0xFC;
}
