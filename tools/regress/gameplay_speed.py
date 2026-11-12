"""Summaries for tools/regress/gameplay_speed.sh runs (pictures taken at fixed wall-clock seconds, SKY_GPU_SNAPSHOT_SECONDS).

  gameplay_speed.py jump <snap dir> <fps> <name> <jump second>: the jump's airtime: the time Gusto's blue (the mean row of the blue pixels in the
      middle of the picture) stays 40 px above its resting row, the two crossings interpolated between pictures.
  gameplay_speed.py compare <snap dir A> <snap dir B>: the mean absolute difference of the same moment's two pictures (grey, a quarter
      of the resolution), for each second the walk and the jump were photographed: a game that plays at the same speed shows the same
      pictures at any frame rate, so the difference between a 30 and a 60 run should stay near the difference between two runs at 30.
Needs numpy and Pillow, so no -I: the pictures are the run's own.
"""
import glob
import re
import sys

import numpy as np
from PIL import Image


def pictures(snap):
    return {float(re.search(r"s_t([0-9.]+)\.ppm", p).group(1)): p for p in glob.glob(snap + "/s_t*.ppm")}


def gray(path, box=None):
    im = Image.open(path).convert("L")
    if box:
        im = im.crop(box)
    return np.asarray(im, dtype=np.float32)


def jump(snap, fps, name, press):
    pics = pictures(snap)
    ts = sorted(t for t in pics if press - 0.5 < t < press + 3)
    y = []
    for t in ts:
        a = np.asarray(Image.open(pics[t]).convert("RGB"), dtype=np.int16)[100:520, 150:1000]
        r, g, b = a[..., 0], a[..., 1], a[..., 2]
        rows = np.nonzero((b > r + 50) & (b > g + 10) & (b > 90) & (b < 230))[0]
        y.append(rows.mean() if len(rows) > 200 else np.nan)
    y = np.array(y)
    if len(ts) == 1:
        print(f"{name}: fps_limit={fps}: Gusto's row at {ts[0]:g} s ({ts[0] - press:+.2f} s after the press): {y[0]:.0f} px")
        return
    thr = np.nanmedian(y[-5:]) - 40
    k = int(np.nanargmin(y))
    up = next(ts[i] + (ts[i + 1] - ts[i]) * (y[i] - thr) / (y[i] - y[i + 1]) for i in range(k) if y[i] > thr >= y[i + 1])
    down = next(ts[i] + (ts[i + 1] - ts[i]) * (thr - y[i]) / (y[i + 1] - y[i]) for i in range(k, len(y) - 1) if y[i] <= thr < y[i + 1])
    print(f"{name}: fps_limit={fps}: jump airtime {down - up:.2f} s (up {up - press:+.2f}, down {down - press:+.2f} after the press; apex {np.nanmin(y):.0f} px, rest {thr + 40:.0f} px)")


def compare(a, b):
    pa, pb = pictures(a), pictures(b)
    row = []
    for t in sorted(set(pa) & set(pb)):
        ga, gb = gray(pa[t]), gray(pb[t])
        row.append(f"{t:g}:{float(np.abs(ga[::4, ::4] - gb[::4, ::4]).mean()):.1f}")
    print("difference by second: " + " ".join(row))


if sys.argv[1] == "jump":
    jump(sys.argv[2], sys.argv[3], sys.argv[4], float(sys.argv[5]))
else:
    compare(sys.argv[2], sys.argv[3])
