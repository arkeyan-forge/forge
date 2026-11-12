#!/usr/bin/env python3
"""Per-channel levels of a raw float32 48 kHz interleaved capture (SDL's disk driver, SKY_AUDIO_DUMP).

    python3 -I tools/regress/surround_rms.py <capture.f32> [channels=6] [--check [--front-back]]

Prints one line per channel: RMS (dBFS), peak, and the share of the signal's power below 120 Hz (the LFE must be low),
then the correlation of each back channel with each front one over the whole capture (a back pair in the right order
follows its own side). Only the frames after the first second of sound count, so the device's silent start does not dilute
the levels; the share below 120 Hz is measured on the first 60 s.
--check: exit 1 unless every channel carries signal above -70 dBFS, the centre, LFE and back channels are not copies of
the front ones (correlation below 0.995) and the LFE has at least 70 % of its power below 120 Hz (the runtime's low-pass;
unfiltered, Giants' and Trap Team's have under 10 %).
--front-back: only the front and back pairs must carry signal (SSA: its Wwise mix leaves the centre and the LFE empty).
"""
import array
import math
import sys

NAMES = ["FL", "FR", "C", "LFE", "BL", "BR"]
RATE = 48000


def read(path, channels):
    samples = array.array("f")
    with open(path, "rb") as f:
        data = f.read()
    count = len(data) // (4 * channels)
    samples.frombytes(data[: count * channels * 4])
    if sys.byteorder != "little":
        samples.byteswap()
    return [samples[c::channels] for c in range(channels)], count


def rms(values):
    return math.sqrt(sum(v * v for v in values) / len(values)) if values else 0.0


def db(value):
    return 20 * math.log10(value) if value > 0 else -math.inf


def low_share(values, cutoff=120.0):
    # The power through a 4th-order Butterworth low-pass (host/speakers.h's LFE filter) over the whole power.
    w = 2 * math.pi * cutoff / RATE
    stages = []
    for q in (0.54119610014619698, 1.3065629648763766):
        alpha = math.sin(w) / (2 * q)
        a0 = 1 + alpha
        stages.append([(1 - math.cos(w)) / 2 / a0, (1 - math.cos(w)) / a0, -2 * math.cos(w) / a0, (1 - alpha) / a0,
                       0.0, 0.0, 0.0, 0.0])
    low_power, power = 0.0, 0.0
    for v in values:
        x = v
        for st in stages:
            b0, b1, a1, a2, x1, x2, y1, y2 = st
            y = b0 * x + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2
            st[4], st[5], st[6], st[7] = x, x1, y, y1
            x = y
        low_power += x * x
        power += v * v
    return low_power / power if power > 0 else 0.0


def correlation(a, b):
    n = len(a)
    if n == 0:
        return 0.0
    ma, mb = sum(a) / n, sum(b) / n
    sab = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    saa = sum((x - ma) ** 2 for x in a)
    sbb = sum((y - mb) ** 2 for y in b)
    return sab / math.sqrt(saa * sbb) if saa > 0 and sbb > 0 else 0.0


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    check = "--check" in sys.argv
    if not args:
        print(__doc__)
        return 2
    channels = int(args[1]) if len(args) > 1 else 6
    data, count = read(args[0], channels)
    # Start at the first non-silent frame, plus a second.
    start = next((i for i in range(0, count, 256) if any(data[c][i] != 0.0 for c in range(channels))), count)
    start = min(count, start + RATE)
    # Levels on every 4th frame, the low-pass and the correlations on the first 60 s.
    data = [d[start:] for d in data]
    seconds = len(data[0]) / RATE
    print("capture: %s, %d channels, %.1f s analysed" % (args[0], channels, seconds))
    if seconds < 1:
        print("FAIL: under a second of sound")
        return 1
    names = NAMES if channels == 6 else ["ch%d" % c for c in range(channels)]
    window = [d[: RATE * 60] for d in data]  # the low-pass needs every frame
    levels, lows = [], []
    for c in range(channels):
        levels.append(db(rms(data[c][::4])))
        lows.append(low_share(window[c]))
        peak = max(abs(v) for v in data[c])
        print("%-3s rms %7.1f dBFS  peak %.3f  below 120 Hz %5.1f %%" % (names[c], levels[c], peak, 100 * lows[c]))
    fail = []
    required = range(channels)
    if channels == 6:
        if "--front-back" in sys.argv:
            required = [NAMES.index(n) for n in ("FL", "FR", "BL", "BR")]
        sub = [d[::16] for d in data]  # the whole capture
        pairs = [("BL", "FL"), ("BL", "FR"), ("BR", "FL"), ("BR", "FR"), ("C", "FL"), ("LFE", "FL"), ("BL", "BR")]
        text = []
        for x, y in pairs:
            r = correlation(sub[NAMES.index(x)], sub[NAMES.index(y)])
            text.append("%s/%s %.2f" % (x, y, r))
            if y != "BR" and abs(r) > 0.995 and NAMES.index(x) in required:
                fail.append("%s is a copy of %s (correlation %.3f)" % (x, y, r))
        print("correlation: " + ", ".join(text))
        lfe = NAMES.index("LFE")
        if lfe in required and levels[lfe] >= -70 and lows[lfe] < 0.7:
            fail.append("LFE not band-limited (%.0f %% below 120 Hz)" % (100 * lows[lfe]))
    for c in required:
        if levels[c] < -70:
            fail.append("%s silent (%.1f dBFS)" % (names[c], levels[c]))
    if check:
        if fail:
            print("FAIL: " + "; ".join(fail))
            return 1
        print("PASS: %d channels with distinct signal" % len(required))
    return 0


if __name__ == "__main__":
    sys.exit(main())
