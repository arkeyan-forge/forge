// Skylanders: Swap Force: the guest functions the runtime hooks and the guest data it reads. Same names as
// games/ssa/addresses.h and games/giants/addresses.h. It starts empty: no hook exists in a Swap Force build until its
// line is added here, e.g.
//     #define GAME_HOOK_D3DDevice_DrawVertices sub_XXXXXXXX
// A data global is a constexpr in namespace game, 0 while the address is not known. Never reuse another game's address:
// every one is re-found in Swap Force's own binary (docs/porting/day1.md).
#pragma once

// D3D (runtime/d3d/*.cpp): XDK D3D9 2.0.21250 (not LTCG), found with tools/xmatch.py against Giants (`.:giants`) and SSA
// (games/swapforce/docs/boot.md, "Direct3D"). As in Giants: no command buffers, no Create*Shader (the shader Init
// functions instead), no BeginVertices (it exists, sub_82F64CF8, shape 1.00 against SSA's, but only D3D's own Swap
// helper sub_82F5C4B0 calls it).
#define GAME_HOOK_D3DDevice_NextRingSegment sub_82F58760 // same code as Giants' sub_823D0DB8 (asm side by side)
#define GAME_HOOK_D3DDevice_KickOff sub_82F59840 // call graph from VdInitializeRingBuffer's caller (SSA symbols)
#define GAME_HOOK_D3DDevice_BlockOnFence                                                                               \
    sub_82F58460 // call graph (SSA symbols); takes one more argument (r7) than Giants'
#define GAME_HOOK_D3DDevice_InsertCallback sub_82F58EF0 // shape 1.00 against Giants', same callees
#define GAME_HOOK_D3DDevice_DrawVertices sub_82F651B8 // mapped callees, same size 0x418
#define GAME_HOOK_D3DDevice_DrawIndexedVertices                                                                        \
    sub_82F655D0 // same size 0x478; base vertex back in r5 (VGT_INDX_OFFSET)
#define GAME_HOOK_D3DDevice_Resolve                                                                                    \
    sub_82F64AB0 // the public Resolve: SSA's arguments, forwarded to the worker sub_82F63958
#define GAME_HOOK_D3DDevice_ClearF_Worker sub_82F626B0 // shape 1.00 against Giants' and SSA's, same size 0x680
#define GAME_HOOK_D3DDevice_Swap sub_82F5D018 // only caller of VdSwap
#define GAME_HOOK_D3DVertexShader_Init sub_82F527F8 // identical to Giants' sub_823DC7E8
#define GAME_HOOK_D3DPixelShader_Init sub_82F51A10 // identical to Giants' sub_823DC5F0

// XMA (runtime/kernel/xma.cpp): the same two engines as SSA and Giants (shape similarity 1.00 each). Unlike Giants, engine
// 2 has its own init again (sub_82F9FA20 reads 0x7FEA1800 like engine 1's; called from the voice set-up sub_8268B9F0,
// Giants' sub_823561C0).
#define GAME_HOOK_Xma1_Init sub_82FAB058
#define GAME_HOOK_Xma2_Init sub_82F9FA20
#define GAME_HOOK_Xma1_VoiceClear sub_82FAB0B8
#define GAME_HOOK_Xma1_VoiceKick sub_82FABA50
#define GAME_HOOK_Xma2_VoiceClear sub_82F9FAF0
#define GAME_HOOK_Xma2_VoiceLock sub_82F9FCF0
#define GAME_HOOK_Xma2_VoiceKick sub_82F9FED8
#define GAME_HOOK_Xma1_LockAll sub_82FAB8E0

// LZMA (engines/alchemy/lzma.cpp): the LZMA SDK's LzmaDec_DecodeReal (9.x), the inner loop of every archive chunk's
// decode, found by its constants (kTopValue lis 256, the literal table at probs + 3692 bytes, kMatchSpecLenStart 274);
// the same code, byte for byte in its asm, in Giants and Swap Force.
#define GAME_HOOK_LzmaDec_DecodeReal sub_82476EF8

