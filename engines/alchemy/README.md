# engines/alchemy/: the Alchemy engine's hooks (Giants, Swap Force)

> **Purpose:** what code is in this folder and where its documentation is · **Status:** current (2026-10-28) · **Tags:** [Alchemy] [Giants] [SwapForce]

Hooks that belong to the Alchemy engine (class **b**, [docs/architecture/hooks.md](../../docs/architecture/hooks.md)).
Compiled into the runtime when `ARKEYAN_GAME=giants` or `swapforce`; the addresses come from `games/<id>/addresses.h`. Any `*.cpp` in
this folder is compiled automatically. Put a hook here when it is about Alchemy's own behaviour (its heap, its scene or
movie code) and would be the same in the next Alchemy game; Xbox and XDK hooks (D3D, XMA, Bink) stay in `runtime/`.

| File | What |
|---|---|
| `lzma.cpp` | `LzmaDec_DecodeReal`: the LZMA SDK's decoder loop (every archive chunk) as host code, on the guest's own decoder state; `SKY_HOST_LZMA=0`, `SKY_LZMA_CHECK=1` ([lzma.md](../../docs/engines/alchemy/lzma.md)) |
| `zone.cpp` | `SKY_LEVEL` for Swap Force (`Zone_IndexByName`) |
| `ultrawide.cpp` | `Ultrawide_SceneWidth`, `_SceneHeight`, `_SceneTargets`, `_ScenePasses`: the 3D scene's size for ultrawide, kept under the EDRAM budget (the camera's aspect is in `games/giants/adapter.cpp`) |
| `prompt_text.cpp` | `String_Intern`, `Font_CharMetrics`: a press-Start prompt ("PRESS START", "Appuie sur Start", "Druk op Start"...: `prompt_text.h`, tested by `tests/prompt_text_test.cpp`, `ArkeyanPromptTextTest`) shows the Start button's picture of the device in use, for Swap Force, SuperChargers and Imaginators (each game's art.md, "Button pictures") |
| `heap_guard.cpp` | `TlsfAddPool`, `TlsfMalloc`, `TlsfMemalign`, `TlsfFree` [SuperChargers]: a free of a block from before its pool was made again (the level heap is remade at each zone start; the menus' last object released after it) is dropped, not merged into the new pool (`live_blocks.h`, tested by `tests/live_blocks_test.cpp`, `ArkeyanLiveBlocksTest`; [status.md](../../games/superchargers/docs/status.md), the race start) |
| `owner_id.cpp` | `Owner_Fetch`, `Options_MostRecentSlot` [SuperChargers]: the player's offline owner id fetched again once the options save is loaded, so it is the save's and not a new one each session (Arkeyan Launcher issue #28; [figures.md](../../docs/runtime/host/figures.md) "Owner") |
| `nightmare.cpp` | `NightmareUnlocked`: "Nightmare from the start" (`[<game>] nightmare_from_start`, the adapter's `AlchemySetNightmareFromStart`) for Swap Force and Imaginators ([Swap Force history](../../games/swapforce/docs/history.md), "Nightmare difficulty") |

Documentation: [docs/engines/alchemy/README.md](../../docs/engines/alchemy/README.md) (the engine, the formats, the
gotchas).
