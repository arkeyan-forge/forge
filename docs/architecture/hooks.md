# Hooks: what is hooked, where it lives, which game has it

> **Purpose:** which guest functions does the runtime replace, in which file, for which game, and what does the per-game adapter provide? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK] [Lux] [Alchemy] [SSA] [Giants] [SwapForce]

A **hook** is a C++ function that replaces (or wraps) one recompiled guest function: `PPC_FUNC(GAME_HOOK(Name))` in a
source file, active only when the game's `games/<id>/addresses.h` defines `GAME_HOOK_<Name>` as that function's symbol
(`runtime/game_hooks.h`). A game that lacks the macro simply has no such hook: the hook source stays in the build and
compiles to nothing. Besides hooks, `addresses.h` holds guest data addresses and per-XDK constants (`namespace game`).

## The three classes

| Class | What | Lives in | Addresses from | Count |
|---|---|---|---|---|
| **a** Xbox / XDK / middleware | D3D, XMA, Bink, zlib, the portal library: the same guest library in every game, hooked the same way | `runtime/` | `games/<id>/addresses.h` | 32 |
| **b** Engine | Behaviour of one engine: Lux (NGL, the heap, the movie player, the light tiles, the scene targets) and Alchemy (the scene targets' size) | `engines/<engine>/` | `games/<id>/addresses.h` | 13 (Lux 8, Alchemy 5) |
| **c** Game adapter | What only one game does or has; reached through the interface `runtime/game.h` | `games/<id>/` | `games/<id>/addresses.h` | 5 |

Total: **58 distinct hook names. SSA defines 39, Giants 29, Swap Force 30** (counted 2026-10-27, after the full-resolution scene). Recount:
`grep -rhoE 'GAME_HOOK\(\w+\)' runtime engines games | sort -u` (drop the `name`/`Name` of comments) and
`grep -c '^#define GAME_HOOK_' games/<id>/addresses.h`.

A class-**a** hook is a candidate for the same code in the next game of the family: it needs only the address (and, for the
D3D ones, the XDK differences in `namespace game`: `DeviceStateShift`, `DrawIndexedBaseVertex`...). A class-**b** hook is
shared by every game on the engine. A class-**c** hook is written again per game.

## The adapter interface (`runtime/game.h`)

Every game implements `game::Current()` in `games/<id>/adapter.cpp`, returning a `game::Adapter` with 7 members:

| Member | Meaning | SSA | Giants |
|---|---|---|---|
| `info` | id, display name, accepted disc releases, executable (`games/<id>/info.cpp`) | yes | yes |
| `ReadPortalPrompt` | the game's "place a Skylander" prompt, read from guest memory every frame ([portal.md](../runtime/host/portal.md)) | `games/ssa/adapter.cpp` + `ssa_portal_prompt.*` | `games/giants/adapter.cpp` + `giants_portal_prompt.*` (script variables found by a scan) |
| `ultrawide` | the adapter hooks the camera aspect, so ultrawide may widen the picture ([ultrawide.md](../runtime/gpu/ultrawide.md)) | true (`Ultrawide_CameraAspect`) | true (`Ultrawide_CameraAspect`) |
| `SetVoiceLanguage` | voices in another language than the text | `ssa::SetVoiceLanguage` (`games/ssa/language.cpp`) | `giants::SetVoiceLanguage` (`games/giants/language.cpp`) |
| `glyphTextures` | the game's Xbox 360 button glyph textures, redrawn for the device in use (`host/glyphs.cpp`, [input.md](../runtime/xbox360/input.md)) | yes | yes |
| `introMovies` | the start-up logos `[video] skip_intro_movies` skips | yes | yes |
| `presentsAtEveryBlank` | the game presents at every vblank by its own configuration, so `fps_limit 30` shows every second frame (`kernel/video.cpp`) | false (default; Lux asks for interval 2) | true (Alchemy's `presentationInterval = 1`) |

The interface stays this small on purpose: a new member needs a second game that uses it.

## Every hook

| Hook | Class | File | SSA | Giants | Swap Force | What it does |
|---|---|---|---|---|---|---|
| `BinkClose` | a | `runtime/d3d/bink_display.cpp` | - | `sub_82155B08` | `sub_83084128` | The movie is over (ultrawide's bars) |
| `BinkDoFrame` | a | `runtime/d3d/bink_display.cpp` | `sub_829EDCF8` | - | - | Shows each decoded Bink frame in the window before the renderer exists |
| `BinkOpen` | a | `runtime/d3d/bink_display.cpp` | - | `sub_821550A0` | `sub_83083260` | Bink 2 header read; `SKY_SKIP_MOVIES=1` makes every movie one frame long |
| `D3DDevice_BeginCommandBuffer` | a | `runtime/d3d/command_buffers.cpp` | `sub_822A6BC8` | - | - | XDK command buffers: record |
| `D3DDevice_EndCommandBuffer` | a | `runtime/d3d/command_buffers.cpp` | `sub_822A6FB8` | - | - | XDK command buffers: end the recording |
| `D3DDevice_RunCommandBuffer` | a | `runtime/d3d/command_buffers.cpp` | `sub_822A73D8` | - | - | XDK command buffers: replay |
| `D3DDevice_BlockOnFence` | a | `runtime/d3d/null_gpu.cpp` | `sub_822A3CB8` | `sub_823D1D10` | `sub_82F58460` | No GPU command processor: fence waits return at once (needs KickOff too) |
| `D3DDevice_InsertCallback` | a | `runtime/d3d/null_gpu.cpp` | `sub_822A3DB0` | `sub_823D1E08` | `sub_82F58EF0` | Runs the title's GPU callbacks on the host |
| `D3DDevice_KickOff` | a | `runtime/d3d/null_gpu.cpp` | `sub_822A3BB0` | `sub_823D1C10` | `sub_82F59840` | No GPU command processor: kick-off handled by the host |
| `D3DDevice_BeginVertices` | a | `runtime/d3d/render.cpp` | `sub_8229A850` | - | - | Inline vertex data for a draw |
| `D3DDevice_ClearF_Worker` | a | `runtime/d3d/render.cpp` | `sub_822ADDE0` | `sub_823DDF38` | `sub_82F626B0` | Clears of render targets and depth |
| `D3DDevice_DrawIndexedVertices` | a | `runtime/d3d/render.cpp` | `sub_8229B170` | `sub_823E07F8` | `sub_82F655D0` | Indexed draw call (SSA and Swap Force pass a base vertex, Giants does not: `game::DrawIndexedBaseVertex`) |
| `D3DDevice_DrawVertices` | a | `runtime/d3d/render.cpp` | `sub_8229AD58` | `sub_823E03E0` | `sub_82F651B8` | Draw call: reads the device state, draws on the host GPU |
| `D3DDevice_Resolve` | a | `runtime/d3d/render.cpp` | `sub_822A1560` | `sub_823DED00` | `sub_82F64AB0` | Resolve (copy a render target to a texture) |
| `D3DDevice_Swap` | a | `runtime/d3d/render.cpp` | `sub_822A95C0` | `sub_823DA640` | `sub_82F5D018` | Frame boundary: present, frame pacing, per-frame bookkeeping |
| `D3DDevice_NextRingSegment` | a | `runtime/d3d/ring.cpp` | `sub_822A2D78` | `sub_823D0DB8` | `sub_82F58760` | Command ring: next segment |
| `D3DDevice_CreatePixelShader` | a | `runtime/d3d/shaders.cpp` | `sub_82290160` | - | - | Pixel shader creation: finds the translated host shader |
| `D3DDevice_CreateVertexShader` | a | `runtime/d3d/shaders.cpp` | `sub_82290508` | - | - | Vertex shader creation: finds the translated host shader |
| `D3DPixelShader_Init` | a | `runtime/d3d/shaders.cpp` | - | `sub_823DC5F0` | `sub_82F51A10` | Pixel shader init (Alchemy builds its shader objects itself) |
| `D3DVertexShader_Init` | a | `runtime/d3d/shaders.cpp` | - | `sub_823DC7E8` | `sub_82F527F8` | Vertex shader init (Alchemy builds its shader objects itself) |
| `inflate` | a | `runtime/inflate.cpp` | `sub_825F24F8` | - | - | zlib: the host's inflate replaces the game's |
| `inflateEnd` | a | `runtime/inflate.cpp` | `sub_825F3B80` | - | - | zlib: the host's inflate replaces the game's |
| `inflateInit2_` | a | `runtime/inflate.cpp` | `sub_825F2298` | - | - | zlib: the host's inflate replaces the game's |
| `Portal_Log` | a | `runtime/kernel/portal.cpp` | `sub_826434D8` | - | - | The Skylanders portal library's log, echoed with `SKY_PORTAL_LOG`. Shared by the Skylanders games, but it needs the kernel's `FormatGuestString`, so it stays in `runtime/kernel/portal.cpp` |
| `Xma1_Init` | a | `runtime/kernel/xma.cpp` | `sub_82503F40` | `sub_82436480` | `sub_82FAB058` | XMA decoder (XMA1): init |
| `Xma1_LockAll` | a | `runtime/kernel/xma.cpp` | `sub_825047C8` | `sub_82436D08` | `sub_82FAB8E0` | XMA1 lock all voices (takes the engine's critical section, `game::Engine1CriticalSection`) |
| `Xma1_VoiceClear` | a | `runtime/kernel/xma.cpp` | `sub_82503FA0` | `sub_824364E0` | `sub_82FAB0B8` | XMA1 voice clear |
| `Xma1_VoiceKick` | a | `runtime/kernel/xma.cpp` | `sub_82504938` | `sub_82436E78` | `sub_82FABA50` | XMA1 voice kick: decode on the host |
| `Xma2_Init` | a | `runtime/kernel/xma.cpp` | `sub_82616A80` | `sub_823561C0` | `sub_82F9FA20` | XMA decoder (XMA2): init |
| `Xma2_VoiceClear` | a | `runtime/kernel/xma.cpp` | `sub_82616B50` | `sub_823CD048` | `sub_82F9FAF0` | XMA2 voice clear |
| `Xma2_VoiceKick` | a | `runtime/kernel/xma.cpp` | `sub_82616EF8` | `sub_823CD4A0` | `sub_82F9FED8` | XMA2 voice kick: decode on the host |
| `Xma2_VoiceLock` | a | `runtime/kernel/xma.cpp` | `sub_82616D10` | `sub_823CD1F0` | `sub_82F9FCF0` | XMA2 voice lock |
| `LightTile_ClassifyJob` | b | `engines/lux/light_tiles.cpp` | `sub_821404D8` | - | - | Which scene depth resolve the light tile job reads (written back to guest memory) |
| `Movie_Update` | b | `engines/lux/movie.cpp` | `sub_82852D90` | - | - | The movie player: `SKY_SKIP_MOVIES`, `[video] skip_intro_movies` |
| `Heap_Free` | b | `engines/lux/patches.cpp` | `sub_828A25F8` | - | - | The main heap's free: skips a free that would corrupt the heap ([boot_saves.md](../engines/lux/boot_saves.md)) |
| `Ngl_ListSubmit` | b | `engines/lux/patches.cpp` | `sub_822895E0` | - | - | NGL: skips an empty render list |
| `Ngl_SceneTraversal` | b | `engines/lux/patches.cpp` | `sub_82631098` | - | - | NGL: skips an empty scene list |
| `Ultrawide_CreateTexture` | b | `engines/lux/ultrawide.cpp` | `sub_829611D8` | - | - | Ultrawide: widens the 16:9 scene targets |
| `Ultrawide_LightManagerSetup` | b | `engines/lux/ultrawide.cpp` | `sub_828924B8` | - | - | Ultrawide: the sun tile grid |
| `Ultrawide_TextureHeader` | b | `engines/lux/ultrawide.cpp` | `sub_823B38E0` | - | - | Ultrawide: widens one more scene texture header |
| `Ultrawide_SceneWidth` | b | `engines/alchemy/ultrawide.cpp` | - | `sub_8218ED90` | - | Ultrawide: the scene's width (16:9 capped at 1000) widened |
| `Ultrawide_SceneHeight` | b | `engines/alchemy/ultrawide.cpp` | - | `sub_8218EDD8` | - | Ultrawide: the scene's height, lowered to keep the pixel count (fits the EDRAM) |
| `Ultrawide_SceneTargets` | b | `engines/alchemy/ultrawide.cpp` | - | `sub_82196150` | - | Ultrawide: the scene targets made from a size capped inline by the caller |
| `Ultrawide_ScenePasses` | b | `engines/alchemy/ultrawide.cpp` | - | `sub_8219EC20` | - | Ultrawide: the scene passes' set-up, same reason |
| `Zone_IndexByName` | b | `engines/alchemy/zone.cpp` | - | - | `sub_829EA688` | `SKY_LEVEL` [SwapForce]: the zone lookup by name; the first zone's name (`game::FirstZone`) becomes `SKY_LEVEL`'s, in place |
| `Ultrawide_CameraAspect` | c | `games/ssa/adapter.cpp`, `games/giants/adapter.cpp` | `sub_827F56A0` | `sub_8219B900` | - | Ultrawide: the game's camera aspect (`Adapter::ultrawide`); in Giants one global that the projection reads, Hor+ |
| `Ultrawide_ScenePass` | c | `games/swapforce/ultrawide.cpp` | - | - | `sub_8258EBE8` | Ultrawide and the full-resolution scene [SwapForce]: the scene chain's root pass asked for 1280x720 (or the wide size) by default; with `original_resolution=1` a pass that comes out 1040x620 is widened; its children follow |
| `Scene_Tiles` | c | `games/swapforce/ultrawide.cpp` | - | - | `sub_825138F8` | Full-resolution scene [SwapForce]: a set of targets the game split into predicated tiles (past 2048 EDRAM tiles) kept in one piece |
| `Scene_SurfaceTiles` | c | `games/swapforce/ultrawide.cpp` | - | - | `sub_82516EB8` | Full-resolution scene [SwapForce]: the EDRAM plan counts the chain's surfaces at the original size, so every base is the console's |
| `Ultrawide_WorldCamera` | c | `games/swapforce/ultrawide.cpp` | - | - | `sub_82B992A8` | Ultrawide [SwapForce]: the world camera's projection, with the window's aspect in its viewport object for that call only |
| `Audio_SetLanguage` | c | `games/ssa/language.cpp` | `sub_82864B90` | - | - | Voices in another language: the audio start-up's language code (`Adapter::SetVoiceLanguage`) |
| `Movie_VoiceTrack` | c | `games/ssa/language.cpp` | `sub_827F4400` | - | - | Voices in another language: the cutscenes' Bink voice track |
| `Language_LoadPack` | c | `games/giants/language.cpp` | - | `sub_822A8590` | - | Voices in another language: the resource loader; the voice language's `<LANGUAGE>.pak` is loaded before the text language's ([boot.md](../../games/giants/docs/boot.md#voice-language)) |
| `Movie_Open` | c | `games/giants/language.cpp` | - | `sub_823344B0` | - | Voices in another language: the story movies' Bink voice track (language + 1), set just before `BinkSetSoundTrack` |

## Adding a hook

1. Find the guest function ([sigmatch.md](../tools/toolchain/sigmatch.md), `tools/ppcinfo.py`; [problem_catalogue.md](../porting/problem_catalogue.md)).
2. Decide the class: does every game with this library need it (a), every game on this engine (b), or only this game (c)?
3. Write it in the class's folder as `#ifdef GAME_HOOK_<Name>` ... `PPC_FUNC(GAME_HOOK(<Name>))`; call the original with
   `GAME_IMP(<Name>)(ctx, base)` after `PPC_FUNC_IMPL(GAME_IMP(<Name>));` if the hook needs it.
4. Add `#define GAME_HOOK_<Name> sub_XXXXXXXX` to each game's `addresses.h` that has the function.
5. Add the row above and update the counts (`tools/check_docs.py` does not check this table; recount with the commands
   at the top).

Check after a change: the runtime objects define one `sub_82xxxxxx` symbol per `GAME_HOOK_` macro of the game
(`nm` of `build/ArkeyanRuntime/CMakeFiles/ArkeyanRuntime.dir`; SSA: 39 macros, 39 symbols).
