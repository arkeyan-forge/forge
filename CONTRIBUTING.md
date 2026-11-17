# Contributing to the Arkeyan forge

This page says what is welcome, how to send it, and what a pull request needs before it can be merged. Questions: the
[Discord](https://discord.gg/UMQY4SAS3F) or an [issue](https://github.com/arkeyan-forge/forge/issues).

## What you can contribute
- **The ports**: the runtime, the engines' and games' hooks, the builder, the tools, the docs, our patches to the
  translators (`patches/`).
- **The launcher** lives in [Arkeyan Launcher](https://github.com/mflr0/Arkeyan-Launcher), under the same rules.
  Players report problems there; issues about the ports' code live here.
- **Never**: anything from a game (disc images, game files, translated code, shaders, extracted pictures, sounds, text,
  figure dumps, byte signatures of game code) and code copied or adapted from another project. Libraries are added only
  after an issue and the owner's agreement.

## Before you start
- **A feature or a big change: open an issue first** (or comment on the existing one) and wait for an agreement.
- **A small fix** (a typo, an obvious bug): send the pull request directly.
- Work in your fork, on a branch of your own (for example `21-cheaper-draws`), and target `main`.

## Building and testing
See [building and playing](docs/getting-started/building-and-playing.md) and [testing](docs/getting-started/testing.md).
Once per clone: `git config core.hooksPath tools/git-hooks`. The hooks check the format, the identity, that no game
data and no personal information is staged or pushed (`tools/leak_scan.py`), and run the quick checks before a push.
Before you send: `tools/ci/run.sh` and `tools/test_all.sh --quick`, both PASS.

New behaviour needs a `*_test.cpp` in `sky_add_tests`; a bug fix comes with the test that failed. Run games only with
scratch settings, saves and figures, never on your real data ([testing](docs/getting-started/testing.md)).

## Code
- C++20, the style in `.clang-format` (4 spaces, braces on their own line, 120 columns); the hooks check it. More:
  [docs/CONVENTIONS.md](docs/CONVENTIONS.md).
- `runtime/` names no game and has no per-game `#ifdef`: game behaviour is a hook in `games/<id>/` or `engines/<engine>/`.
- Never edit `thirdparty/`, `ArkeyanRecompLib/` or generated code: change the translators through `patches/`.
- Match the code around yours. Update the docs page that covers what you changed (`python3 -I tools/check_docs.py`
  reports 0 broken).

## Commits and pull requests
- **One subject per pull request.** Its **title** becomes the merged commit's message: `area: what (#n)`, lower-case
  area, English, 72 characters at most, the issue number when there is one.
- Pull requests are **squashed** when merged. The owner reviews and merges every pull request.

## AI tools
Welcome, under the rules of the [Code of Conduct](CODE_OF_CONDUCT.md#contributions-and-ai-tools): you are the author of
what you send.

## License
The project is under the [MIT license](LICENSE). By sending a pull request, you agree that your
contribution is published under it.
