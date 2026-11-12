# Runtime

> **Purpose:** what replaces the Xbox 360, and which of the three areas (console, GPU, host) answers my question? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [XDK]

The hand-written runtime (`runtime/`, [its README](../../runtime/README.md)) that runs every game's recompiled code.
Index of the whole tree: [../README.md](../README.md). How the code is split between runtime, engines and games:
[architecture/layout.md](../architecture/layout.md).

| Area | Answers |
|---|---|
| [xbox360/](xbox360/README.md) | The console: kernel and XAM imports, threads, memory, files, saves, audio and XMA, vblank, input, the XEX loader, stubs |
| [gpu/](gpu/README.md) | The GPU: replacing D3D9LTCG (Option B), Xenos registers, command buffers, the ring mirror, EDRAM, textures, shaders, pipelines, plume, rendering bugs, ultrawide, texture packs |
| [host/](host/README.md) | What the host adds: portal, figures, the shared collection, the Toy Box, the disc's pictures, the runtime's `--launcher` window |
