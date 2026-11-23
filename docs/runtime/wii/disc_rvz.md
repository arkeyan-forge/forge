# The Wii disc from an RVZ image

> **Purpose:** how does the forge read a Wii disc from the player's RVZ image, and how does it prove the data intact? · **Status:** current (2026-11-23) · **Tags:** none

Code: [`disc/rvz.{h,cpp}`](../../../runtime/disc/rvz.cpp) (the image), [`disc/wii_disc.{h,cpp}`](../../../runtime/disc/wii_disc.cpp)
(partitions, file system, hash check), [`vfs.cpp`](../../../runtime/vfs.cpp) (`DiscFileSystem::Open` takes an RVZ).
Test: `ArkeyanRvzTest`.

## Why RVZ only
A Wii disc's partitions are encrypted with a key of the console's. An RVZ stores them already decrypted, so the forge
holds no console key and the player's image is about a third of the disc's size. ISO and WBFS images are refused.
The format is Dolphin's, read here from its published description; no code is taken from it.

## The image
All numbers big-endian. Offsets in the file unless said.

| Part | Layout |
|---|---|
| Header 1 (0x48) | magic `RVZ\1`, version, oldest compatible version, size of header 2, SHA-1 of header 2, disc size (0x24), file size (0x2C), SHA-1 of its first 0x34 bytes |
| Header 2 (0xDC) | disc type (2 Wii), compression (0 none, 5 Zstandard; 2 bzip2 and 3/4 LZMA refused), level, chunk size (32 KiB to 2 MiB, a power of two), the disc's first 0x80 bytes, then the three tables below |
| Partitions (0x30 each, stored plain, SHA-1 in header 2) | the title key (unused here), then two ranges: first disc sector (0x8000 bytes), sector count, first group, group count |
| Plain regions (0x18 each, compressed) | disc offset, size, first group, group count; groups start at the offset rounded down to 0x8000 |
| Groups (0x0C each, compressed) | file offset / 4, stored size (bit 31: compressed), packed size (0: not packed) |

A group holds one chunk of the disc. In a partition, a chunk is its sectors' data only, 0x7C00 bytes each (the 0x400
bytes of hashes are dropped and rebuilt), preceded by the **hash exceptions**: one list per 64 sectors, a count then
(offset, SHA-1) pairs, for the hashes the disc stores differently from what its data gives; the offset is the sector in
the group times 0x400 plus the place in its hashes. Not compressed, the lists are padded to 4 bytes. A stored size of 0 is a
chunk of zeros.

**Packed** groups are runs: a 4-byte length, then the bytes, or, with bit 31 set, the 68-byte seed of the disc's
padding generator. The generator is a lagged Fibonacci one (17 seed words, lags 521 and 32; each output word gives
its bytes from bits 24, 18, 8 and 0). It restarts every 0x8000 bytes of the data it fills (disc offsets in the plain
regions, offsets in the partition's data in a partition), so a run that starts inside a block skips that much of its
output first. Checked on SKNP52: every partition's padding matches the disc's hashes.

## The Wii disc
| Where | What |
|---|---|
| Disc 0x40000 | the partition table: 4 groups of (count, table offset / 4); entries (partition offset / 4, type: 0 the game, 1 an update) |
| Partition + 0x2A4 | title metadata size and offset / 4 (the first content's SHA-1 at +0x1F4 is the hash of the H3 table), H3 table offset / 4 (0x2B4, 0x18000 bytes), data offset / 4 (0x2B8) |
| Partition data 0 | `boot.bin` (0x440: game id, title at 0x20, `main.dol` offset / 4 at 0x420, file system offset / 4 and size / 4 at 0x424), `bi2.bin` (0x2000 at 0x440), the apploader (0x2440, 0x20 + its sizes at 0x14 and 0x18) |
| File system | 12-byte entries: kind (1 directory) and name offset, then a file's offset / 4 and size, or a directory's parent and the index past its last entry; the names follow the entries |

`WiiDisc` lists the game partition as an extractor lays it out: `sys/boot.bin`, `sys/bi2.bin`, `sys/apploader.img`,
`sys/main.dol`, `sys/fst.bin`, and the file system under `files/`.

## The hash check
`WiiDisc::Verify` rebuilds every sector's hashes from its data: 31 H0 (one per 0x400 bytes), the H1 of each group of 8
sectors (over their H0), the H2 of each group of 64 (over the H1), with the image's exceptions put in place of the
computed ones at each level. The SHA-1 of each group of 64's H2 must be its entry in the partition's H3 table, and the
H3 table's SHA-1 the title metadata's. A short last group is hashed as if sectors of zeros filled it. Every byte of every
partition is checked this way: SKNP52 (the European disc, 140,467 sectors) is intact in 33 s on an M-series Mac, with
no exception. An image whose exceptions sit in a group smaller than 64 sectors has not been seen yet: their offsets are
read as relative to the group's first sector.
