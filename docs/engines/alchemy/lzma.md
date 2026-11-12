# The game's LZMA decoder loop as host code

> **Purpose:** why the LZMA SDK's inner decoder loop of Giants and Swap Force runs as host code, how, and what it gains · **Status:** current (2026-10-28) · **Tags:** [Alchemy] [Giants] [SwapForce]

Code: [`engines/alchemy/lzma.cpp`](../../../engines/alchemy/lzma.cpp); addresses: `GAME_HOOK_LzmaDec_DecodeReal` in
`games/giants/addresses.h` and `games/swapforce/addresses.h`. Class **[Alchemy]** (every IGA archive is chunked LZMA),
addresses per game. The Lux counterpart (SSA's zlib): [../lux/zlib.md](../lux/zlib.md).

## What the games link [V]
- **The LZMA SDK's decoder, 9.x** (public domain, Igor Pavlov): `LzmaDec_DecodeReal` is Giants' `sub_822BBA60` and Swap
  Force's `sub_82476EF8`, the same code (62 KB of translated C++ each, the same asm). Found by its constants: kTopValue
  (`lis r26,256`), the literal table at `probs + 3692` bytes (1846 probabilities), `kMatchSpecLenStart` 274 added on
  the end marker, the `CLzmaDec` fields at +0..+72 (lc, lp, pb, dicSize, probs, dic, buf, range, code, dicPos,
  dicBufSize, processedPos, checkDicSize, state, reps[4], remainLen).
- **zlib 1.2.3** too (`inflate`: Giants `sub_82337DE8`, Swap Force `sub_82703388`, found by the string "incorrect
  header check"), but no level load or menu calls it: it never showed in the samples of loads below. So no zlib hook.
- Who decodes: the job queue's workers (`JQ Worker` threads: 2 in Swap Force's loads, 5 in Giants'), one archive chunk
  (0x8000 bytes out) per job; the loader thread (`tfbLoadThread` in Giants) and the main thread consume the results.

## The hook
Only the inner loop is replaced: `LzmaDec_DecodeReal(p, limit, bufLimit)` decodes literals, matches and reps until the
dictionary reaches `limit` or the input `bufLimit`, then stores the state back. The host version is written from the
public LZMA format and works on the guest's own state: the `CLzmaDec` struct, the 16-bit **big-endian** probability
table and the dictionary stay in guest memory where the game keeps them, so every other SDK function (DecodeToDic, the
dummy try, WriteRem, the allocators) keeps running translated and sees exactly what it would have.
- `SKY_HOST_LZMA=0`: the translated loop (to measure the difference in the same build).
- `SKY_LZMA_CHECK=1`: every call runs the host loop, then the translated one from a copy of the state as it was, and
  compares the result, the struct, the probabilities and the dictionary (`[lzma] check: N calls, M differ`).
- The log: `[lzma] N calls (host): X MB decoded in Y ms of the decoder threads' time` at 4096 calls and each doubling.

**Checked** [V] (2026-10-28): `SKY_LZMA_CHECK=1`, Swap Force title -> `barrens/brn2` (262 144 calls, 285 MB) and Giants
title -> Chapter 1 (131 072 calls, 201 MB): **0 differences**.

## What it gains [V] (M1 Pro, 2026-10-28, a shared machine: load average 6-40)
Same build, `SKY_HOST_LZMA=0` then default, run one after the other (all the numbers:
[renderer_threads.md](../../runtime/gpu/renderer_threads.md), "Loading and frame time per game"):

| Game | Decoder threads' time, translated -> host | Level load (its own file to the level), translated -> host | In the level, 30 fps |
|---|---|---|---|
| Swap Force, start -> `brn2` (285 MB) | 8.5-10.1 s -> 4.8-5.9 s | `brn2.pak`: 5.6 / 6.8 / 8.7 -> 4.9 / 5.0 / 5.3 s | - |
| Giants, start -> Chapter 1 (201 MB) | 9.5-11.0 s -> 5.3-5.7 s | `level_213_tutotial.arc`: 1.73 / 2.33 -> 1.23 / 1.00 s | process CPU 17.7-18.1 -> 15.1-15.8 ms a frame, work p99 17-18 -> 12-15 ms (the game streams) |

The decoder's own time halves (~30 -> ~55-60 MB/s per thread, under load). What is left of a load is the games' own
loading code: Giants' `tfbLoadThread` (busy ~72 % of a load) and the ~4 s of save and fade before its `.arc`, Swap
Force's main thread (~60 %); the decoding workers are no longer saturated.

## Finding it in another Alchemy game
In the translated code (`private/<game>/ppc/ppc_recomp.*.cpp`), the one function whose asm comments hold `lis r26,256`,
`addi r3,r27,3692` and ~22 `rlwinm rX,rY,21,11,31` (`range >> 11`); it is ~62 KB of C++. Check the `CLzmaDec` offsets
against the loads at its start (`lwz r11,8(r3)` pb, `4(r3)` lp, `0(r3)` lc, `16(r3)` probs, `52(r3)` state, `24(r3)` buf),
add `GAME_HOOK_LzmaDec_DecodeReal`, and run once with `SKY_LZMA_CHECK=1` through a few loads (0 differences).
