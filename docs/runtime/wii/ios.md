# The Wii host's IOS: devices answered by the host

> **Purpose:** how does the host answer IOS, the Wii's system on its second processor, and which devices does it serve? · **Status:** current (2026-11-24) · **Tags:** [V]

## The calls
The game reaches IOS through the SDK's IOS client: `IOS_Open(path, mode)` gives a descriptor, then `IOS_Close`,
`IOS_Read`, `IOS_Write`, `IOS_SeekAsync`, `IOS_Ioctl(fd, command, in, inLength, out, outLength)`,
`IOS_Ioctlv(fd, command, inCount, outCount, vectors)`, each also asynchronous (`...Async`, a callback and its
argument after the other arguments). The host takes the calls over (`runtime/wii/ios_calls.cpp`, bound in
`games/racing/hooks.cpp`): the arguments from the registers (an ioctlv's vectors from guest memory: address and length
for each), the result in r3. An asynchronous call returns 0 at once, and its callback (result, argument) runs as an
event ([threads.md](threads.md)), as IOS's reply interrupt would make it run; a request a device answers later
(`Device::Pending`, an event hook) has no callback until then. Each kind of request is logged its first three times.

## The devices (`runtime/wii/ios.{h,cpp}`, `ios_devices.cpp`)
`wii::ios::Ios` serves devices by path (each open makes one), 32 descriptors; a path outside `/dev/` is a NAND file
(`Ios::ServeFiles`); any other path nothing serves is not found (-6), as on the console. IOS's results: >= 0 a descriptor or a count, -1 no access, -4 invalid, -6 not found, -22 no
descriptor left.

