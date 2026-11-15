# CLAUDE.md: rules for AI tools working in the forge

Native ports of the six Xbox 360 Skylanders games by static recompilation. What the project is, the status of each
game and the map of the tree: [README.md](README.md). The docs: [docs/README.md](docs/README.md). How a change is sent:
[CONTRIBUTING.md](CONTRIBUTING.md); the AI rules: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md#contributions-and-ai-tools).

## Where to look
| Need | Go to |
|---|---|
| Build, run, test, isolation env names | [docs/getting-started/](docs/getting-started/README.md) ([testing.md](docs/getting-started/testing.md)) |
| Code, runtime, engines, one game | [docs/architecture/](docs/architecture/README.md), `games/<id>/README.md` |
| Solved problems, traps | [docs/porting/](docs/porting/README.md) (`gotchas.md`, `problem_catalogue.md`: grep, do not read whole) |
| Releases, kits | [docs/release/](docs/release/README.md) |

Pages over 300 lines (`games/*/docs/{boot,status,history}.md`, `docs/porting/problem_catalogue.md`): `grep -n` or
`sed -n`, never whole.

## Code conventions (full: [docs/CONVENTIONS.md](docs/CONVENTIONS.md); enforced by `.clang-format` and the hooks)
- Format is automatic (4 spaces, Allman, 120 columns); never by hand. `runtime/` has no game name, no per-game `#ifdef`.
- Guest memory through `g_memory.Read<T>` / `Write<T>` only; no allocation or log in a per-frame path.
- New host logic: a `*_test.cpp` in `sky_add_tests`; a bug fix: the test that failed. `tools/ci/run.sh` and
  `tools/test_all.sh --quick` before a merge.
- A new warning is a bug (`cmake/Quality.cmake`). Never touch `thirdparty/`, `ArkeyanRecompLib/`, generated code: use
  `patches/`.
- Comments: why only. Commits: one line, `area: what (#n)`, under 72 characters, signed off (`git commit -s`), no
  co-author lines.

## Hard rules
- **No game data, ever, in git or in anything shared** (ISO, `ppc/`, shader cache, extracted art, figure dumps, byte
  signatures of game code), and never pasted into an AI tool. Disc-derived material goes in `private/` (git-ignored).
- **No outside code, ever.** Xenia, ReXGlue, Core of Light, Unleashed Recompiled, Dolphin, RPCS3 and any other project
  are read-only references for ideas and diagnosis: never copy, adapt or translate their code. The Xenia-derived parts
  already in `runtime/` stay as they are, with their attribution.
- **No personal information**: no names, user names, home paths, machine names or addresses in files or commit
  messages. Commits are authored as `mflr0` or `Suolumi`. `tools/leak_scan.py` (the pre-push hook) checks the tree.
- **Never touch the player's real files** (`~/Library/Application Support/{Skylanders,SSARecomp,Skylauncher}` and the
  figures). Every game or launcher run uses scratch `SKY_SETTINGS`, `SKY_LAUNCHER_INI`, `SKY_COLLECTION`,
  `SKY_SAVE_DIR`, `SKYLANDERS_DATA`, `SKY_LOG`, `SDL_VIDEO_DRIVER=offscreen`, `SDL_AUDIO_DRIVER=disk`,
  `SKY_LAUNCHER_NO_UPDATE_CHECK=1` ([testing.md](docs/getting-started/testing.md)).
- **Processes.** Never `pkill`/`killall` the game: kill only your own PIDs. Warn before closing a running game. At most 1
  game instance per session. `-j4` and `nice`.
- **Shell.** Never edit a script while it runs. `rm -rf "${VAR:?}"`. No full-screen screenshots. Never drive the real
  mouse or keyboard: use the game's own test hooks.
- **Commits come after a passing test.** Every shell sequence that commits, pushes or tags runs with `set -euo pipefail`
  and checks each test's exit status first.
- **Pull requests only.** Every change goes through a pull request into `main`; the owner reviews and merges it. After a
  merge touching `patches/`: reset the submodules in `thirdparty/`, rebuild.
