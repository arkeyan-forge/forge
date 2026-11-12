#!/usr/bin/env python3
"""Grid score of presented frames: SSA's sun light tiles shaded with the wrong class show as blocks of 32x32 scene pixels.

The score is the mean luminance step across the tile boundaries over the mean step elsewhere (1.0 = no grid; the
blocks raise it). The scene (SCENE_W x SCENE_H guest pixels, 1120x704 in SSA) is stretched over the whole frame, so a
tile is 32 * frame width / SCENE_W host pixels wide. docs/runtime/gpu/plume_backends.md has the measurements.

    tools/regress/grid_score.py <frame.ppm|png> ... [--scene 1120x704] [--tile 32]
Prints one line per frame (score, block edges), then the mean and the worst frame. A block edge is a tile edge segment,
one tile long, where the luminance jumps across the edge in one direction while both sides are flat.
"""
import argparse
import sys

import numpy as np
from PIL import Image


def luminance(path):
    rgb = np.asarray(Image.open(path).convert("RGB"), dtype=np.float32)
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def axis_score(steps, size, scene, tile):
    """steps: |L[i+1] - L[i]| along one axis, averaged over the other; i in 0..size-2."""
    pitch = tile * size / scene
    boundary = np.zeros(size - 1, dtype=bool)
    k = 1
    while k * pitch < size - 1:
        edge = int(round(k * pitch))  # the step between edge - 1 and edge crosses the boundary
        if 1 <= edge < size:
            boundary[edge - 1] = True
        k += 1
    if not boundary.any() or boundary.all():
        return None
    return float(steps[boundary].mean()), float(steps[~boundary].mean())


def edges(lum, scene_w, scene_h, tile):
    """Tile edge segments (one tile long) whose luminance jumps across the edge, flat on both sides: a block's side."""
    h, w = lum.shape
    count = 0
    for image, size, scene, other, other_scene in ((lum, w, scene_w, h, scene_h), (lum.T, h, scene_h, w, scene_w)):
        pitch, along = tile * size / scene, tile * other / other_scene
        k = 1
        while k * pitch < size - 5:
            e = int(round(k * pitch))
            if e >= 5:
                cross = np.abs(image[:, e + 1] - image[:, e - 2])
                inside = (np.abs(image[:, e - 2] - image[:, e - 5]) + np.abs(image[:, e + 4] - image[:, e + 1])) / 2
                j = 0
                while (j + 1) * along <= other:
                    a, b = int(round(j * along)), int(round((j + 1) * along))
                    jump = image[a:b, e + 1] - image[a:b, e - 2]
                    c, i = cross[a:b].mean(), inside[a:b].mean()
                    # one direction along the whole segment: a block's side, not texture noise
                    if c > 6 and c > 3 * i and (np.sign(jump) == np.sign(jump.sum())).mean() > 0.8:
                        count += 1
                    j += 1
            k += 1
    return count


def score(path, scene_w, scene_h, tile):
    lum = luminance(path)
    h, w = lum.shape
    cols = np.abs(np.diff(lum, axis=1)).mean(axis=0)  # w - 1
    rows = np.abs(np.diff(lum, axis=0)).mean(axis=1)  # h - 1
    on = off = 0.0
    for result in (axis_score(cols, w, scene_w, tile), axis_score(rows, h, scene_h, tile)):
        if result is not None:
            on += result[0]
            off += result[1]
    return on / off if off > 0 else float("nan")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", nargs="+")
    parser.add_argument("--scene", default="1120x704")
    parser.add_argument("--tile", type=int, default=32)
    args = parser.parse_args()
    scene_w, scene_h = (int(v) for v in args.scene.split("x"))
    scores, counts = [], []
    for path in args.frames:
        s = score(path, scene_w, scene_h, args.tile)
        n = edges(luminance(path), scene_w, scene_h, args.tile)
        scores.append(s)
        counts.append(n)
        print(f"{s:.3f} {n:4d} {path}")
    if scores:
        # a black frame (the start) has no steps: no score
        print(f"mean {np.nanmean(scores):.3f} worst {np.nanmax(scores):.3f} block edges {sum(counts)} "
              f"(worst frame {max(counts)}) frames {len(scores)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