| Path | Answer |
|---|---|
| `/dev/stm/immediate` | the state module: power, the screen's dimming: every request done |
| `/dev/stm/eventhook` | its one request waits for the power or reset button: pending for good |
| `/dev/di` | the disc drive, from the player's image (`ios_disc.cpp`): below |
| `/dev/fs`, the NAND's files | the NAND, in a folder of the host (`ios_nand.cpp`, `nand.{h,cpp}`): below |
| `/dev/es` | the title service: the title's ID and its data folder (`ios_nand.cpp`): below |
| everything else | not found: `/dev/net/kd/request` and `/dev/net/kd/time` (WiiConnect24: the SDK logs its failures and goes on, offline), `/dev/usb/oh1/57e/305` (the Bluetooth dongle: the remotes' library is answered above it, below) |

**`/dev/di`**: every request a 0x20-byte block, its command in the first word's top byte; DI's own result, 1 for
success, 2 for an error. Read (0x71: the opened partition's data, offset in words, `disc::WiiDisc::Read`),
UnencryptedRead (0x8D: the disc itself, `ReadRaw`), ReadDiskID (0x70: the disc's first 0x20 bytes), Inquiry (0x12),
OpenPartition (the ioctlv 0x8B: the game partition only; its title metadata back, `WiiDisc::TitleMetadata`, ES's error
0), the cover (0x7A: a disc in; 0x86, 0x88), the status registers (0x95, 0x96), RequestError (0xE0: none), Reset
(0x8A), ClosePartition (0x8C), SetMaximumRotation (0xE3); any other command is logged once and answered as done.

**The NAND** (`wii::nand::FileSystem`): the console's flash, as IOS's file system (ISFS) serves it, in the folder
`ArkeyanWiiRuntime` is given: one host folder for each NAND folder, one host file for each file. A path is absolute,
each name 12 characters at most, no `.` or `..` (anything else is refused, so the game never reaches outside the
folder). ISFS's results: -101 invalid, -102 no access (a file opened without the mode), -103 the host refused, -105
already there, -106 not found, -115 a folder not empty. `/dev/fs`'s requests, as the SDK's ISFS client lays them out
(read from the game's own code): a path is 64 bytes; an entry's attributes are 76 (owner, group, the path at 6, the
three permissions at 70, the flags at 73). The ioctls: 2 GetStats (the clusters of 16 KB, plenty free), 3 CreateDir and
9 CreateFile (the attributes), 5 SetAttr (accepted: the host keeps no owners or permissions, every entry reads read
and write for all), 6 GetAttr, 7 Delete (a file, or an empty folder), 8 Rename (two paths; it replaces what was at
the new one), 13 Shutdown; the ioctlvs 4 ReadDir (the count; with a buffer, as many names as fit, a NUL after each)
and 12 GetUsage (the clusters and entries under a folder). A file opened by its path: Read, Write, Seek (inside the
file only), the ioctl 11 GetFileStats (its size and position); each write goes straight to the host file.

**What the NAND holds before the game** (`wii::PrepareNand`, each made only when missing, so the game's saves and
changes stay): `/tmp`, `/shared2/sys`, the title's data folder (`/title/00010000/<game ID in hex>/data`, where the
SDK's NAND library makes the game's home), the console's settings `/shared2/sys/SYSCONF` and the system menu's
`setting.txt` (below).

**`/dev/es`**: the ioctlvs 32 GetTitleID (the title metadata's ID, at its 0x18C) and 29 GetDataDir (that title's data
folder). Any other request is logged once and refused (ES's -1017): the boot asks 27 (the disc's ticket view) and
goes on.

## The console's settings (`runtime/wii/settings.{h,cpp}`)
The SDK's settings library (SC) reads two files, from their public description and the game's own reads:
- **SYSCONF** (`wii::MakeSysconf`, 16 KB): "SCv0", the item count and each item's offset (and the end's), the items,
  "SCed" in the last 4 bytes; the library keeps its index in the 76 bytes before those, so the items end before. An
  item: a byte (its type in the top 3 bits: 1 an array up to 64 KB, 2 an array up to 256 bytes, 3 a byte, 5 a 32-bit
  number; below, its name's length less one), the name, the value (an array's length less one first). The host's:
  the language (`IPL.LNG`, English for now), the screen (`IPL.AR` 16:9, `IPL.PGS` progressive, `IPL.E60` PAL at 60
  Hz, `IPL.DH`), the sound (`IPL.SND` stereo), `IPL.CB`, `IPL.IDL`, `IPL.SSV`, and the remotes' (`BT.DINF` and
  `BT.CDIF` with no remote paired, `BT.SENS`, `BT.SPKV`, `BT.MOT`, `BT.BAR`).
- **setting.txt** (`wii::MakeSettingText`, 256 bytes): the console's region as `KEY=value` lines (`AREA`, `MODEL`,
  `DVD`, `MPCH`, `CODE`, `SERNO`, `VIDEO`, `GAME`), from the disc's region letter (the game ID's fourth: J, E, K, else
  Europe), encrypted byte by byte with a rolling key (`0x73B5DBFA`, turned left by one bit after each byte). The SDK
  reads it from low memory (`0x80003800`, where the system menu leaves it: the boot information,
  [memory_boot.md](memory_boot.md)) and from the NAND.

## The firmware check
OSInit compares the IOS running (`0x80003140`) with the one the loader started (`0x80003188`): both written from the
title metadata's ([memory_boot.md](memory_boot.md)), else "This firmware is an improper version for this SDK".

## How far it boots [V] (2026-11-24)
SuperChargers Racing, with the NAND and ES: the settings read (SYSCONF, `setting.txt`), the NAND library's home from
ES, WiiConnect24 offline, the disc drive; the engine's start then goes through its saves (the home folder), its
streams, its resources, the input (KPAD over the answered remotes' library), its packages and the graphics (the first
frame buffer handed to VI), and waits in the audio interface's start (`__AI_SRC_INIT`, which times the interface's
sample counter): the audio is #82's next piece. Before the NAND, the save manager's start failed, and the engine's
shutdown then stopped in the network library (`bdSingleton.inl` line 29).
