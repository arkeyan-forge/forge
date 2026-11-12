#!/usr/bin/env python3
"""Checks on the output of one regression game run (tools/test_all.sh). Needs numpy and Pillow.

  analyze.py scan   <game log> <refs dir> [--record]      faults, [guard] and [stub] lines against the allow-lists
  analyze.py opens  <game log> <text> [<text>...]         the log shows each text (a file open, a marker)
  analyze.py fps    <frames.txt> <run seconds> <floor> <window> [--end]
  analyze.py audio  <raw float32 stereo 48 kHz> <min blocks> <max repeat percent>
  analyze.py tour   <summary.tsv> <known failing levels> [--record]
  analyze.py frame  <snapshot dir> <reference.png> <min psnr dB> [--record] [--single]
                                                          (a set: reference.png plus reference_*.png; --single: reference.png only)
  analyze.py shimmer <snapshot dir> <frames.txt> <from s> <max %>
                                                          (consecutive frames, Swap Force's lava canyon: no patch
                                                          that flips from one frame to the next)
  analyze.py banding <snapshot dir> <max ratio>           (Swap Force's lava canyon: no posterized gradient)
  analyze.py sharpness <min gain %> <reference image> <image>...
                                                          (the detail of each image against the reference: the mean absolute
                                                          Laplacian of the grey 3D area, the middle 80 % wide, rows 12-62 %;
                                                          edge smoothing: the same static frame at Game, Light and Off)

Each prints one line (the reason) and exits 0 (PASS) or 1 (FAIL). The allow-lists and the reference image live in
private/test-refs/<game>/ (never in git: they come from the game). `--record` writes them from this run instead of
comparing."""
import glob
import os
import re
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError:
    sys.exit("needs numpy and Pillow (python3 -m pip install numpy pillow)")


def out(ok, text):
    print(text)
    sys.exit(0 if ok else 1)


def read_set(path):
    """The lines of an allow-list; `#` starts a comment (the reason a level is on the list)."""
    if not os.path.exists(path):
        return None
    return {line.split("#")[0].strip() for line in open(path)} - {""}


def cmd_scan(log, refs, record):
    text = open(log, errors="replace").read()
    faults = re.findall(r"^.*(?:Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed).*$", text, re.M)
    guards = sorted(set(re.findall(r"\[guard\] bad indirect call to (0x[0-9A-Fa-f]+)", text)))
    stubs = sorted(set(re.findall(r"^\[stub\] (\S+)", text, re.M)))
    if faults:
        out(False, f"{len(faults)} fault line(s), first: {faults[0].strip()[:100]}")
    os.makedirs(refs, exist_ok=True)
    for name, now in (("guards.txt", guards), ("stubs.txt", stubs)):
        path = os.path.join(refs, name)
        if record:
            open(path, "w").write("\n".join(now) + "\n")
    if record:
        out(True, f"recorded {len(stubs)} stub(s), {len(guards)} guard(s)")
    allowed_guards, allowed_stubs = read_set(os.path.join(refs, "guards.txt")), read_set(os.path.join(refs, "stubs.txt"))
    if allowed_guards is None or allowed_stubs is None:
        out(False, f"no allow-list in {refs} (run once with --record on a good build)")
    new_guards, new_stubs = set(guards) - allowed_guards, set(stubs) - allowed_stubs
    if new_guards or new_stubs:
        out(False, f"new [guard] {sorted(new_guards)[:3]} new [stub] {sorted(new_stubs)[:5]}")
    out(True, f"no fault; {len(stubs)} stub(s) and {len(guards)} guard(s), all allowed")


def cmd_opens(log, needles):
    text = open(log, errors="replace").read()
    missing = [n for n in needles if n not in text]
    out(not missing, f"missing in the log: {missing}" if missing else f"found: {', '.join(needles)}")


