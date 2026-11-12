# Shaders: containers, archives, XenosRecomp, specializations, the translated-shader ABI

> **Purpose:** how are the title's shaders found, translated by XenosRecomp, cached and bound? · **Status:** current (2026-11-03) · **Tags:** [Xbox] [Lux] [SSA]

Code: [`d3d/shaders.cpp`](../../../runtime/d3d/shaders.cpp), [`gpu/draw.cpp`](../../../runtime/gpu/draw.cpp), [`gpu/shader_cache.h`](../../../runtime/gpu/shader_cache.h),
`thirdparty/XenosRecomp` + [`patches/xenosrecomp.patch`](../../../patches/xenosrecomp.patch), [`tools/shaders/`](../../../tools/shaders/).
Every patch hunk is explained in [toolchain/xenosrecomp.md](../../tools/toolchain/xenosrecomp.md). Class **[Xbox]** unless marked **[Lux]** (archives, run-time shaders) / **[SSA]**.

## Pipeline in one picture

```
disc: shaders.lsa, effects.lsa (Lux) + 81 containers embedded in default.xex + run-time shaders (Bink), dumped with SKY_DUMP_SHADERS
  --tools/shaders/build_cache.sh--> XenosRecomp --> private/<id>/ppc/shader_cache.cpp  (HLSL -> Metal AIR + SPIR-V(smol-v) + signed DXIL, zstd)
run time: CreateVertex/PixelShader hook hashes the container (XXH3-64) --> d3d::GuestShader
          draw: FindXenosShader(hash) --> host pipeline (plume), pipelines.bin / baseline
```

## Shader containers (standard D3D shader container, passed to D3D unchanged)

Big-endian header: `+0` flags (**`0x102A1101` vertex, `0x102A1100` pixel**), `+4` virtual size, `+8` physical size, `+0x18` shader offset (header at `container + shaderOffset`).
Total size = virtual + physical; the microcode is in the physical part.
- **Vertex header** (`d3d/shaders.cpp` `Describe`): `dword0` = offset of the microcode inside the physical part (code = `container + virtualSize + dword0`), `+0x18` first (index) and `+0x1C` count
  of vertex inputs, then a list from `+0x24` of 32-bit entries (index + input): **address (12 bits: the vertex fetch instruction), usage (4 bits at 12), usage index (4 bits at 16)**. Each fetch
  instruction is 3 dwords at `code + address * 12`; source register = bits 5-10, source component = bits 30-31 of its first dword: if it is not **r0.x** the fetch uses a **computed index** ->
  `GuestShader::pulled` (except normals/tangents/binormals/texcoords).
- **Pixel header**: dword `+0x8` = the shader's part of `SQ_PROGRAM_CNTL` (**bit 18 = param_gen**), `+0xC` = its `SQ_CONTEXT_MISC` (**param_gen_pos in bits 8-15** = the register the pixel position goes
  into). They agree in all 516 dumped SSA pixel shaders (321 off, 195 on: bit 18 set <=> MISC low bits 2 or 6). `GuestShader::paramGen = enabled ? 0x80000000 | register : 0`.
  Survey: 371 PS containers have register 0 (no position), the rest 1-10.
- **`Create*Shader` rewrites parts of the container in place**: hash it *before* calling the original (the cache key is the on-disc container; D14). The vertex fetch formats inside the containers are 0
  (D3D patches them at bind time). Two embedded containers (`0x82095BF0` VS, `0x82095D90` PS) have **no constant table** and cannot be translated (XenosRecomp asserts).
- Constant table: `D3DXGetShaderConstantTable` (`0x822E5B88` [SSA]) in the title; XenosRecomp reads the CTAB for names/registers. **PS booleans live at registers 128-255 while the CTAB numbers them from 0.**

## Lux shader archives (`.lsa`) [Lux]

