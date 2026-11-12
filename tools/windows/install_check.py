"""Disc-free checks used by install_test.sh (standard library only)."""
import re
import sys
import struct
from pathlib import Path


class CheckError(ValueError):
    pass


def prepare(root, out):
    root, out = root.resolve(), out.resolve()
    scratch = root / ".scratch"
    if scratch.resolve() != scratch or scratch not in out.parents:
        raise CheckError("output must be below this clone's .scratch directory")
    if out.exists() and (not out.is_dir() or any(out.iterdir())):
        raise CheckError("output must be new or empty; choose another .scratch directory")
    print(out)


def metadata(kit, game):
    values = {}
    for line in (kit / "config" / game / "game.txt").read_text().splitlines():
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key] = value
    if values.get("id") != game or not re.fullmatch(r"[A-Za-z0-9_][A-Za-z0-9_ '.-]*", values.get("runtime", "")):
        raise CheckError("kit game metadata is invalid")
    if not values.get("movie_shaders", "").isdigit() or int(values["movie_shaders"]) == 0:
        raise CheckError("kit must declare a positive movie_shaders target")
    return values


def shader_file(path):
    if not re.fullmatch(r"[0-9a-fA-F]{16}\.(vs|ps)", path.name) or not path.is_file():
        return False
    with path.open("rb") as stream:
        header = stream.read(12)
    if len(header) != 12:
        return False
    flags, virtual, physical = struct.unpack(">III", header)
    return (flags == (0x102A1101 if path.suffix == ".vs" else 0x102A1100)
            and virtual >= 0x40 and virtual % 4 == 0 and physical > 0
            and path.stat().st_size == virtual + physical)


def installed(kit, game, out, log):
    values = metadata(kit, game)
    text = log.read_text(errors="replace")
    exe = values["runtime"] + ".exe"
    done = re.findall(r"^@done (.+)$", text, re.M)
    if len(done) != 1 or done[0].strip().replace("\\", "/").lower() != f"z:/work/game/{exe}".lower():
        raise CheckError("install needs exactly one @done naming the installed executable")
    if re.search(r"^@error(?: |$)", text, re.M):
        raise CheckError("install emitted @error")
    if not (out / "game" / exe).is_file() or (out / "game" / exe).stat().st_size == 0:
        raise CheckError("installed executable is missing or empty")
    work = out / "game/arkeyan-work"
    files = list((work / "shaders/input/runtime").glob("*"))
    shaders = [p for p in files if shader_file(p)]
    expected = int(values["movie_shaders"])
    stamp = (work / "movies/done.txt").read_text().strip()
    if len(shaders) < expected or not stamp.isdigit() or int(stamp) != len(shaders):
        raise CheckError(f"run-time shaders: {len(shaders)}/{expected}; completion marker must match the files")
    cache = (work / "ppc/shader_cache.cpp").read_text()
    hashes = {int(h, 16) for h in re.findall(r"^\t\{ 0x([0-9a-fA-F]+),", cache, re.M)}
    if any(int(p.stem, 16) not in hashes for p in shaders):
        raise CheckError("a captured run-time shader is absent from the translated cache")
    print(f"PASS install: @done, {len(shaders)}/{expected} run-time shaders, all translated")


def ppm(path):
    """Read the runtime's binary RGB PPM, including normal PPM comments."""
    data = path.read_bytes()
    header = re.match(rb"P6\s+(?:#[^\n]*\n\s*)*(\d+)\s+(\d+)\s+255(?:\r\n|\n| )", data)
    if not header:
        raise CheckError(f"invalid presented image: {path.name}")
    width, height = map(int, header.groups())
    pixels = data[header.end():]
    if width < 64 or height < 64 or len(pixels) != width * height * 3:
        raise CheckError(f"incomplete presented image: {path.name}")
    # Use the centre 80%: a border or an overlay alone cannot rescue a black picture.
    luma = []
    for y in range(height // 10, height * 9 // 10):
        for x in range(width // 10, width * 9 // 10):
            at = (y * width + x) * 3
            r, g, b = pixels[at:at + 3]
            luma.append((54 * r + 183 * g + 19 * b) / 256)
    mean = sum(luma) / len(luma)
    bright = sum(v > 16 for v in luma) / len(luma)
    return mean, bright, max(luma) - min(luma)


def played(out, seconds, attempt):
    run = out / f"run-{attempt}"
    status = (run / "status").read_text().strip()
    if status not in ("124", "137"):
        raise CheckError(f"game exited early (status {status}); expected the {seconds} s time limit")
    logs = [p for p in out.glob("run-*/*.log") if p.is_file()]
    for log in logs:
        text = log.read_text(errors="replace")
        missing = len(re.findall(r"no translated", text, re.I))
        if missing:
            raise CheckError(f"{log.parent.name}/{log.name}: {missing} 'no translated' lines")
        if re.search(r"Fault \(|Indirect call to unrecompiled|\[crash\]", text):
            raise CheckError(f"game fault in {log.parent.name}/{log.name}")
    # Two recent presents are needed; an early title frame cannot hide a later black picture or a hang.
    tail = sorted(run.glob("shot_t*.ppm"), key=lambda p: float(p.stem.split("_t")[1]))
    recent = [p for p in tail if float(p.stem.split("_t")[1]) >= seconds - 65]
    if len(recent) < 2 or float(recent[-1].stem.split("_t")[1]) < seconds - 15:
        raise CheckError("need two recent presented images, including one near the end of the run")
    for shot in recent:
        mean, bright, spread = ppm(shot)
        if mean < 3 or bright < 0.01 or spread < 16:
            raise CheckError(f"black/blank presented picture: {shot.name} (luma {mean:.1f}, lit {bright:.1%})")
    print(f"PASS Vulkan: {seconds} s, zero 'no translated' lines, {len(recent)} recent nonblack presents")


def main(args):
    try:
        mode, *rest = args
        if mode == "install":
            kit, game, out, log = rest
            installed(Path(kit), game, Path(out), Path(log))
        elif mode == "play":
            out, seconds, attempt = rest
            played(Path(out), int(seconds), int(attempt))
        elif mode == "prepare":
            root, out = rest
            prepare(Path(root), Path(out))
        elif mode == "runtime":
            kit, game = rest
            print(metadata(Path(kit), game)["runtime"])
        else:
            raise CheckError("unknown check")
    except (CheckError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