def cmd_fps(path, seconds, floor, window, end):
    """Frame times (ms since start, work ms, draws). Window: with --end, the last <window> s of the run; otherwise
    +10..+40 s after the level's first frame (>= 1000 draws), as tools/perf/frames.py."""
    frames = [tuple(float(x) for x in l.split()) for l in open(path) if len(l.split()) == 3]
    if len(frames) < 30:
        out(False, f"only {len(frames)} frames: the game did not render")
    last = frames[-1][0] / 1000
    if last < seconds - 12:
        out(False, f"hang: the last frame is at {last:.0f} s of a {seconds:.0f} s run")
    if end:
        t0 = seconds * 1000 - window * 1000
        sel = [i for i in range(1, len(frames)) if frames[i][0] >= t0]
        what = f"last {window:.0f} s"
    else:
        start = next((i for i, f in enumerate(frames) if f[2] >= 1000), None)
        if start is None:
            out(False, "no level frame (>= 1000 draws)")
        t0 = frames[start][0] + 10000
        sel = [i for i in range(start + 1, len(frames)) if t0 <= frames[i][0] <= t0 + window * 1000]
        what = f"level +10..+{10 + window:.0f} s"
    if len(sel) < 30:
        out(False, f"window too short ({len(sel)} frames)")
    span = (frames[sel[-1]][0] - frames[sel[0] - 1][0]) / 1000
    fps = len(sel) / span
    gaps = sorted(frames[i][0] - frames[i - 1][0] for i in sel)
    p99 = gaps[int(0.99 * (len(gaps) - 1))]
    out(fps >= floor, f"{fps:.1f} fps over {what} (floor {floor:g}), p99 gap {p99:.0f} ms, max {gaps[-1]:.0f} ms")


def cmd_audio(path, min_blocks, max_percent):
    data = np.fromfile(path, dtype="<f4")
    block = 1024 * 2
    n = len(data) // block
    blocks = data[: n * block].reshape(n, block)
    loud = np.abs(blocks).max(axis=1) > 1e-4
    pairs = loud[1:] & loud[:-1]
    same = (blocks[1:] == blocks[:-1]).all(axis=1) & pairs
    if loud.sum() < min_blocks:
        out(False, f"almost no sound: {int(loud.sum())} non-silent 1024-sample blocks (need {min_blocks})")
    percent = 100 * same.sum() / max(pairs.sum(), 1)
    out(percent <= max_percent, f"{percent:.2f}% of {int(pairs.sum())} non-silent blocks repeat the previous one "
        f"(limit {max_percent:g}%), {n * 1024 / 48000:.0f} s captured")


def cmd_tour(summary, known, record):
    """Giants' level tour (tools/pipelines/tour_giants.sh): the levels that fault, stop or are not reached must be the ones
    in the known list (one level per line, `#` comments); a level that now works is reported but passes."""
    rows = [l.rstrip("\n").split("\t") for l in open(summary) if l.strip()]
    bad = sorted(r[0] for r in rows if r[-1] != "ok")
    if record:
        open(known, "w").write("\n".join(bad) + "\n")
        out(True, f"recorded {len(bad)} known failing level(s) of {len(rows)}")
    allowed = read_set(known)
    if allowed is None:
        out(False, f"no list {known} (run once with --record on a good build)")
    new = [b for b in bad if b not in allowed]
    fixed = sorted(allowed - set(bad))
    out(not new, f"{len(rows) - len(bad)}/{len(rows)} levels ok; new failures {new[:5]}; now ok {fixed[:5]}")


def small(image):
    return np.asarray(image.convert("L").resize((160, 90), Image.BOX), dtype=np.float64)


def psnr(a, b):
    mse = ((a - b) ** 2).mean()
    return 99.0 if mse == 0 else 10 * np.log10(255 * 255 / mse)


def reference_frames(ref):
    """The reference set: <ref> (frame.png) and its siblings frame_*.png, in the same folder."""
    folder, name = os.path.split(ref)
    stem = os.path.splitext(name)[0]
    extra = sorted(glob.glob(os.path.join(folder, stem + "_*.png")))
    return ([ref] if os.path.exists(ref) else []) + extra


MAX_REFERENCES = 16
DUPLICATE_DB = 35.0  # a recorded frame this close to one already in the set adds nothing
UNREADABLE = (OSError, ValueError, SyntaxError, Image.DecompressionBombError)  # a half-written or corrupt image


def shot_reduction(path):
    """The 160x90 greyscale reduction of a snapshot, or None if it cannot be read. The game may be stopped while it
    writes a snapshot, so the file can be truncated; that is the harness's problem, not a failure of the game."""
    try:
        return small(Image.open(path))
    except UNREADABLE as error:
        print(f"skipping {path}: {error}")
        return None