`shaders.lsa` (600 VS + 419 PS) and `effects.lsa` (17 VS + 57 PS) at the disc root. Format `\0ASL` v2 (`tools/shaders/lsa.py`), all big-endian:
header 0x40 bytes: `magic 0x0041534C`, `version 2`, `shaderCount`, `shaderTable` (0x40), `programCount`, `programTable`; **32-byte shader entries**: `keyCount`, `keyOffset` (the keys, then 0x55 padding up to the
container), `type` (`0x1909BEB3` VS, `0x88E6E692` PS: name hashes), `size`, `dataOffset` (the container), 3 reserved; **32-byte program entries**: group and name hashes + 3 lists (techniques/permutations,
constant/sampler bindings) not decoded and not needed. The loader `0x828FE7B0` (from `0x828574D8` [SSA]) hands each container verbatim to Create*Shader (lux wrappers `0x8213AEB8` VS / `0x8213A958` PS; shader constructor `0x8296E750`).
**81 more containers** are embedded in `default.xex` (`0x8208DD18..0x820A1C58`, pointer table `0x82A42D74`, used by `sub_828A5E48`) [SSA]. **Bink's movie shaders are built at run time** (not in any archive):
run once with `SKY_DUMP_SHADERS=<dir>` and feed the dump to the cache build (D15). The xex also links a run-time microcode assembler (`0x82291448-0x82296028`) and the D3DX HLSL compiler; whether the game
compiles HLSL at run time besides Bink is unknown.
SSA totals: **1119 unique containers (600+17 VS, 419+57 PS in archives, 81 embedded, plus run-time ones); 1122 of 1122 translate** (after the patch; before: 1111/1119, then 1118/1122 with 2 shaders from `shaders.lsa` the Metal
compiler rejected: SSA's grass vertex shaders, fixed 2026-10-27 by vertex texture fetch, below).
Microcode census (`tools/shaders/ucode.py`): only 4 VS output point size, 2 PS use `SetTextureLod`/`useRegLod`, **no** shader uses `CondExec(bool)`, `CondCall` or `memexport`; `CondExecPred` clauses contain only predicated instructions.
**Explicit texture LOD and cube operands** [Xbox] [V] translation, [?] picture (2026-11-01): a pixel shader's
`SetTextureLod` writes the selected source component to invocation-local `textureLod`; a texture fetch with `useRegLod`
uses `SampleLevel` (DXIL/SPIR-V) or `level()` (Metal) for 2D, 3D and cube textures (also the existing 1D-as-2D path).
The state starts at zero before all guest control flow: a skipped or predicated-off setter preserves it, and a taken
branch or loop iteration carries its latest value into later fetches; it is never substituted at translation time.
Explicit 2D fetches retain source/destination swizzles, offsets, resolve scale and unnormalized coordinates; a signed
instruction LOD bias adds sixteenths of a mip level to the register value. Cube direction decoding now applies both
operands' selection, swizzles and modifiers to their packed coordinates (`src0.z`, `src1.x`, `src1.z`), instead of
assuming the source register's XYZ. The canonical packing is `src0 = (z,z,x,y)`, `src1 = (y,x,z,z)` (see the
[AMD ISA's CUBE description](https://www.amd.com/content/dam/amd/en/documents/radeon-tech-docs/instruction-set-architectures/R600_Instruction_Set_Architecture.pdf));
the existing direction-carrying cube helper and its canonical call spelling stay intact. LOD helpers are emitted only
for shaders using the state, leaving the common header and unrelated translations byte-identical. Earlier LOD-only
experiments did not improve Swap Force's lava flicker; this corrects reflection sampling, with visual results still
requiring a game run.

Local audit, deduplicated by complete container content with `python3 -I tools/shaders/ucode.py <folders...>`;
counts below are **shaders using a feature**, not instruction counts. The three private pixel-shader samples are a
subset of Trap Team: all three set/use explicit LOD and cube, two use noncanonical cube operands. Giants' locally
available disc containers were in a scratch sample folder; all game inputs were read only. SSA has compiled caches
but no raw containers available here, so its earlier census above was not repeated.

| Feature | Giants (4394) | Swap Force (1081) | Trap Team (880) |
|---|---:|---:|---:|
| `SetTextureLod` / `tfetch.useRegLod` | 0 / 0 | 55 / 55 | 64 / 64 |
| Cube ALU / noncanonical operands | 0 / 0 | 55 / 28 | 64 / 2 |
| `tfetch.useCompLod` (implicit derivatives, already used) | 4130 | 468 | 436 |
| `GetTextureComputedLod` (still ignored) | 0 | 1 | 1 |
| `GetTextureGradients` (still ignored) | 0 | 22 | 8 |
| `SetTextureGradientsHorz` / `SetTextureGradientsVert` (still ignored) | 0 / 0 | 0 / 0 | 0 / 0 |
| `tfetch.useRegGradients` / nonzero `lodBias` | 0 / 0 | 0 / 0 | 0 / 0 |

Both LOD-selection bits are clear on 28 fetches in 3 Swap Force shaders and 24 fetches in 2 Trap Team shaders: these should
select level zero, but still take the implicit path; this separate gap and the gradient/query gaps remain outside this
change. Every audited explicit fetch has `useCompLod = 0`, no gradient flag, and zero bias.

Proof (no game run): XenosRecomp built locally; all **6355** distinct containers translate before and after;
**6236** full HLSL outputs are byte-identical, **119** change, exactly the explicit-LOD shaders (55 + 64). All 119
compile with the project's DXC path to DXIL (every specialization variant) and SPIR-V, including the three private
samples. Three invented canonical-cube shaders, including partial write masks, remain byte-identical to the baseline.
The disc-free test covers predicated setters/fetches, selected components, signed bias, dimensions, denormalized
coordinates, branch/loop state placement, and modified cube operands. Run it with:

```sh
cmake -S tools/shaders -B .scratch/texlod-tests -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C .scratch/texlod-tests -j2 XenosRecomp ShaderTranslationTest
ctest --test-dir .scratch/texlod-tests --output-on-failure
```

The test executable also accepts `<manifest> <output-folder> <shader_common.h> --translate|--compile` for local
before/after comparisons; manifests contain one container path per line, outputs are numbered HLSL files. Keep real
inputs and all derived outputs private in ignored scratch storage. Both modes also check that `shaderMain` gives every
local a value when it declares it and zero-initialises its output (a read of a local without one is undefined: Apple
and AMD give 0, NVIDIA anything); the corpus mode counts the shaders that do not and fails on one. [V] (2026-11-10): 0
of 9928 (SSA 1122 from the disc, Giants 4442, Swap Force 1081, Trap Team 878, SuperChargers 1215, Imaginators 1190;
Giants', SuperChargers' and Imaginators' from a run's `SKY_DUMP_SHADERS`): every temp `r0-r31`, `a0`, `aL`, `p0`, `ps`,
the cube state and the outputs start with a value, so the translation has no undefined read of that kind. Metal compilation was attempted on all three samples
with the project's flags but `xcrun metal` reported a missing Metal toolchain; no Metal device or game run was available.
On hardware, rebuild each game's cache, then check Trap Team and Swap Force reflection roughness and direction while
turning the camera (no wrong face, sparkle or overly sharp reflections), plus unchanged 2D passes, shadows, movies and
Giants/SSA rendering at scales 1 and 2 on Metal, D3D12 and Vulkan. A setter inside a branch/loop must affect only
fetches reached after that setter executes. Cache delivery and version changes are in
the review and merge guide (in the private office) and each game's shader page.

## The cache (`private/<id>/ppc/shader_cache.cpp`, generated, never committed)

`ShaderCacheEntry { u64 hash; u32 dxilOffset, dxilSize, spirvOffset, spirvSize, airOffset, airSize, specConstantsMask; const char* filename }`, sorted by hash, plus zstd blobs
`g_compressedDxilCache` (**Windows only**, `#if defined(_WIN32)`), `g_compressedAirCache` (Apple) or `g_compressedSpirvCache` (others), and `g_shaderCacheEntryCount`.
`tools/shaders/build_cache.sh <iso> [<dump dir>]` extracts the archives with `xdvdfs.py`, copies `private/image.bin` as `default.xex.image` (so XenosRecomp finds the 81 embedded), adds `runtime/` dumps,
runs XenosRecomp (asserts on: they reject bad shaders): ~1.5 min on an M1 Pro. **Build it on the Mac**: it then holds AIR + SPIR-V + DXIL and serves every platform (a cache built on Linux has no AIR and breaks macOS).
**Rebuild the cache after any XenosRecomp change** (after the `g_ResolutionScale` change an old cache broke the light passes at x2; after the invariance flag, depth-EQUAL passes).
DXIL entry layout: `flags (bit 0: the vertex shader outputs a point size)`, `variant count`, each variant's size, the signed containers in increasing order of `g_SpecConstants()`.
SPIR-V is stored as **smol-v** (`smolv::Encode`): decode before `createShader` (lavapipe crashed on the raw blob until `gpu/draw.cpp` decoded it).

## XenosRecomp semantics we depend on (what the patch adds, in behaviour terms)

- **Vertex index in r0.x** (as the hardware does): `float4(float(iVertexId), 0, 0, 0)`; the sun/shadow light pass places **770 point sprites, one per screen tile**, from it. Without it all landed on one spot (black light buffers).
- **Computed-index vertex fetch** (`PULLED_VERTEX(index)`): a fetch whose source is not r0.x reads `g_PulledVertices[min(index, count-1)]` (host-gathered float4s; Metal, DXIL and, since 2026-11-03, SPIR-V through `vk::RawBufferLoad` at `g_PulledVertices`; before, Vulkan read the vertex's own).
  Formats gathered: UBYTE4, UBYTE4N, D3DCOLOR, FLOAT1-4. Each tile's VS fetches a per-tile byte this way and **kills itself** (kill-vertex export: `oPos = (-2, -2, 0, 1)`) unless the pass is its own.
- **Point sprites**: VS export 63 (`VSPointSizeEdgeFlagKillVertex`): `.x` size (`oPointSize = pts.x * g_ResolutionScale`), `.z` kill flag. Metal rejects a point-size output in triangle pipelines, so only vertex shaders
  that export a size keep it (`exportsPointSize`). The pixel shader gets the point coordinate when `param_gen` is on. **D3D12 has no point sprites**: a geometry shader
  (`gpu/shaders/point_sprite.hlsl`) expands points to squares; 4 vertex shaders export a size.
- **param_gen register layout** (`g_ParamGen`: bit 31 enabled, bits 0-3 register): `gen = float4((vPos.xy / g_ResolutionScale - 0.5) * float2(iFace ? 1 : -1, 1), pointCoord.xy)`: **pixel position in XY (X negative on
  back faces: games take its absolute value) and the point coordinate in ZW** (Xenia's layout). The first version wrote `float4(point_coord, 0, 0)` and broke every pass drawn with param_gen (D3D sets it whenever a PS reads vPos).
- **`g_PositionScale`** (`oPos.xy = oPos.xy * g_PositionScale + g_HalfPixelOffset * oPos.w`): converts screen-space positions when the viewport transform is disabled (2/W, -2/H) and draws over the whole target.
- **DEC3N normals** (`tfetchR11G11B10` with `SPEC_CONSTANT_R11G11B10_NORMAL`): three signed 10-bit components, X in the low bits: `max((mag - sign)/511, -1)`.
- **Vertex texture fetch** [Lux] [SSA] [V]: a vertex shader's `tfetch` (SSA's grass samples its wind simulation, `SimSampler`; 3 vertex shaders, `shaders.lsa` 975, 977, 979) uses the fetch constant **16 + n**
  (`instr.constIndex`) while its constant table lists the sampler as register **n**. The translator maps `16 + n` to the table's sampler, reads the descriptor indices from `SharedConstants` +320 (2D texture) and
  +336 (sampler), 4 each (the pixel shader's slots 0-15 are the first 256 bytes), samples at level 0 (`tfetch2DVertex`: a vertex shader has no derivatives), and the Metal entry point takes the 2D texture and sampler heaps
  (`[[buffer(0)]]`, `[[buffer(3)]]`, bound to the vertex stage too). Before, the references to `s16_*` were undeclared: Metal rejected both shaders (`Skipped ... Metal compiler exited`) and every grass draw was skipped
  (`[render] no translated vertex shader CF51652CDDD369A7`). Only with a real constant table (Alchemy's synthetic one lists 16 samplers in every shader; no Giants vertex shader fetches a texture: checked on all 264).
  The runtime: `GuestShader::samplers` (the table's samplers 0-3, read in `d3d/shaders.cpp`), fetch constants 16-19 (registers `0x4800 + 16 * 6`) in `d3d/render.cpp` only for those, `DrawDesc::vertexTextures`.
- **Texcoords** `swapFloats(g_SwappedTexcoords, ...)` swaps the halves of 16-bit pairs back.
- `[[position]] [[invariant]]` on Metal needs the `-fpreserve-invariance` compile flag, **else depth-EQUAL passes lose the terrain** (D16).
  HLSL's `precise float4 oPos : SV_Position`: dxc's SPIR-V decorates **every operation that feeds the position `NoContraction`** (no FMA fusion, so the same
  arithmetic in every pipeline) but does **not** decorate `Position` `Invariant` [V] (`spirv-dis`, 2026-11-03). Not the cause of SSA's Vulkan squares (that was the two inputs
  below); if a depth-EQUAL pass ever flickers on one Vulkan driver only, `Invariant` (`vk::ext_decorate`) is the next thing to add.
- **SPIR-V has the same inputs as Metal** (2026-11-03) [V]: computed-index fetches (`pulledVertex()` in `shader_common.h`, nothing read when the count is 0) and the param_gen register
  (the Metal branch, with `[[vk::ext_builtin_input(16)]] static const float2 iPointCoord;` declared for pixel shaders: dxc's `vk::builtin` has no PointCoord). Every pixel shader's
  SPIR-V changes with it (a load of `g_ParamGen`, a branch); DXIL and Metal do not: DXIL bytes identical for all 1122 SSA shaders, and the Metal-preprocessed source identical for
  all 1119 disc shaders (the AIR of 179 changes size only because the source text around it moved). Checked by `ShaderTranslationTest` (a computed-index fetch has no `__spirv__`
  branch and compiles to SPIR-V; a pixel shader's SPIR-V declares PointCoord and FrontFacing).
- Pixel shader booleans: `boolAddress >= 128 ? boolAddress - 128` (CTAB numbering).
- **Unnormalized texture coordinates** (the fetch instruction's `texCoordDenorm` bit, 2026-10-27) [Xbox] [V]: the
  coordinates are guest texels, not 0..1. 2D fetches with the bit become `tfetch2DDenorm` / `getWeights2DDenorm`, which
  divide by the texture's size after scaling by `g_ResolutionScale` when the register holds a resolve
  (`g_ScaledTextures`, set by `d3d/render.cpp` from `HostTexture::resolved`): a resolve's host texture is scale x its
  guest size, a disc texture is not. Screen-space passes use it with the pixel position (`abs(vPos)`): Swap Force's
  deferred composite, shadow-mask blur and FXAA (19 of its 556 shaders), SSA's soft-particle depth fetches (6 of 1113
  dumped shaders), Giants none (0 of 4444). Before, the coordinates were taken as normalized: every pixel sampled the
  corner texel (Swap Force: a flat, dark scene). 1D, 3D and cube fetches with the bit are not handled (none seen).
- **An unnormalized fetch reads inside its texel** (`DENORM_TEXEL`, 2026-11-10) [Xbox] [V] (Mac; the PC: [?]): the pixel position
  (`iPos / scale - 0.5`) makes the host coordinate an integer, texel h's left and top edge, where the console, Apple and
  AMD read texel h and NVIDIA (the float coordinate a hair under the edge, truncated to its 8-bit subtexel grid) reads
  h - 1: on the owner's RTX 3060 each such pass of Trap Team's title moved its picture one host pixel right and down.
  `DENORM_TEXEL(texCoord, offset, scale)` in `shader_common.h` adds 1/256 of a texel (D3D's minimum subtexel step) for
  `tfetch2DDenorm` and `tfetch2DLodDenorm`, Metal and HLSL: a point fetch reads texel h on every GPU, a bilinear one moves
  by 1/256 of a texel (at most one 8-bit step). `ShaderTranslationTest` models that GPU (normalize, scale back, truncate)
  over 64 to 1280 texels at scales 1 to 3: on the edge it misses 279 of 39 888 pixels (NVIDIA's own arithmetic missed
  all of the title's), with the nudge none. A Mac cache with the
  coordinate 1/1024 before the edge reproduces the PC's shifts; with the fix they are 0
  ([status.md](../../../games/trapteam/docs/status.md#the-title-logos-black-outline-on-a-windows-pc-2026-11-10)). Every
  game's shader cache is translated again on its next build (the cache key covers `shader_common.h`).
- **Fetch offsets are guest texels** (the "kernel scale", 2026-10-27) [Xbox] [V]: a fetch instruction's `OffsetX/Y`
  (the half-texel steps of blur and shadow-filter kernels, FXAA's corner taps) is in texels of the guest texture. A
  pixel shader's 2D (and 1D) `tfetch2D` / `getWeights2D` now take the same scale argument as the unnormalized ones:
  `tfetch2D` adds `offset x scale / host size`, and `getWeights2D` takes the bilinear weights on the guest grid
  (`fract(uv x host size / scale + offset - 0.5)`). Before, at resolution scale N such a tap moved 1/N of a guest
  texel. Vertex-shader fetches keep scale 1 (their samplers are not in `g_ScaledTextures`); disc textures are
  unchanged (scale 1). **What it touches** (a scan of every translated pixel shader, 2026-10-27): only fetches with a
  non-zero instruction offset: **SSA 2** of 516 pixel shaders, **Swap Force 19** of 498 (FXAA 3.11's +-0.5 corner taps
  and 4-tap downsamples), **Giants 0**. The games' blur and shadow kernels step by constants in normalized coordinates
  that the game computes from the guest sizes, which were already right at every scale: the "shadows smaller at scale
  2" another public port reports (`office/lessons_2026-10-27.md`, F1) comes from its translation, not ours [V].
  **One exception** (2026-10-30): FXAA is an edge filter, so at scale N it wants host pixels, not the frame's proportions: `game::HostKernelPixelShader` ([swapforce shaders.md](../../../games/swapforce/docs/shaders.md#the-lavas-posterized-gradients-2026-10-30-swapforce-ssa-xbox-v)).
- A shader the translator or the Metal compiler cannot handle is **reported and skipped** (`Skipped <hash> (<file>): reason`), not fatal; `N of M shaders translated`, `N of them without DXIL`.

### Binding model (`SharedConstants`, 352 bytes; `shader_common.h` offsets)

| Offset | Field |
|---|---|
| 0, 64, 128, 192 | texture heap indices: 2D x16, 3D x16, cube x16, samplers x16 (u32 each) |
| 256 | `g_Booleans` (VS bits 0-15, PS bits 16-31) |
| 260 | `g_SwappedTexcoords` |
| 264 | `g_HalfPixelOffset` (float2) |
| 272 | `g_AlphaThreshold` |
| 276 | `g_ParamGen` |
| 280 | `g_PositionScale` (float2) |
| 288 / 296 | `g_PulledVertices` (device address) / `g_PulledCount` |
| 300 | `g_ResolutionScale` |
| 304 | point scale (1 / viewport size, D3D12 point-sprite GS) |
| 312 | `g_ScaledTextures`: bit n = sampler register n holds a resolve (host size = guest size x `g_ResolutionScale`); unnormalized fetches scale by it (2026-10-27) |
| 320, 336 | a vertex shader's texture heap indices (2D x4) and sampler indices (x4): vertex texture fetch |

**Metal/Vulkan**: push constants = three device addresses `{vertexShaderConstants, pixelShaderConstants, sharedConstants}` (`[[buffer(8)]]`); bindless heaps: descriptor sets 0-2 = 2D/3D/cube textures,
set 3 = samplers (`[[buffer(0..3)]]`; `[[id(binding)]]`; Metal needs tier-2 argument buffers). **D3D12** (no device addresses): sets 0-3 = register spaces 0-3 and **four root descriptors**: CBV b0/b1/b2 in
space 4 (VS, PS, shared constants, 256-byte aligned) and SRV `t0, space5` (pulled vertices; the shared constants' address when none). Up to **16 samplers** per draw (`MaxSamplers`), plus 4 vertex samplers (`MaxVertexSamplers`).
Textures per slot: placeholders black for unbound slots; sampler 0 kept. Constants: 256 float4 per stage uploaded once per frame when identical banks repeat.

### Specialization constants

Only two flags exist in SSA, each shader uses at most one: **R11G11B10 normals** (vertex shaders) and **alpha test** (pixel shaders): 297 shaders have 1 variant, 821 have 2. Metal/Vulkan use real
specialization constants (`gpu::SpecConstant::{R11G11B10Normal = 1, AlphaTest = 2}`); **D3D12 has none**, so XenosRecomp compiles **one DXIL per combination** with `-DSPEC_CONSTANTS=<value>` (20.6 MB of DXIL, 3.2 MB
compressed) and `VariantOf` picks the pipeline's one. Alternative rejected: DXIL libraries linked at run time need `dxcompiler.dll` (~20 MB, outside the UWP API surface) plus signing at run time.
**DXIL signing** (`signDxil`): dxc only writes the container hash when `dxil.dll` is present (macOS has none) and D3D12 rejects unsigned containers; we compute MD5 over the bytes after the hash with the bit count first and
`(bytes * 2) | 1` last (reproduces 15 `dxil.dll`-signed SDL shaders). `tools/shaders/check_dxil.sh` validates all 1939 containers (0 invalid, 0 wrong hash) under Wine.
Shaders with jumps run their blocks in `while (true)` from block `pc`; Metal/SPIR-V use a `switch` with fall-through, **DXIL uses `if (pc <= N)` blocks** (`PC_DISPATCH` / `PC_CASE(N)`): vkd3d-proton's dxil-spirv made invalid SPIR-V from the switch for 3 shaders (black terrain).
Input locations come from XenosRecomp's `USAGE_LOCATIONS` and must match the host's `InputLocation` (`d3d/shaders.cpp`); TEXCOORD 8 (Trap Team) is 17 in both [TrapTeam] [V] (2026-10-30: the first level's frame is whole with it). DXIL vertex inputs are `ATTRIBUTE<location>`; every DXIL VS outputs `POINTCOORD` then `PSIZE` last so the pixel shader's inputs are a prefix of the vertex outputs (D3D12 links by register).

## Binding a draw (`d3d/render.cpp` `PrepareDraw`)

`vs = FindGuestShader(device+0x3290)`, `ps = FindGuestShader(device+0x328C)` (**null PS = depth-only pass**: must not be skipped, D20); missing translated shader -> `[render] no translated vertex|pixel shader <hash> (primitive N): its draws are skipped`.
Booleans: `(GpuRegister(0x4900) & 0xFFFF) | (GpuRegister(0x4904) & 0xFFFF) << 16`. Alpha test: `RB_COLORCONTROL` bit 3 -> `SpecConstant::AlphaTest` and `alphaThreshold = RB_ALPHA_REF` float.
Shader constant names seen in translated HLSL: `DepthMap`, `DynamicShadowMap`, `GBuffer`, `GBuffer2`, `DiffuseLightingBuffer`, `SpecularLightingBuffer`, `VposParams`, `IrisParams`, `AmbientMap`, `ShadowBlendFilterParams`
[Lux]. Shader hash -> HLSL: match the dumped container with the `private/shaders/exp/in/*` inputs.
