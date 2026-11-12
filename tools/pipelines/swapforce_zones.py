#!/usr/bin/env python3
"""Every zone Swap Force can load, from the player's disc ([Alchemy] [SwapForce]; tools/pipelines/tour_swapforce.sh).

The zones are the CZoneInfo objects of /archives/zoneinfos.pak (one maps\\<area>\\[<group>\\]<map>\\<map>_zoneinfo.mua
each); their _zoneName ("<area>/[<group>/]<map>/<map>") is the name the game looks up (Zone_IndexByName) and what
SKY_LEVEL takes. A zone whose map has no /archives/<map>.pak (golden, the lockpick template, the town archetype
pieces) cannot be loaded alone and is left out (listed on stderr).

  swapforce_zones.py <Swap Force .iso> <scratch dir>     (prints one zone name per line)"""
import os
import re
import subprocess
import sys

root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(root, "tools"))
import toy_tables  # noqa: E402  (the IGA reader)

iso, scratch = sys.argv[1], sys.argv[2]
xdvdfs = os.path.join(root, "tools", "shaders", "xdvdfs.py")
listing = subprocess.run([sys.executable, "-I", xdvdfs, iso], check=True, capture_output=True, text=True).stdout
paks = set(re.findall(r"/archives/([^/\s]+)\.pak$", listing.lower(), re.M))
subprocess.run([sys.executable, "-I", xdvdfs, iso, scratch, "/archives/zoneinfos.pak"], check=True, capture_output=True)
blob = open(os.path.join(scratch, "archives", "zoneinfos.pak"), "rb").read()
zones = []
for name, entry in toy_tables.iga_entries(blob).items():
    if name.lower().endswith("_zoneinfo.mua"):
        found = re.search(rb'name="_zoneName" value="([^"]+)"', toy_tables.iga_read(blob, entry))
        if found:
            zones.append(found.group(1).decode().lower())
for zone in sorted(set(zones)):
    if zone.split("/")[-1] in paks:
        print(zone)
    else:
        print(f"left out (no /archives/{zone.split('/')[-1]}.pak): {zone}", file=sys.stderr)