def cmd_frame(folder, ref, threshold, record, single):
    # The last 20 snapshots (one every 60 frames) are the run's resting scene. Scenes with dialogue or a moving camera
    # differ from run to run with the timing, so the reference is a SET of frames: the check passes when any of the last
    # 20 snapshots matches any reference. --record adds this run's distinct frames to the set (every 4th of the last 20,
    # skipping near-duplicates, at most MAX_REFERENCES); delete the game's frame*.png first to start the set over.
    # --single keeps the old check for steady scenes (SSA, Giants' first run): frame.png only, the 10th from the end.
    shots = [p for p in glob.glob(os.path.join(folder, "*.ppm")) if "_t" not in os.path.basename(p)]  # not the timed ones
    shots = sorted(shots, key=lambda p: int(re.findall(r"\d+", os.path.basename(p))[-1]))
    shots = shots[-20:]
    if not shots:
        out(False, "no frame was dumped")
    readable = {}
    for shot in shots:
        now = shot_reduction(shot)
        if now is not None:
            readable[shot] = now
    if not readable:
        out(False, f"no shot can be read among the last {len(shots)} snapshot(s)")
    shots = list(readable)
    if record and single:
        os.makedirs(os.path.dirname(ref), exist_ok=True)
        Image.open(shots[-10]).convert("RGB").save(ref)
        for old in reference_frames(ref)[1:]:
            os.remove(old)
        out(True, f"reference recorded from {os.path.basename(shots[-10])} of the last {len(shots)}")
    if record:
        os.makedirs(os.path.dirname(ref), exist_ok=True)
        have = [small(Image.open(r)) for r in reference_frames(ref)]
        folder_name, name = os.path.split(ref)
        stem = os.path.splitext(name)[0]
        added = 0
        for shot in shots[::4]:
            if len(have) >= MAX_REFERENCES:
                break
            image = Image.open(shot).convert("RGB")
            now = readable[shot]
            if any(psnr(now, h) >= DUPLICATE_DB for h in have):
                continue
            path = ref if not os.path.exists(ref) else os.path.join(folder_name, f"{stem}_{len(have):02d}.png")
            image.save(path)
            have.append(now)
            added += 1
        out(True, f"{added} frame(s) added to the reference set, {len(have)} in all")
    refs = reference_frames(ref)[:1] if single else reference_frames(ref)
    if not refs:
        out(False, f"no reference {ref} (run once with --record on a good build)")
    wants = [small(Image.open(r)) for r in refs]
    scores = [(psnr(now, w), os.path.basename(s), os.path.basename(r)) for s, now in readable.items() for w, r in zip(wants, refs)]
    best = max(scores)
    out(best[0] >= threshold, f"best of the last {len(shots)} frames x {len(refs)} reference(s): PSNR {best[0]:.1f} dB "
        f"({best[1]} vs {best[2]}; min {threshold:g})")


