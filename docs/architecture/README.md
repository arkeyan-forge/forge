# Architecture

> **Purpose:** how is the code organised (runtime, engines, games), and which page answers which question about it? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [Lux] [Alchemy] [SSA] [Giants]

Index of the whole tree: [../README.md](../README.md).

| Page | Answers |
|---|---|
| [layout.md](layout.md) | Where is everything (one line per folder and file), which code is runtime, engine or game, how does a build pick its game? |
| [hooks.md](hooks.md) | What is hooked, where does the hook live, which game has it; the adapter's 7 members |
| [subsystems.md](subsystems.md) | Per runtime subsystem: role, main files, key functions, invariants, gotchas |

The plan that split the tree into `runtime/`, `engines/` and `games/` is archived in
[layout.md](layout.md).