// Bink 2 (runtime/d3d/bink_display.cpp): BinkOpen calls the header reader sub_83081CE8 (shape 1.00 against Giants');
// BinkClose sets HBINK+76, then frees (0.94 against Giants', the same first 30 instructions).
#define GAME_HOOK_BinkOpen sub_83083260
#define GAME_HOOK_BinkClose sub_83084128

// SKY_LEVEL (engines/alchemy/zone.cpp): the zone list's lookup by name, (list, const char* name) -> index or -1; it
// hashes the name (sub_8295FE60) and looks it up (sub_82461980). Found from the level loader sub_829EC0F0, which reads
// the zone's archives (the "generated/maps/%s_characters" format) after this lookup.
#define GAME_HOOK_Zone_IndexByName sub_829EA688

// The save tasks (games/swapforce/save_tasks.cpp): the storage object's poll, (storage) -> busy; it finishes a task once
// its worker thread set the done flag (game::SaveTaskDone). Its start, sub_829CF218, clears that flag after starting
// the thread, so a worker that ends first loses it (the stop at the save slots).
#define GAME_HOOK_SaveTask_Poll sub_829CF1C0

// The render thread's wait for its last swap (games/swapforce/swap_wait.cpp): (device, out) -> out[2] = swaps not shown
// yet; its only caller, the scene's end sub_82515D68, spins on it before D3DDevice_Swap.
#define GAME_HOOK_SwapWait_Status sub_82F5CB30

// Nightmare difficulty (engines/alchemy/nightmare.cpp): (storage) -> whether the save's "nightmare_difficulty_unlocked"
// property is 1 (a script variable of the slot that the story's end sets through tryUnlockNightmareDifficultyLevel). Its one
// caller, the difficulty menu's availability check sub_82C12BC0 (difficulty index >= 3 asks it), shows the padlock and "Beat
// the game to unlock Nightmare difficulty." while it answers 0.
#define GAME_HOOK_NightmareUnlocked sub_829CC788

// The jump component's stuck-falling check (games/swapforce/fall_check.cpp): (component) -> void, called every frame; it
// compares the squared distance moved since the last frame (the position at +144..+152 against the object's) with
// (the config's threshold at +92 x the float at +104 of the component)^2. Found by its reflection names and read in the
// recompiled code; the same address as the other Swap Force port's note, checked here.
#define GAME_HOOK_FallCheck sub_82A4A828

// Languages (games/swapforce/language.cpp, boot.md "Text and voice language"): Language_Get is the title's one reader of the
// console language, XGetLanguage mapped to an IG_CORE_LANGUAGE value (6 English, 8 French), returned in r3; Language_SoundPak
// opens one sound package of a sound set ("<set>_<abbreviation>.pak", the abbreviation from that same value); Movie_Open
// starts a movie (r3: the movie; its description at +32, the language's Bink track id at +56 of the description), calling
// BinkSetSoundTrack sub_8307EC40 then BinkOpen. Found by hand (the strings "%s/%s_%s.pak", "LocalizationData.xmlb" and the
// call graph); Giants' counterparts are not in the same place (tools/xmatch.py shape: 0.55 for Movie_Open, none for the rest).
#define GAME_HOOK_Language_Get sub_823EFC68
#define GAME_HOOK_Language_SoundPak sub_825F5B48
#define GAME_HOOK_Movie_Open sub_825A71D0

