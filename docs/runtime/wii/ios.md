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
`wii::ios::Ios` serves devices by path (each open makes one), 32 descriptors; a path nothing serves is not found (-6),
as on the console. IOS's results: >= 0 a descriptor or a count, -1 no access, -4 invalid, -6 not found, -22 no
descriptor left.

| Path | Answer |
|---|---|
| `/dev/stm/immediate` | the state module: power, the screen's dimming: every request done |
| `/dev/stm/eventhook` | its one request waits for the power or reset button: pending for good |
| `/dev/di` | the disc drive, from the player's image (`ios_disc.cpp`): below |
| everything else | not found: `/dev/fs` (the NAND's files), `/dev/es` (titles), `/dev/net/kd/request` (WiiConnect24) for now |

**`/dev/di`**: every request a 0x20-byte block, its command in the first word's top byte; DI's own result, 1 for
success, 2 for an error. Read (0x71: the opened partition's data, offset in words, `disc::WiiDisc::Read`),
UnencryptedRead (0x8D: the disc itself, `ReadRaw`), ReadDiskID (0x70: the disc's first 0x20 bytes), Inquiry (0x12),
OpenPartition (the ioctlv 0x8B: the game partition only; its title metadata back, `WiiDisc::TitleMetadata`, ES's error
0), the cover (0x7A: a disc in; 0x86, 0x88), the status registers (0x95, 0x96), RequestError (0xE0: none), Reset
(0x8A), ClosePartition (0x8C), SetMaximumRotation (0xE3); any other command is logged once and answered as done.

## The firmware check
OSInit compares the IOS running (`0x80003140`) with the one the loader started (`0x80003188`): both written from the
title metadata's ([memory_boot.md](memory_boot.md)), else "This firmware is an improper version for this SDK".

## How far it boots [V] (2026-11-24)
SuperChargers Racing: OSInit's IOS requests (`/dev/stm`, the network's WiiConnect24, the disc drive), the first GX
"draw done", the engine's first thread, then the game's own start stops in its network library's memory hook
(`bdMemory`'s allocator not set, an assertion in `bdSingleton.inl`): its setter runs on a path that needs what IOS does
not serve yet (`/dev/fs`, `/dev/es`).
