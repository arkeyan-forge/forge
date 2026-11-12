# Code conventions

> **Purpose:** the rules for every line of our own code (runtime, games, engines, tools); format is enforced by `.clang-format` and the git hooks, this page is what a tool cannot check · **Status:** current (2026-10-29) · **Tags:** none

Not covered here: `thirdparty/`, `ArkeyanRecompLib/`, the generated code in `private/`. Never reformat or "fix" them.
Where their warnings reach our targets, their include directory is `SYSTEM` (`target_include_directories`); a system
directory is searched after every `-I` one, so when two directories hold the same header (`ppc_context.h`: the game's
ppc folder and XenonUtils), both are `SYSTEM`, the one to use first (`runtime/CMakeLists.txt`).

Format: **clang-format 23.1.3**, the one version for every machine and CI (`pip3 install --user clang-format==23.1.3`;
the pre-commit hook finds it on `PATH` or in pip's user folder and refuses any other version). Another version can
format the same file differently.

Bug finders: `.clang-tidy` (bug checks only, no style). CI's `tidy` job runs it on the `.cpp` files a pull request
changes, and a finding fails it. Started by hand (`workflow_dispatch`), it checks every file and lists the findings by
check without failing: older code still has some (2026-11-12: `getenv` in `runtime/host/env.h`, unchecked `optional`
access in `runtime/host/portal_slots.h`).

## Naming

| What | Style | Example |
|---|---|---|
| Types, enums, enum values | PascalCase | `GuestHeap`, `Glyph::LeftStickIcon` |
| Functions and methods | PascalCase | `SystemAlloc`, `SamplerFor` |
| Locals and parameters | camelCase | `threadId`, `firstWord` |
| Members | `m_` + camelCase | `m_free`, `m_pageSize` |
| Globals | `g_` + camelCase, only in a `.cpp` unless shared | `g_memory`, `g_physicalHeap` |
| Constants (`constexpr`) | PascalCase, no prefix | `GuestMemorySize`, `PhysicalWindowsBase` |
| Guest functions | `sub_XXXXXXXX` only through `GAME_HOOK(Name)` / `GAME_IMP(Name)` | `GAME_HOOK(PresentSwap)` |
| Guest data addresses | `constexpr uint32_t` in `namespace game`, in `games/<id>/addresses.h` | `game::DeviceGlobal` |
| Env variables | `SKY_` + UPPER_SNAKE | `SKY_DUMP_FRAME` |
| Files | snake_case | `portal_backends.cpp` |

## Outside code

No outside code, ever. Xenia, ReXGlue, Core of Light (tsapZ), Unleashed Recompiled, Dolphin, RPCS3 and any other
project are read-only references for ideas and diagnosis: never copy, adapt or translate their code; we write our own.
The Xenia-derived parts already in `runtime/` stay as they are, with their attribution.

## Where code goes

- **`runtime/`**: what every game needs. No game name, no `#ifdef` per game, no guest address.
- **`engines/<lux|alchemy>/`**: what every game of one engine needs.
- **`games/<id>/`**: hooks and addresses of one game. A hook that two games need moves to the engine.
- **`runtime/host/`**: the PC side (UI, collection, art, settings). It never reads guest memory directly; it goes
  through the runtime's accessors.
- A new kernel import goes in `runtime/kernel/import_list.inc`; a game-only import in `imports_<id>.cpp`.

## Guest memory and the recompiled code

- Guest memory is big-endian: always `g_memory.Read<T>()` / `Write<T>()` (they swap), never a raw pointer cast.
- A guest pointer is a `uint32_t`; a host pointer is never stored in guest memory.
- A hook keeps the guest ABI: arguments in `ctx.r3..r10`, return in `ctx.r3`; it calls `GAME_IMP(Name)(ctx, base)`
  for the original, never the `sub_` symbol directly.
- No host allocation in a per-frame path (draws, swaps, vblank): reuse buffers.
- Threads: a guest thread blocks only through the kernel's sync objects (`runtime/kernel/sync.cpp`), never a host
  `sleep_for` in a hook (it breaks the guest's timing).

## Errors, logs, asserts

- A state the console cannot reach: `abort()` with a `[tag]` message, never a silent `return 0`.
- A recoverable state: log once per cause (a `static` seen-set, like `indirect_call_guard.cpp`), then continue.
- Logs: `[tag] message` on stderr, one line, no trailing period; tags are short and stable (`[gpu]`, `[portal]`,
  `[video]`, `[guard]`). No log in a per-frame path unless it is behind a `SKY_*` variable.
- `assert()` only for programmer errors; it is gone in release builds.

## C++

- C++20. `std::` containers, `std::span`, `std::bit_cast`, `<bit>`; no raw `new`/`delete` (use `std::unique_ptr`).
- `const` by default; references for non-null, pointers for nullable.
- Fixed-width integers for anything that touches the guest (`uint32_t`, never `unsigned long`).
- `enum class` for new enums. No `using namespace` in headers.
- Every header: `#pragma once`, then its own includes (it must compile alone).
- Platform code only in `runtime/platform/` behind one function; no `#ifdef _WIN32` scattered in the logic.

## Comments

- Say **why**, or a fact about the console the code cannot show: an address, a field layout, a measured number with
  its tag [V] / [?]. Never what the next line does.
- A function gets a comment only if its name and signature do not say enough.
- No dates, no session names, no "fixed in 0.6.1" in code: that is the `git log`'s job.
- No TODO without the issue it waits on: `// TODO(#12): ...`.

## Tests

- New host-side logic (parsing, collection, settings, portal, art) comes with a test in its `*_test.cpp`,
  registered with `sky_add_tests` (`cmake/Quality.cmake`); `ctest` must pass.
- Game-side changes come with a regression script in `tools/regress/` or a line in `tools/test_all.sh`.
- A bug fix comes with the test that failed before it.

## Commits

- Subject: `area: what changed`, imperative, under 72 characters (`gpu: anisotropy only for mipmapped textures`).
- Body: why, in at most 3 lines, only when the subject is not enough.
- One topic per commit. Identity `mflr0`. No sign-off lines (the `commit-msg` hook refuses them).
