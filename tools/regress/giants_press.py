#!/usr/bin/env python3
"""The SKY_PRESS script of the Giants runs (tools/test_all.sh, tools/pipelines/tour_giants.sh): A every 4 s through the
title and the menus (the first level is playable at ~30-45 s), then from 40 s A, the left stick in four directions, X, Y
and B in turn, and the pause menu (two pages) in the 20 s before the end.

  giants_press.py [<run seconds, default 100>]"""
import sys

end = float(sys.argv[1]) if len(sys.argv) > 1 else 100
pause = end - 20
p = [f"a:{t}" for t in range(8, 37, 4)]
dirs = ["lup", "lleft", "ldown", "lright"]
t, i = 40.0, 0
while t < end - 4:
    if not pause - 1 <= t < pause + 13:
        p += [f"a:{t:g}", f"{dirs[i % 4]}:{t + 0.3:g}:2.5", f"x:{t + 0.6:g}", f"x:{t + 1.2:g}", f"y:{t + 1.8:g}",
              f"b:{t + 2.4:g}", f"y:{t + 3.0:g}:1"]
        i += 1
    t += 4
p += [f"start:{pause:g}", f"down:{pause + 1.5:g}", f"a:{pause + 3:g}", f"right:{pause + 5:g}", f"b:{pause + 7:g}",
      f"down:{pause + 8:g}", f"a:{pause + 9:g}", f"b:{pause + 11:g}", f"start:{pause + 12.5:g}"]
print(",".join(p))
