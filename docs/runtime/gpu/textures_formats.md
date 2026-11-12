# Textures, fetch constants, formats, tiling, vertex formats

> **Purpose:** how are fetch constants, texture formats, tiling, endianness, mips and vertex formats decoded? · **Status:** current (2026-10-27) · **Tags:** [Xbox]

Code: [`d3d/xenos.{h,cpp}`](../../../runtime/d3d/xenos.cpp), [`d3d/render.cpp`](../../../runtime/d3d/render.cpp) (`TextureFor`, `DecodeTexture`),
[`gpu/draw.cpp`](../../../runtime/gpu/draw.cpp). Layout from Xenia (`texture_util`, `texture_address`, BSD, adapted with attribution). Class **[Xbox]**.

## Texture fetch constant (6 dwords, `device+0x480 + 0x18*slot`, or resource `+0x1C`)

| Dword | Fields |
|---|---|
| 0 | bits 0-1 type (**2 = texture**), 10-12 / 13-15 / 16-18 clamp X/Y/Z, bits 22-30 pitch in units of 32 texels, **bit 31 tiled** |
| 1 | bits 0-5 format, bits 6-7 endian, **bit 10 stacked (2D array)**, bits 12-31 base address (physical, 4 KB aligned) |
| 2 | width-1 (13 bits), height-1 (bits 13-25), depth-1 (bits 26-31; stacked/3D) |
| 3 | bits 1-12 swizzle (3 bits per component: 0-3 = XYZW, 4 = 0, 5 = 1; `0x688` = XYZW), 19-20 mag filter, 21-22 min, 23-24 mip filter |
| 4 | bits 2-5 mip min level, 6-9 mip max level |
| 5 | bits 9-10 dimension (0 1D, **1 2D, 2 3D, 3 cube**), bit 11 packed mips, bits 12-31 mip address (0 = no mips) |