// The string table's intern (engines/alchemy/prompt_text.cpp, art.md "Button pictures"): r3 the table (*0x83403DB0), r4
// a C string; returns the shared copy, looked up by content (sub_823EBFF0) or allocated and copied (sub_823EC3B8,
// memcpy). Every string of the IGZ files (the TSTR records, sub_82433D90) and of the igx text goes through it. Found
// from the IGZ loader's TSTR case (sub_82459898) by hand.
#define GAME_HOOK_String_Intern sub_823EC0D8
// A font's character lookup (engines/alchemy/prompt_text.cpp): r3 the igBitmapFont, r4 a u16* character code, r5 a u8*
// set to 1 when the character is in the alternate list (drawn from the font's "button" image); returns the
// igCharMetrics or 0. A binary search of the main list (+0x40), then the alternate one (+0x44); nothing cached. Found
// from the CPU text renderer (sub_82557560, sub_82557A48) by hand.
#define GAME_HOOK_Font_CharMetrics sub_82553E48

// Ultrawide and the full-resolution scene (games/swapforce/ultrawide.cpp, docs/runtime/gpu/ultrawide.md "Swap Force"): a
// render-target pass computes its size in sub_8258EBE8 (an explicit size, its parent's or the display's, times its scales);
// the scene's chain starts at a pass with an explicit 1040x620 (81 % of 1280x720) and its children, half, quarter and the
// reductions, read their parent's; the world camera's set-up (sub_82B992A8) reads the
// display's aspect from its viewport object (+8: 16:9). The full-screen composite and the HUD are drawn by cameras that
// read the same aspect and need 16:9, so only the world camera gets the window's. Giants' Ultrawide_* hooks are not in this
// engine version (shape similarity under 0.2); these were found with a store watch (docs/porting/debugging.md).
#define GAME_HOOK_Ultrawide_ScenePass sub_8258EBE8
#define GAME_HOOK_Ultrawide_WorldCamera sub_82B992A8
// The scene at full resolution (game::Adapter::fullScene): sub_825138F8 splits a set of targets into tiles when they need
// more than the console's 2048 EDRAM tiles (10 MB; the 1280x720 G-buffer does): the hook keeps them in one piece.
// sub_82516EB8 counts a surface's EDRAM tiles; the scene's EDRAM plan (sub_82593870, which calls it) gets the original
// size's count, so every pass keeps the console's EDRAM base (games/swapforce/docs/boot.md, "The scene at full resolution").
#define GAME_HOOK_Scene_Tiles sub_825138F8
#define GAME_HOOK_Scene_SurfaceTiles sub_82516EB8
// The final composite (the scene into the frame) is an indexed quad here (6 indices), not Giants' 4 vertices.
#define GAME_ULTRAWIDE_COMPOSITE_INDEXED

#include <cstdint>
#include <string_view>

