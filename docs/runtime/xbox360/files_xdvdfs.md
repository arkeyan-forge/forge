# Files and XDVDFS

> **Purpose:** how does the runtime read the disc (XDVDFS) and answer the Nt file API and content roots? · **Status:** current (2026-10-26) · **Tags:** [Xbox] [SSA]

Code: [`vfs.{h,cpp}`](../../../runtime/vfs.cpp) (disc reader), [`kernel/files.cpp`](../../../runtime/kernel/files.cpp)
(Nt* file API), [`tools/shaders/xdvdfs.py`](../../../tools/shaders/xdvdfs.py) (offline reader). Class: **[Xbox]**
unless marked.

## XDVDFS (the disc file system)

| Fact | Value |
|---|---|
| Sector size | 2048 bytes |
| Volume descriptor | magic `MICROSOFT*XBOX*MEDIA` at `partition + 32 * 2048`; then `u32 root sector`, `u32 root size` (little-endian on disc) |
| Game partition offsets | `0x0` (plain), `0x2080000` (XGD3), `0xFD90000` (XGD2), `0x18300000` (XGD1) -- the code tries them in this order; SSA's Redump image is XGD3 |
| Directory | the entries form a **binary tree**: `u16 left, u16 right` (offset in dwords from the directory start, `0xFFFF`/`0xFFFF` = unused), `u32 sector`, `u32 size`, `u8 attributes (0x10 = directory)`, `u8 nameLength`, name (Latin-1) |
| Case | names are case-insensitive: `Normalize` lowercases and uses `/` |
| Read | positional reads of the ISO (`PlatformReadFile`, safe from several threads); the whole tree is indexed in memory at open (`m_entries`) |
| Files of SSA | 1142 files (8.10 GB), disc image 8 738 846 720 bytes. `default.xex` 12 595 200 bytes |

**A bad image (0.9.2, [Xbox] [V]).** `IndexDirectory` walks each directory extent once (`m_directories`), so a record that points
back at a directory already walked, as in an 80 KB image whose root holds many records leading to itself, no longer branches to
the power of the depth: `Open` returns false and lists nothing, as it does for a tree deeper than 64 levels or over 262 144 entries.
A directory larger than 16 MB or running past the end of the image is skipped (a truncated image still opens, as far as it goes). The
launcher shows that as "Not an Xbox 360 disc image, or the file is incomplete". Proof: `ArkeyanVfsTest` (the looping, fanned, deep, long
and truncated images), `./ArkeyanVfsTest <scratch dir>`; the launcher's `tests/vfs_test.cpp` and its start with such an image registered.

**Folder mode.** `DiscFileSystem::Open` also takes a folder holding the disc's files (Arkeyan Launcher's "Copy the game data": the
copy, its manifest and the runtime's choice are [host/game_data.md](../host/game_data.md)): the folder is walked once, paths are
lowercased like the disc's, and a file is opened per read (`Entry::host`; `offset` is unused). `Files()` lists the files.

The same reader exists in Python (`xdvdfs.py <iso> [outdir files...]`) for the shader cache and tools.

## Names the title uses

`game:`, `d:`, `\Device\Cdrom0`, `\??\game:` and symbolic links created with `ObCreateSymbolicLink` are **all the same disc**:
`DiscPath` strips the device/drive and lowercases. `ObCreateSymbolicLink` only logs `[file] symbolic link A -> B` and succeeds;
`ObDeleteSymbolicLink` succeeds. Save content is mounted as the drive name the title passes to `XamContentCreateEx`
(**`lux:`** for the Lux games) onto a host folder ([xam_content_saves.md](xam_content_saves.md)).

## Nt file API (all transfers are synchronous)

| Call | Semantics |
|---|---|
| `NtCreateFile` / `NtOpenFile` | OBJECT_ATTRIBUTES: `+0` root directory handle, `+4` ANSI_STRING (len u16 at +0, pointer at +4). Dispositions 0-5 (supersede/open/create/open-if/overwrite/overwrite-if) implemented for host files; **options are the 9th argument of `NtCreateFile` at `r1+0x54`** (`FILE_DIRECTORY_FILE` 1, `FILE_NON_DIRECTORY_FILE` 0x40, `FILE_DELETE_ON_CLOSE` 0x1000). Logs `[file] open <path> -> 0xSTATUS` |
| `NtReadFile` / `NtWriteFile` | (handle, event, apc routine, apc context, io status, buffer, length, byte offset*). A negative or absent offset = current position. Completion: write IO_STATUS_BLOCK (`+0` status, `+4` information), queue the APC (routine pointer with its low bit cleared) on the calling thread, signal the event handle. Read past the end with length > 0 returns `STATUS_END_OF_FILE`. Disc files are read-only (`STATUS_ACCESS_DENIED` on write). `SKY_TRACE_FILES=1` logs every transfer |
| `NtQueryInformationFile` | classes 5 (FileStandard: 0x18 bytes), 6 (Internal: disc offset or a hash), 14 (Position), 34 (NetworkOpen: 0x38 bytes: 4 times, allocation, end of file, attributes). Other classes: log + `STATUS_INVALID_PARAMETER` |
| `NtSetInformationFile` | 10 Rename (BOOLEAN replace, root handle, ANSI name; only content files), 13 Disposition (delete on close), 14 Position, 19 Allocation (accepted), 20 EndOfFile (`resize_file`) |
| `NtQueryFullAttributesFile` | NetworkOpen information for a path, disc or content |
| `NtQueryVolumeInformationFile` | class 3 (FileFsSize): units of 32 sectors x 512 bytes; disc: 0/0; content: host `space()` |
| `NtFlushBuffersFile` | flushes a writable host stream |
| `NtQueryDirectoryFile`, `NtReadFileScatter`, `NtCancelIoFile` | weak stubs: never called (XAPI's table slot for `NtQueryDirectoryFile` at `*(0x82A1E308)` is never read) |

Disc attributes: size from the directory entry, time 0, `FILE_ATTRIBUTE_READONLY` (0x01) or `DIRECTORY` (0x10).
Host files: `NORMAL` (0x80), times from the host's last write time converted to the 1601 epoch.

XAPI file calls go through a function table at `*(0x82A1E308)` [SSA] (address).

## Content (host) files

`MountContentRoot(name, directory)` / `UnmountContentRoot`: lowercase drive name -> host directory. A guest path
`lux:\GLOBAL.DAT` resolves to `<dir>/<component>`; each component goes through `HostFileName`: control characters,
non-ASCII and `<>:"/\|?*` become `_`; `.` and `..` are refused (`STATUS_OBJECT_NAME_INVALID`: **content paths cannot leave
their root**). Stream-based I/O (`std::fstream`), one mutex per file.

## Level redirect test aid [SSA]

`SKY_LEVEL=<level>` rewrites every `\level_027` path component followed by `\` or `.` (levels dir, audio bank
`audio\levels\level_027.bnk`, `audio\level_027.lxb`, `text\<lang>\level_027.lxb`) to `<level>`, so the fast-recipe "new game"
starts in another level. **Giants needs its own first-level name** (`RedirectLevel`).

## Pipeline hook on level loads

`OpenFile` calls `gpu::WaitForPipelineList(path)` after a successful open of a disc file whose path starts with `\levels\`, ends in
`.str` and contains `\mission` (every level's `mission.str`, the frontend, the hub's `mission_stage<n>.str`): the loading screen
waits until every listed pipeline is built ([gpu/pipeline_cache.md](../gpu/pipeline_cache.md)). **[Lux]** file naming.