def cmd_shimmer(folder, frames_path, start, max_percent):
    """Consecutive presented frames (SKY_GPU_SNAPSHOT_SECONDS=<s>,<s+0.001>,...: <prefix>_t<s>.ppm, one per present) of
    a scene that moves smoothly (Swap Force's tutorial, the ship in the lava canyon, at a steady 30 or 60 FPS: the
    per-frame readback of the snapshots holds a 60 FPS run at 30 while it lasts). Per pixel of the upper
    part (the canyon walls), the part of the luma that flips with the frame's parity: |mean_i (-1)^i (f_i - (f_i-1 +
    f_i+1) / 2)|, a lock-in at the Nyquist frequency. A smooth motion averages out; a patch that is drawn on one frame and
    not the next (decals depth-fighting the wall: the missing polygon offset, games/swapforce/docs/shaders.md) stays.
    Fails when more than <max %> of the pixels flip by more than 16 levels (good build 0-0.7 %, without the polygon
    offset 1.7-3.3 %; scale 1 or 2, 30 or 60 FPS), when the frames are not the lava canyon (orange) or not at a steady
    rate (uneven steps)."""
    shots = sorted(glob.glob(os.path.join(folder, "*_t*.ppm")),
                   key=lambda f: float(re.search(r"_t([0-9.]+)\.ppm$", f).group(1)))
    if len(shots) < 30:
        out(False, f"{len(shots)} consecutive frames saved (30 needed): the game did not reach {start:g} s")
    frames = [tuple(float(x) for x in l.split()) for l in open(frames_path) if len(l.split()) == 3]
    gaps = sorted(frames[i][0] - frames[i - 1][0] for i in range(1, len(frames))
                  if (start - 1) * 1000 <= frames[i][0] <= (start + 4) * 1000)
    median = gaps[len(gaps) // 2] if gaps else 0.0
    fps = 60 if median < 25 else 30
    if abs(median - 1000.0 / fps) > 1.0:
        out(False, f"no steady 60 or 30 FPS around {start:g} s (median gap {median:.1f} ms over {len(gaps)} frames): no measure")
    rgb = [np.asarray(Image.open(f).convert("RGB"), np.float32) for f in shots]
    rgb = [a[: a.shape[0] * 45 // 100] for a in rgb]
    mean = np.mean([a.reshape(-1, 3).mean(axis=0) for a in rgb], axis=0)
    if not (mean[0] > 110 and mean[0] > 1.6 * mean[2]):
        out(False, f"the frames are not the lava canyon (upper part RGB {mean[0]:.0f} {mean[1]:.0f} {mean[2]:.0f})")
    luma = np.array([a @ np.array([0.299, 0.587, 0.114], np.float32) for a in rgb])
    second = luma[1:-1] - (luma[:-2] + luma[2:]) / 2
    sign = np.where(np.arange(len(second)) % 2 == 0, 1.0, -1.0).astype(np.float32)[:, None, None]
    flip = np.abs((second * sign).mean(axis=0))
    percent = 100.0 * float((flip > 16).mean())
    out(percent <= max_percent, f"{len(shots)} frames at {fps} FPS (median gap {median:.1f} ms): {percent:.2f} % of the "
        f"canyon's pixels flip from frame to frame by more than 16 levels (max {max_percent:g} %), mean {flip.mean():.2f}")


def cmd_banding(folder, max_ratio):
    """Consecutive presented frames of the lava canyon: the histogram of the green channel over the upper part. A smooth
    gradient gives a smooth histogram (each bin 1.2 x its neighbours at most); a colour table read through doubled entries
    (a table target scaled with the frame) piles half of each step on one level: bins 6-19 x their neighbours at scale 2
    (games/swapforce/docs/shaders.md, "The lava's posterized gradients")."""
    shots = sorted(glob.glob(os.path.join(folder, "*_t*.ppm")))
    if not shots:
        out(False, "no frames saved")
    worst = 0.0
    for shot in shots[:: max(len(shots) // 5, 1)]:
        a = np.asarray(Image.open(shot).convert("RGB"), np.float32)
        hist = np.bincount(a[: a.shape[0] * 45 // 100, :, 1].ravel().astype(int), minlength=256)[30:200].astype(float)
        worst = max(worst, float((hist[1:-1] / np.maximum((hist[:-2] + hist[2:]) / 2, 1)).max()))
    out(worst <= max_ratio, f"the worst histogram bin is {worst:.2f} x its neighbours (max {max_ratio:g}) over {len(shots)} frames")


def detail(path):
    """The mean absolute Laplacian of the grey picture's 3D area (the HUD and the screen's edges are outside it)."""
    grey = np.asarray(Image.open(path).convert("L"), np.float32)
    rows, cols = grey.shape
    area = grey[rows * 12 // 100 : rows * 62 // 100, cols // 10 : cols * 9 // 10]
    return float(np.abs(4 * area[1:-1, 1:-1] - area[:-2, 1:-1] - area[2:, 1:-1] - area[1:-1, :-2] - area[1:-1, 2:]).mean())


def cmd_sharpness(min_gain, reference, images):
    """More detail than the reference by at least min_gain percent (a negative number: at most that much less). A picture of
    the same frame at another edge smoothing: Game is the reference, Light and Off gain 20-40 % in Swap Force and Trap Team,
    nothing in the games without an FXAA pass (games/trapteam/docs/status.md, "Edge smoothing")."""
    base = detail(reference)
    gains = [100 * (detail(image) / base - 1) for image in images]
    text = ", ".join(f"{os.path.join(os.path.basename(os.path.dirname(i)), os.path.basename(i))} {g:+.1f} %" for i, g in zip(images, gains))
    out(all(g >= min_gain for g in gains), f"detail against {os.path.basename(reference)} ({base:.3f}): {text} (min {min_gain:g} %)")


if __name__ == "__main__":
    a = [x for x in sys.argv[1:] if not x.startswith("--")]
    flags = [x for x in sys.argv[1:] if x.startswith("--")]
    cmd = a[0]
    if cmd == "scan":
        cmd_scan(a[1], a[2], "--record" in flags)
    elif cmd == "opens":
        cmd_opens(a[1], a[2:])
    elif cmd == "fps":
        cmd_fps(a[1], float(a[2]), float(a[3]), float(a[4]), "--end" in flags)
    elif cmd == "audio":
        cmd_audio(a[1], int(a[2]), float(a[3]))
    elif cmd == "tour":
        cmd_tour(a[1], a[2], "--record" in flags)
    elif cmd == "shimmer":
        cmd_shimmer(a[1], a[2], float(a[3]), float(a[4]))
    elif cmd == "banding":
        cmd_banding(a[1], float(a[2]))
    elif cmd == "sharpness":
        cmd_sharpness(float(a[1]), a[2], a[3:])
    elif cmd == "frame":
        cmd_frame(a[1], a[2], float(a[3]), "--record" in flags, "--single" in flags)
    else:
        sys.exit(__doc__)
