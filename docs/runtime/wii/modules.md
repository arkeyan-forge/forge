# The Wii host's modules: an RSO translated where the host loads it

> **Purpose:** how does a module the game loads at run time (an RSO) get translated, and where does the host put it? · **Status:** current (2026-11-25) · **Tags:** [V]

## The problem
A Wii game may keep part of its code in an RSO, a relocatable module it reads from the disc and links at run time
(SuperChargers Racing: `files/Data/gamelogic.rso`, 3.1 MB of code, the game's logic). The engine loads it into its heap
(`GEEngineMgrImp::InitGL`): wherever the heap has room, which changes from one boot to the next (`0x80EA9B80`, then
`0x80EC1B80`). A translation is for one address: the module's must be where it was translated for.

## Its place
The host keeps an area past MEM1 for it, where no Wii game's memory is (`wii::module::Area`, mapped by the host as
`GameAddresses::moduleArea`; SuperChargers Racing: `0x81800000`, 5 MB). The module's code must stay within 32 MB of the
engine it calls (its `REL24` branches); past MEM1's end it is. The engine's two allocations for it (the file, then its
BSS) come from that area: `MMMemoryMgr::Alloc`, by its return addresses in `InitGL`; the engine's heap never shrinks
(`MMHeap::Realloc`, the link's last step) nor frees (`MMMemoryMgr::Free`) a block of the area. The game's own linker
(`RSOLinkListFixed`) then links it there, as the translation did; the host checks it did: the FNV-1a hash of the linked
sections against the one `tools/rso_link.py` wrote beside the translation (`ppc_module_link.h`), and stops the boot
when they differ.

## The link (`tools/rso_link.py`)
The SDK's linker, read from its code in the game (`LocateObject`, `RSORelocate`, `LinkList`):
- each section's address is the module's plus its offset; the section with a size and no offset is the BSS, at its
  own address (the host: after the file, 32-byte aligned);
- an internal relocation's offset is from the module's start, its symbol the section it points into, plus its addend;
- an import's relocations point at the module's `unresolved` function, then at the export of that name: the engine's
  export table (`ExposeEngineRSOSymbols`: 206 entries of 12 bytes at `0x80510498`, a section, the name's address, the
  symbol's), not the ELF's symbols: `sqrt`, `sin`, `cos` and 8 others are the single-precision functions there;
  `gpAPIInstall` and `smWiiMoteExtension__15InputManagerImp` are not exported and stay at `unresolved`;
- the types: 1 ADDR32, 2 ADDR24, 3 ADDR16, 4 ADDR16_LO, 5 ADDR16_HI, 6 ADDR16_HA, 7 ADDR14, 10 REL24, 11 REL14.

The module has no symbols for its own functions; they are found in the linked code: its prolog, epilog and unresolved
functions, its export, every `bl` target, every code address its data or its `lis`/`addi` pairs hold (vtables,
function pointers) but a jump table's labels (a table an `addi` loads for an `lwzx`, `mtctr`, `bctr`, as long as its
bounds check says), and every branch target from another function (a tail call) when an unconditional end comes before
it (padding, `blr`, `b`, `bctr`), until no branch finds another. SuperChargers Racing: 15,449 functions; the
translation has no call to an address that is no function's start.

The tool writes one ELF: the game's, the module's linked sections appended at their addresses, and a symbol table of
the module's functions. XenonRecomp translates it as one image (one function table, direct calls both ways), and
`tools/elf_switch_tables.py` finds the jump tables of both (SuperChargers Racing: 231, 30 of them the module's).

## Tests
`rso_link.py --self-test` (in `tools/test_all.sh`): a synthetic module linked against a synthetic game (an internal
branch, an import resolved through an export table rather than the symbol, a jump table and a function pointer side by
side, a function only a tail call reaches, a branch out of reach). `ArkeyanWiiTest`: the area's blocks, the hash of a
linked module's sections (its BSS left out).
