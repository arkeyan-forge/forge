"""Writes Giants' hook inventory as a sigmatch hooks file, to find Giants' hooks in a later Alchemy game (Swap Force...).

usage: python3 -I tools/sigmatch/giants_inventory.py [tools/sigmatch/giants_hooks.json]
  then (Giants' image and function list from its own disc, the target's image from tools/day1.sh):
  python3 -I tools/sigmatch/sigmatch.py make tools/sigmatch/giants_hooks.json --image <Giants image.bin> \
      --funcs private/giants/ppc/ppc_func_mapping.cpp --sigs private/<game>/day1/giants_sigs.json
  python3 -I tools/sigmatch/sigmatch.py match tools/sigmatch/giants_hooks.json private/<game>/day1/image.bin \
      --sigs private/<game>/day1/giants_sigs.json --out private/<game>/day1/giants_sigmap.json --toml

Entries: every GAME_HOOK_ and nonzero data address of games/giants/addresses.h and the [main] block of
games/giants/config/Giants.toml. The field "ssa" holds the Giants address (sigmatch's name for the reference). Metadata
only: the signatures (game bytes) go to private/.
"""
import json
import re
import sys

ROOT = __file__.rsplit("/tools/", 1)[0]
out = sys.argv[1] if len(sys.argv) > 1 else f"{ROOT}/tools/sigmatch/giants_hooks.json"
src = open(f"{ROOT}/games/giants/addresses.h").read()
toml = open(f"{ROOT}/games/giants/config/Giants.toml").read()
entries = []
for name, address in re.findall(r"#define GAME_HOOK_(\w+) sub_([0-9A-F]{8})", src):
    entries.append({"name": name, "kind": "hook", "ssa": f"0x{address}", "what": "games/giants/addresses.h",
                    "scope": "xbox" if name.startswith(("D3D", "Xma", "Bink")) else "alchemy"})
for name, address in re.findall(r"constexpr uint32_t (\w+) = (0x8[0-9A-F]{7})", src):
    entries.append({"name": name, "kind": "data", "ssa": address, "what": "games/giants/addresses.h",
                    "scope": "xbox" if name.startswith("Engine") else "alchemy"})
for name, address in re.findall(r"^(\w+_address) = (0x[0-9A-F]{8})", toml, re.M):
    entries.append({"name": name, "kind": "config", "ssa": address, "what": "games/giants/config/Giants.toml",
                    "scope": "xbox"})
with open(out, "w") as f:
    json.dump({"description": __doc__.strip().splitlines()[0], "entries": entries}, f, indent=1)
    f.write("\n")
print(f"{len(entries)} entries -> {out}")