namespace game
{
    // D3D: XDK 2.0.21250 (D3D9). The device layout is Giants' (XDK 21076): every offset the runtime reads was checked in
    // Swap Force's own setters and draws (games/swapforce/docs/boot.md, "Direct3D").
    constexpr uint32_t DeviceVertexDeclaration = 0x2FD0; // SetVertexDeclaration sub_82F51E68: stw r4,0x2FD0(r3)
    constexpr bool DrawIndexedBaseVertex = true; // (device, primitive, base vertex, start, count), as SSA
    constexpr bool ExpandTiledTargets = true;
    constexpr bool ResolveHasLevel = true; // the public Resolve has SSA's (.., level, slice, color) again
    constexpr uint32_t DeviceStateShift = 0x80;
    constexpr uint32_t LanguageIndex = 0;
    constexpr uint32_t LanguageCodes = 0;
    constexpr uint32_t NglCurrentScene = 0;
    constexpr uint32_t Engine1CriticalSection = 0x831106E4; // Xma1_LockAll enters it first
    constexpr uint32_t DeviceGlobal = 0;
    constexpr uint32_t UltrawideDisplayAspect = 0;
    constexpr uint32_t UltrawideSceneWidth = 1040, UltrawideSceneHeight = 620;
    constexpr bool UltrawideSceneKeepsArea = true;
    constexpr uint32_t UltrawideCompositeVertices = 6;
    constexpr int UltrawideCompositeSlot = 0;
    constexpr bool UltrawideCompositeVpos = false;
    // Upscaling (d3d/upscale.h): the vertex constant where the position matrix starts: c0-c3, row vectors (Alchemy, as
    // Giants'; the camera vote's "[upscale] last 900 frames" line checks it).
    constexpr int32_t UpscalePositionMatrix = 0;
    // The post pass whose constants and fetch offsets give its kernel in guest pixels: at scale N, in host pixels.
    constexpr uint64_t HostKernelPixelShader = 0x123B15C5DE8896D4ull; // FXAA 3.11 (games/swapforce/docs/shaders.md)
    // Where the pass keeps its contrast threshold in the pixel constants (a float index; the minimum follows): the edge smoothing setting.
    constexpr uint32_t HostKernelThreshold =
        12; // FXAA 3.11's threshold (c3) and minimum (c4), after c2 (sharpness): games/swapforce/docs/shaders.md
    constexpr std::string_view FirstZone = "tutorial\\tutorial_a\\tutorial_a"; // a new game's first zone (SKY_LEVEL)
    // The engine's name table (sub_8294EA00 turns a name handle into its string, sub_82954BA0 returns the table, sub_82954080
    // clears it): a u32 generation, 32768 u32 string offsets, at +0x20004 the u32 end of the strings, at +0x20008 the
    // strings, packed and NUL-terminated. A new game's first zone is the table's last string when it is looked up, so
    // SKY_LEVEL can write a longer zone name there and move the end (engines/alchemy/zone.cpp).
    constexpr uint32_t NameTable = 0x831379A8;
    // The game mode (SKY_LEVEL in a survival arena, engines/alchemy/zone.cpp). The game object is a function-local
    // static (sub_829B05A8 returns it, built before the title); it holds the mode at +1184 (sub_82989648 reads it;
    // SetGameMode sub_829A8928 stores it and tells the mode's systems) and the next zone's mode at +1188 (sub_82989658
    // stores it). Entering a zone (sub_829C6220) keeps the mode at +1180 and calls SetGameMode with the next one.
    // Logged from the menus [V]: Story Mode 1, Arena Modes > Solo Survival 64 (sub_82BDE3E8, from the menu item).
    // A survival arena's objective (sub_82A621F8, the component's start) builds its rules object at +28 only in modes 4,
    // 32, 64, 128 and 256; in Story Mode it stays null and the arena faults at its first wave (sub_82A5D168 reads it).
    constexpr uint32_t GameInstance = 0x83209B18;
    constexpr uint32_t GameModeOffset = 1184, NextGameModeOffset = 1188, GameModeSoloSurvival = 64;
    constexpr std::string_view SurvivalZones = "multiplayer\\survival\\"; // the survival arenas' zone names
    // The save tasks ("SL_LOAD_SLOTS", "SL_SAVE_OPTIONS", ...: sub_829CC7F0 names the thread by task): the storage object
    // has its busy byte at +185 and its igThread at +188 (that thread's "running" byte at +16, set before the thread is
    // resumed and cleared by the thread's trampoline sub_823F3B70 after the worker returns); every worker sets the done
    // byte as its last store (sub_829CAA90 ...).
    constexpr uint32_t FallCheckScaleOffset =
        104; // the float multiplying the stuck-falling threshold in a jump component
    constexpr uint32_t SaveTaskDone = 0x8320ABC1;
    constexpr uint32_t SaveTaskBusyOffset = 185, SaveTaskThreadOffset = 188, ThreadRunningOffset = 16;
    // The D3D device's swap counts (games/swapforce/swap_wait.cpp): issued (D3DDevice_Swap) and shown (the vertical
    // blank callback sub_82F5C128, at a pending swap's blank).
    constexpr uint32_t SwapsIssuedOffset = 16968, SwapsShownOffset = 16976;
}