Filters: 0 = nearest, else linear. Sampler: `maxAnisotropy` (the `anisotropy` setting) applies only when min and mag are
both linear **and the texture has mipmaps** (more than one level); LOD clamp from mip min/max levels. Why the mipmap rule (2026-10-27) [Xbox]: no game asks for
anisotropy (the fetch constants' aniso field is 0 in every Swap Force draw), and a texture without mips is a render
target read at computed coordinates or a colour table: anisotropic taps along those coordinates' gradient blurred Swap
Force's whole frame and turned its lava and Eruptor green ([render_bugs.md](render_bugs.md)). **1D and 3D textures are not supported yet**
(`TextureFor` returns null).

## Format table (`xenos::TextureFormat(format & 63)`)

| # | Xenos format | Host | Notes |
|---|---|---|---|
| 2 | k_8 | R8_UNORM | |
| 3 / 4 / 15 | k_1_5_5_5 / k_5_6_5 / k_4_4_4_4 | R8G8B8A8 | **expanded on the CPU** (`ExpandTexel`, component order X in the low bits) |
| 6 | k_8_8_8_8 | R8G8B8A8_UNORM | |
| 8 | k_8_A | R8_UNORM | |
| 10 | k_8_8 | R8G8_UNORM | |
| 14 | k_8_8_8_8_A | R8G8B8A8_UNORM | |
| 18 / 19 / 20 | DXT1 / DXT2_3 / DXT4_5 | BC1 / BC2 / BC3 | 4x4 blocks, 8/16/16 bytes |
| 24 / 25 / 26 | k_16 / 16_16 / 16_16_16_16 | R16 / R16G16 / R16G16B16A16 UNORM | |
| 30 / 31 / 32 | k_16_FLOAT ... | R16*_FLOAT | |
| 36 / 37 / 38 | k_32_FLOAT, 32_32_FLOAT, 32_32_32_32_FLOAT | R32 / R32G32 / R32G32B32A32_FLOAT | |
| 49 | k_DXN | BC5 | |
| 50 | k_8_8_8_8_AS_16_16_16_16 | R8G8B8A8 | |
| 51-53 | DXT1/2_3/4_5_AS_16_16_16_16 | BC1/2/3 | |
| 54 | k_2_10_10_10_AS_16_16_16_16 | R16G16B16A16_UNORM | resolve targets only |
| 59 | k_DXT5A | BC4 | |
| 7 `k_2_10_10_10` | | **unsupported** | needs a CPU conversion (plume has no 10:10:10:2) |
| 22 / 23 | k_24_8 / k_24_8_FLOAT | **unsupported as guest memory** | depth only arrives as a *resolved host texture* (R32_FLOAT); "k_24_8 from guest memory" is not supported |
| others | | UNKNOWN | the draw uses a null texture |

An unsupported, unresolved texture now logs `[render] unsupported texture format <n> (WxH): sampling black` once per
format. Unbound GPU slots sample the black placeholder. That is black **texture data**, not necessarily a black final
picture: a guest YUV conversion pass could turn three zero inputs green. There is no host NV12/YUY2 conversion in
this path; do not replace every missing texture with neutral chroma, since ordinary RGB draws share the same slots.
The [issue #26](https://github.com/mflr0/Arkeyan-Launcher/issues/26) distinguishes this possibility from the host movie fallback.

## Guest -> host texture pipeline (`TextureFor`)

1. Key `TextureKey` = base >> 12 | format << 17 | width << 23 | height << 37 | dimension << 51. A **resolved** texture is never reloaded.
2. Once per frame per texture, **re-validate**: XXH3-64 of the base level (all slices; mips are assumed to change with it) read as the guest holds it
   (`GuestContentHash`). A matching hash keeps the host texture; a mismatch re-uploads every level and slice. The hash is also the
   key of the glyph replacement ([../xbox360/input.md](../xbox360/input.md)) and of texture packs. **From 64 KB** (`CheckGuestContent`, 2026-10-27) a frame hashes only a
   **sample** (64 lines of 64 bytes, one per 1/64th of the storage) and the whole storage when the sample changed, every 8 frames (staggered per texture), and the
   frame after a change (a texture the game rewrites keeps a whole hash every frame). Bound: a write the sample misses shows at most 8 frames late. Giants
   `level_203`: 21.9 -> 4.3-4.6 MB hashed per frame; checked with `SKY_TEXTURE_STATS=1` (0 changes missed in Giants, 0 in SSA after the rewrite rule:
   [renderer_threads.md](renderer_threads.md), "Giants (2026-10-27)"). The runtime has no guest write tracking (page protection as Xenia does would also
   need every host write into guest memory, file reads included, to unprotect first).
3. Each subresource's guest storage (`sliceBytes`) is **copied at once** (the guest may change it later), then **untiled, endian-swapped and converted on texture worker threads**
   (`gpu::UploadTextureAsync`, `DecodeTexture`) straight into upload memory. At a level start this is ~2857 textures (25.6 MB) in the first frame.

## Tiling and endian

- **Tiled 2D addressing** = Xenia's `Tiled2D(x, y, pitchBlocks, bytesPerBlockLog2)` (pitch a multiple of 32 blocks). Blocks are texels (1x1) or 4x4 for BC formats.
  The linear case: rows aligned to 256 bytes (`max(256 / blockBytes, 32)` blocks).
- **Endian field**: 0 none, 1 8-in-16, 2 8-in-32, 3 16-in-32 (`SwapEndian`). Vertex data is swapped 8-in-32.
  **Vertex data byte-swapped as 32-bit words swaps the halves of 16-bit pairs**: texcoords from 16-bit formats must be swapped back (`g_SwappedTexcoords` bit per TEXCOORDn, D19).

## Mip layout

Level 0 from the base address with the fetch constant's pitch. Levels >= 1 from the **mip address**, each padded to `max(next_pow2(base) >> level, 1)` or a 32x32-block tile; all slices of one level
together (2D arrays padded to 4 slices); array slices / cube faces 4 KB aligned. **Packed mips**: levels from the one whose shorter side is <= 16 texels share one tile (the packed tail): level offsets
inside it (`PackedMipOffset`): 16x16, 8x8, 4x4 stacked across the short side, smaller levels along the long side. When the base itself is the tail, **D3D leaves the mip address at 0** (the tail is at the base:
`TextureLevelCount` handles it). Level count = `min(max level, floor(log2(max(w, h)))) + 1`.

## Vertex formats (`xenos::VertexFormat(declType)`)

Declaration element type words (low 6 bits = Xenos vertex format):

| D3DDECLTYPE | Value | Host |
|---|---|---|
| FLOAT1/2/3/4 | `0x2C83A4` / `0x2C23A5` / `0x2A23B9` / `0x1A23A6` | R32 / R32G32 / R32G32B32 / R32G32B32A32 FLOAT |
| D3DCOLOR | `0x182886` | B8G8R8A8_UNORM (stored ARGB, swizzled to RGBA; checked against real hardware: the sky vertex colour `0xFFF07DFE` ARGB = 240,125,254) |
| UBYTE4 / UBYTE4N | `0x1A2286` / `0x1A2086` | R8G8B8A8_UINT / UNORM |
| SHORT2/4, SHORT2N/4N, USHORT2N/4N | `0x2C2359`/`0x1A235A`, `0x2C2159`/`0x1A215A`, `0x2C2059`/`0x1A205A` | R16G16(B16A16) SINT / SNORM / UNORM |
| FLOAT16_2/4 | `0x2C235F` / `0x1A2360` | R16G16(B16A16)_FLOAT |
| **DEC3N**, UDEC3 | `0x2A2187`, `0x2A2287` | **R32_UINT, handed to shaders as a raw uint** (three signed-normalised 10-bit components, X in the low bits) |

Usage -> shader input location (XenosRecomp's USAGE_LOCATIONS): POSITION 0 (index 1 -> 15), NORMAL 1, TANGENT 2, BINORMAL 3, TEXCOORD0-3 4-7 and TEXCOORD4+ 12+, COLOR 0 -> 8 (1 -> 11), BLENDINDICES 9,
BLENDWEIGHT 10, PSIZE 16. **Inputs a shader declares but the declaration lacks read a constant** (0, or 1 for colours, as unbound fetches do): one element of defaults given per instance.
Vertex streams are cached on the GPU keyed by the **range a draw actually reads** ([renderer_threads.md](renderer_threads.md)).
