#!/usr/bin/env python3
"""Unlocks Giants' Heroic Challenges in a scratch save the way the game stores them: as bits of the slot file.

    giants_unlock.py <save dir>      (the SKY_SAVE_DIR of a run that has already made its slots, e.g. one minute in the hub)

Giants' Save_Slot_N.xsg (716 bytes) is a bit-packed record whose last 4 bytes are the FNV-1a (32-bit) hash of the
others, big-endian; the game refuses a slot whose hash is wrong ("Save content is missing or damaged"). Two areas
decide what Cali's Select Challenge list offers [V] (2026-11-08, the last entry chosen with each variant):
0x40..0x2B8, the purchases ("heroic challenges purchased", expansion unlocks, story flags) that a new game leaves at
zero, gives six challenges (000, 001, 016, 023, 024, 028); the story progress in the header, whose zero bytes
0x2C..0x32 must be set too, gives 39 of the 45, in level order (all but 003, 010, 018, 026, 030 and 031; the last
entry opens challenge_level_055; with 0x0C..0x2B set as well but not 0x2C..0x32, still six). Setting the slot's other
zero bytes (0x00..0x0B, 0x2B8..0x2C7) adds none of the six, nor does placing every character on the portal in the hub
(tried with the six-challenge slot).
This tool sets every zero byte of 0x0C..0x2B8 to 0xFF (the variant tested) and fixes the hash. It also sets the area's
other flags (the hub's first-visit cutscenes are skipped; the slot continues on level_hub_2_stage_4): a test aid, not
a player's save. Which bit is which chapter or purchase was not isolated [?]."""
import glob, os, struct, sys

LO, HI = 0x0C, 0x2B8


def fnv1a(data):
    h = 0x811C9DC5
    for b in data:
        h = ((h ^ b) * 0x01000193) & 0xFFFFFFFF
    return h


def unlock(path):
    d = bytearray(open(path, "rb").read())
    if len(d) != 716:
        raise SystemExit(f"{path}: {len(d)} bytes, expected 716")
    for i in range(LO, HI):
        if d[i] == 0:
            d[i] = 0xFF
    d[-4:] = struct.pack(">I", fnv1a(bytes(d[:-4])))
    open(path, "wb").write(d)


if __name__ == "__main__":
    slots = sorted(glob.glob(os.path.join(sys.argv[1], "*", "Save_Slot_*", "Save_Slot_*.xsg")))
    if not slots:
        raise SystemExit("no Save_Slot_*.xsg under " + sys.argv[1])
    for s in slots:
        unlock(s)
        print("unlocked", s)
