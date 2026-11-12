"""Writes a ported game's hook inventory as a sigmatch hooks file, to find that game's hooks in a later Alchemy game.

usage: python3 -I tools/sigmatch/game_inventory.py <game id> [out.json]      (default: tools/sigmatch/<game>_hooks.json)
  then, with the game's image and function list from its own disc and the target's image from tools/day1.sh:
  python3 -I tools/sigmatch/sigmatch.py make tools/sigmatch/<game>_hooks.json --image <game image.bin> \
      --funcs private/<game>/ppc/ppc_func_mapping.cpp --sigs private/<target>/day1/<game>_sigs.json
  python3 -I tools/sigmatch/sigmatch.py match tools/sigmatch/<game>_hooks.json private/<target>/day1/image.bin \
      --sigs private/<target>/day1/<game>_sigs.json --out private/<target>/day1/<game>_sigmap.json --toml

Entries: every GAME_HOOK_ and nonzero data address of games/<game>/addresses.h and the [main] block of its config. The field
"ssa" holds that game's own address (sigmatch's name for the reference). Metadata only: the signatures (game bytes) go to
private/. tools/sigmatch/giants_inventory.py is the same for Giants (it does not join continued #define lines).
"""
import glob
import json
import re
import sys

ROOT = __file__.rsplit("/tools/", 1)[0]
game = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else f"{ROOT}/tools/sigmatch/{game}_hooks.json"
src = re.sub(r"\\\n\s*", " ", open(f"{ROOT}/games/{game}/addresses.h").read())
config = glob.glob(f"{ROOT}/games/{game}/config/*.toml")
toml = open(next(c for c in config if "switch_tables" not in c)).read()
what = f"games/{game}/addresses.h"
entries = []
for name, address in re.findall(r"#define GAME_HOOK_(\w+)\s+sub_([0-9A-F]{8})", src):
    entries.append({"name": name, "kind": "hook", "ssa": f"0x{address}", "what": what,
                    "scope": "xbox" if name.startswith(("D3D", "Xma", "Bink")) else "alchemy"})
for name, address in re.findall(r"constexpr uint32_t (\w+) = (0x8[0-9A-F]{7})", src):
    entries.append({"name": name, "kind": "data", "ssa": address, "what": what,
                    "scope": "xbox" if name.startswith("Engine") else "alchemy"})
for name, address in re.findall(r"^(\w+_address) = (0x[0-9A-F]{8})", toml, re.M):
    entries.append({"name": name, "kind": "config", "ssa": address, "what": f"games/{game}/config", "scope": "xbox"})
with open(out, "w") as f:
    json.dump({"description": __doc__.strip().splitlines()[0], "entries": entries}, f, indent=1)
    f.write("\n")
print(f"{len(entries)} entries -> {out}")
