#!/usr/bin/env python3
"""One coverage entry: one isolated, headless, silent run of a game, driven live (docs/status/COVERAGE.md). Used by
tools/regress/coverage_all.sh under the game lock; it is not a user-facing command.

  coverage_run.py <game> <disc> <run dir> <route> <arg> [--play S] [--fsr] [--seed DIR] [--save-to DIR]

Routes: level (SKY_LEVEL=<arg>), hero (SSA: SKY_NEXT_LEVEL=<arg>, the game starts the level itself, so the Skylander
spawns), cali (Giants: Heroic Challenge entry <arg> chosen at Cali on the hub, with an unlocked save in --seed),
battle (SSA Battle Mode, <arg> = "<mode keys>/<arena index>"), credits (<arg> = movies or a level), figures (<arg> = a
list file: label, dump files, toy types; each figure is put on the portal in turn while the hero plays), seed (a new
game to <arg>, its save copied to --save-to, for a later --seed).

The run starts with the game's menu presses, waits for the entry's own proof line in the log (the level's file opened,
the zone started, the challenge's mesh, the arena's mission), then plays --play seconds with the hero moving (stick,
attacks, jump), then stops the game (its own PID only). Prints one JSON object per entry (figures: one per figure):
name, hint (ok, crash, hang, not_reached), reached (s since launch, or -1), seconds (wall), detail. The game's log,
frame times and the last frame stay in <run dir> for tools/regress/coverage_scan.py.
"""
import glob
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXES = {
    "ssa": ("build", "Skylanders Spyro's Adventure"),
    "giants": ("build-giants", "Skylanders Giants"),
    "swapforce": ("build-swapforce", "Skylanders Swap Force"),
    "trapteam": ("build-trapteam", "Skylanders Trap Team"),
    "superchargers": ("build-superchargers", "Skylanders SuperChargers"),
    "imaginators": ("build-imaginators", "Skylanders Imaginators"),
}
ZONE_PREFIX = {"imaginators": "2016/"}
# The figure on the portal when the entry is not about figures: a character the game accepts (SuperChargers: and a
# vehicle, which its levels ask for).
DEFAULT_FIGURES = {"superchargers": "new:3400,new:3227"}
FPS_LIMIT = {"ssa": 30}
# Where the figures are played: each game's first level (Trap Team's tutorial is read from its disc).
FIGURE_LEVEL = {"ssa": "level_027", "giants": "level_213_tutotial", "swapforce": "tutorial/tutorial_a",
                "superchargers": "academy", "imaginators": "level_508"}
FAULT = re.compile(r"Fault \(|Indirect call to unrecompiled|SIGSEGV|SIGBUS|SIGABRT|assertion failed|\[guard\]")


def exe_of(game):
    override = os.environ.get("COVERAGE_EXE_" + game.upper())
    if override:
        return override
    build, name = EXES[game]
    plain = os.path.join(ROOT, build, "ArkeyanRuntime", name)
    bundled = os.path.join(ROOT, build, "ArkeyanRuntime", name + ".app", "Contents", "MacOS", name)
    return plain if os.access(plain, os.X_OK) else bundled


def camel(level):
    """level_hub -> Level_Hub, pvp_level_001 -> PvP_Level_001: SKY_NEXT_LEVEL's names (tour_ssa_story.sh)."""
    name = "_".join(word[:1].upper() + word[1:] for word in level.split("_"))
    return name.replace("Pvp_", "PvP_")


def zone_path(game, arg):
    return arg if "/" in arg else f"{ZONE_PREFIX.get(game, '')}{arg}/{arg}"


def marker(game, route, arg):
    """The log line that proves the entry was reached."""
    if game == "ssa":
        if route == "battle":
            return re.compile(r"levels\\pvp_level_[0-9a-z_]*\\mission\.str -> 0x00000000")
        return re.compile(re.escape(f"\\levels\\{arg}\\mission.str -> 0x00000000"))
    if game in ("giants", "trapteam"):
        if route == "battle":
            return re.compile(r"\\level\\pvp_level_[0-9a-z_]*\.arc -> 0x00000000", re.I)
        if route == "cali":
            return re.compile(r"\\level\\challengemesh_[a-z0-9]*\.bld", re.I)
        return re.compile(re.escape(f"\\level\\{arg}.arc -> 0x00000000"), re.I)
    if game == "swapforce":
        return re.compile(re.escape(f"open \\archives\\{arg.split('/')[-1]}.pak -> 0x00000000"), re.I)
    return re.compile(r"^\[level\] " + re.escape(zone_path(game, arg)) + " starts", re.M)


class Run:
    def __init__(self, game, iso, rundir, route, arg, fsr, seed):
        self.game, self.iso, self.dir, self.route, self.arg = game, iso, rundir, route, arg
        self.log = os.path.join(rundir, "game.log")
        self.presses = os.path.join(rundir, "presses.txt")
        self.script = os.path.join(rundir, "toybox.txt")
        self.frames = os.path.join(rundir, "frames.txt")
        self.start = time.time()
        self.text = ""
        self.offset = 0
        os.makedirs(os.path.join(rundir, "collection"), exist_ok=True)
        os.makedirs(os.path.join(rundir, "save"), exist_ok=True)
        os.makedirs(os.path.join(rundir, "shots"), exist_ok=True)
        if seed:
            subprocess.run(["cp", "-R", seed + "/.", os.path.join(rundir, "save")], check=False)
        open(self.presses, "w").close()
        open(self.script, "w").close()
        self.env = dict(os.environ)
        for key in [k for k in self.env if k.startswith(("SKY_", "SSA_"))]:
            del self.env[key]
        self.env.update({
            "SDL_VIDEO_DRIVER": "offscreen", "SDL_AUDIO_DRIVER": "disk", "SDL_AUDIO_DISK_OUTPUT_FILE": "/dev/null",
            "SKYLANDERS_DATA": os.path.join(rundir, "data"), "SKY_SETTINGS": os.path.join(rundir, "none.ini"),
            "SKY_LAUNCHER_INI": os.path.join(rundir, "none.ini"),
            "SKY_COLLECTION": os.path.join(rundir, "collection"), "SKY_FIGURES": os.path.join(rundir, "collection"),
            "SKY_SAVE_DIR": os.path.join(rundir, "save"), "SKY_LOG": self.log, "SKY_FRAME_TIMES": self.frames,
            "SKY_LAUNCHER_NO_UPDATE_CHECK": "1", "SKY_SKIP_MOVIES": "1", "SKY_PORTAL": "emulated",
            "SKY_PORTAL_PLACE": DEFAULT_FIGURES.get(game, "new:14"), "SKY_PRESS_FILE": self.presses,
            "SKY_TOYBOX_SCRIPT_FILE": self.script, "SKY_GPU_SNAPSHOT": os.path.join(rundir, "shots", "s"),
            "SKY_GPU_SNAPSHOT_FROM": "300", "SKY_GPU_SNAPSHOT_EVERY": "450",
        })
        if fsr:
            self.env["SKY_UPSCALER"] = "fsr3"
        if game in ("ssa", "imaginators", "superchargers"):
            self.env["SKY_TRACE_FILES"] = "1"  # SSA's mission.str, the licence screen and archives in the log
        self.proc = None

    def launch(self, extra):
        self.env.update(extra)
        # SuperChargers' races at 30: at 60 the trophy that reveals a pack's tracks does not take (the race start's fault
        # is fixed: engines/alchemy/heap_guard.cpp). COVERAGE_FPS_LIMIT overrides it.
        fps = 30 if self.route == "scrace" else FPS_LIMIT.get(self.game, 60)
        fps = int(os.environ.get("COVERAGE_FPS_LIMIT", fps))
        args = ["nice", exe_of(self.game), "--vsync", "0", "--fps-limit", str(fps), self.iso]
        self.out = open(os.path.join(self.dir, "stdout.txt"), "w")
        # The game stays in the run's process group: a stop of the whole coverage run reaches it too.
        self.proc = subprocess.Popen(args, env=self.env, stdout=self.out, stderr=subprocess.STDOUT)
        self.start = time.time()

    def now(self):
        return time.time() - self.start

    def alive(self):
        return self.proc is not None and self.proc.poll() is None

    def read(self):
        try:
            with open(self.log, errors="replace") as f:
                f.seek(self.offset)
                chunk = f.read()
        except OSError:
            return ""
        self.offset += len(chunk.encode("utf-8", "replace")) if chunk else 0
        self.text += chunk
        if len(self.text) > 4_000_000:  # trace logs grow: keep the tail for the markers
            self.text = self.text[-2_000_000:]
        return chunk

    def press(self, *lines):
        with open(self.presses, "a") as f:
            for line in lines:
                f.write(line + "\n")

    def toybox(self, *lines):
        with open(self.script, "a") as f:
            for line in lines:
                f.write(line + "\n")

    def frames_age(self):
        try:
            return time.time() - os.path.getmtime(self.frames)
        except OSError:
            return None

    def stop(self):
        if self.alive():
            self.proc.send_signal(signal.SIGTERM)
            for _ in range(30):
                if not self.alive():
                    break
                time.sleep(0.1)
            if self.alive():
                self.proc.kill()
            self.proc.wait()
        if getattr(self, "out", None):
            self.out.close()


class Driver:
    """Steps the run every 0.25 s: the due presses, the log, the end conditions."""

    def __init__(self, run):
        self.run = run
        self.queue = []  # (time since launch, kind, line)
        self.status = None  # "exited", "hang"
        self.hang_after = 45.0

    def at(self, t, line, kind="press"):
        self.queue.append((t, kind, line))
        self.queue.sort(key=lambda item: item[0])

    def step(self):
        run = self.run
        run.read()
        t = run.now()
        while self.queue and self.queue[0][0] <= t:
            _, kind, line = self.queue.pop(0)
            (run.press if kind == "press" else run.toybox)(line)
        if not run.alive():
            self.status = "exited"
            return False
        age = run.frames_age()
        if age is not None and age > self.hang_after:
            self.status = "hang"
            return False
        return True

    def wait(self, seconds, until=None):
        """Runs for `seconds` or until `until(text)` is true; False when the game ended or hung first."""
        end = self.run.now() + seconds
        while self.run.now() < end:
            if not self.step():
                return False
            if until is not None and until(self.run.text):
                return True
            time.sleep(0.25)
        return until is None

    def play(self, start, seconds, pad=""):
        """The hero moves: A (dialogs, jump), the stick one way, two attacks, the other attacks, every 4 s."""
        dirs = ["lup", "lleft", "ldown", "lright"]
        t, i = start, 0
        while t < start + seconds - 1:
            for offset, line in ((0, "a"), (0.3, f"{dirs[i % 4]}:2.5"), (0.6, "x"), (1.2, "x"), (1.8, "y"),
                                 (2.4, "b"), (3.0, "y:1")):
                self.at(t + offset, pad + line)
            t += 4
            i += 1


def menus(driver, game, route):
    """The presses from the launch to the first level (times since launch), as the per-game tours make them."""
    if game == "ssa":
        for t in range(1, 6):
            driver.at(t, "start")
        for k in range(15):
            driver.at(6 + 1.5 * k, "a")
        for t in range(30, 64, 2):
            driver.at(t, "b")
    elif game in ("giants", "trapteam"):
        for t in range(8, 160, 4):
            driver.at(t, "a")
    elif game == "swapforce":
        for t in range(14, 120, 2):
            driver.at(t, "a")


def level_extra(game, route, arg):
    if route == "hero":
        return {"SKY_NEXT_LEVEL": camel(arg)}
    return {"SKY_LEVEL": arg}


def reach(driver, game, route, arg, timeout, seeded):
    """Drives the menus until the entry's proof line; returns its time, or -1."""
    run = driver.run
    proof = marker(game, route, arg)
    found = lambda text: proof.search(text) is not None
    if game == "imaginators":
        # The licence screen comes at a variable time: Start every 5 s until it opens, Y twice, then A every 4 s.
        if not driver.wait(20):
            return -1
        for _ in range(30):
            if "softwarelicense.pak" in run.text.lower() or found(run.text):
                break
            run.press("start")
            if not driver.wait(5):
                return -1
        if not found(run.text):
            driver.wait(8)
            run.press("y")
            driver.wait(3)
            run.press("y")
        while not found(run.text) and run.now() < timeout:
            run.press("a")
            if not driver.wait(4, found) and driver.status is not None:
                return -1
        return run.now() if found(run.text) else -1
    if game == "superchargers":
        if seeded:  # a continued save: A through the title and the save slot to the Academy (renamed: SKY_LEVEL_FROM)
            for t in range(12, 200, 4):
                driver.at(t, "a")
        else:  # a new game: the lobby test's presses (superchargers_lobby.sh), the rift ride's triggers included
            for t in range(12, 45, 4):
                driver.at(t, "a")
            for t in (46, 48, 50, 54):
                driver.at(t, "y")
            for t in range(58, 151, 5):
                driver.at(t, "a")
            for t, hold in ((205, 60), (266, 60), (327, 45)):
                driver.at(t, f"rt:{hold}")
            for t in range(375, 600, 5):
                driver.at(t, "a")
    else:
        menus(driver, game, route)
    if driver.wait(timeout, found):
        return run.now()
    return -1


def cali(driver, k, timeout):
    """Giants: from the hub (an unlocked save), walk to Cali, talk until the challenge select opens, entry k, A."""
    run = driver.run
    # The unlocked save continues on the hub stage its progress names (stage 4 for every challenge unlocked).
    hub = lambda text: re.search(r"level_hub_2_stage_\d\.bld", text, re.I) is not None
    for t in range(8, 40, 4):
        driver.at(t, "a")
    if not driver.wait(120, hub):
        return -1
    if not driver.wait(28):
        return -1
    for line, pause in (("lup:1.5", 5), ("lright:0.4", 3), ("lup:2.5", 5), ("lright:1.6", 3), ("lup:0.5", 3), ("a", 3),
                        ("lleft:0.4", 2), ("ldown:0.3", 2), ("lright:1.2", 3), ("y", 5)):
        run.press(line)
        if not driver.wait(pause):
            return -1
    select = lambda text: "ui_heroicchallengeselect.bld" in text.lower()
    for _ in range(15):
        if select(run.text):
            break
        run.press("a")
        if not driver.wait(3):
            return -1
    if not select(run.text):
        return -1
    driver.wait(6)
    for _ in range(k):
        run.press("right")
        driver.wait(1.2)
    run.press("a")
    proof = marker("giants", "cali", "")
    if driver.wait(timeout, lambda text: proof.search(text) is not None):
        return run.now()
    return -1


def battle(driver, game, arg):
    """Battle Mode through the menus, two pads, as a person starts it: "<mode keys>/<arena index>[/<arena>]". With an
    arena name, the first arena's files are that arena's (SKY_LEVEL_FROM, kernel/level_redirect.h): the game's own
    battle in an arena a new save keeps locked. SSA: tools/regress/ssa_arena.sh's presses; Giants: the title, CHOOSE
    PLAY, Battle Mode (pad 2 connects with a stick move), PICK MODE, PICK ARENA, READY TO BATTLE, each player's
    Skylander."""
    run = driver.run
    keys, _, rest = arg.partition("/")
    arena = rest.split("/")[0]
    if game == "ssa":
        if not driver.wait(90, lambda text: len(glob.glob(os.path.join(run.dir, "shots", "s_*.ppm"))) > 0):
            return -1
        steps = [("2 a", 25), ("2 down", 2), ("2 a", 6), ("2 a", 6)]
        steps += [(f"2 {key}", 2) for key in keys.split()] + [("2 a", 7)]
        steps += [("2 right", 2)] * int(arena or 0) + [("2 a", 6), ("2 a", 25), ("2 a", 4), ("a", 4), ("a", 30)]
        steps += [("b", 1), ("2 b", 15)]
        driver.wait(8)
    else:
        if not driver.wait(90, lambda text: "[level] title starts" in text):
            return -1
        driver.wait(14)
        steps = [("a", 5), ("a", 5), ("right", 2), ("a", 5), ("2 lleft:0.2", 2), ("left", 1.5), ("left", 1.5),
                 ("a", 5)]
        steps += [(key, 1.5) for key in keys.split()] + [("a", 5)]
        steps += [("right", 1.5)] * int(arena or 0) + [("a", 5), ("a", 7), ("a", 5), ("2 a", 5)]
    for line, pause in steps:
        run.press(line)
        if not driver.wait(pause):
            return -1
    target = (rest.split("/") + [""])[1]
    proof = marker(game, "level", target) if target else marker(game, "battle", "")
    if driver.wait(60, lambda text: proof.search(text) is not None):
        return run.now()
    return -1


def scrace(driver, arg):
    """SuperChargers' Single Race through the menus, "<trophy>/<vehicle>/<stick moves>/<zone>": the title (Fiesta and
    the vehicle on the portal), Racing (X), Single Player, Single Race, the trophy of the track's pack put on the
    portal (one at a time: two are "too many Legendary Treasures"; its message closed, the map opened again: its
    pack's tracks show), the map's cursor moved from its centre ("ldown:0.3,lleft:0.25", 2.5 s apart: closer presses
    are lost), the track, START, the help, the driver, the vehicle (only one of the track's handling type is
    offered). Reached: the zone starts."""
    run = driver.run
    trophy, vehicle, moves, zone = arg.split("/", 3)
    if not driver.wait(120, lambda text: "mainmenubackground starts" in text):
        return -1
    driver.wait(30)
    steps = [("press", "a", 10), ("press", "start", 12), ("press", "x", 5), ("press", "a", 4), ("press", "a", 5)]
    if moves.startswith("pack,"):  # a pack's own tracks show once its trophy comes while the map is open, and the
        # map is opened again (live run)
        dump = os.path.join(run.dir, "collection", f"pack_{trophy}.sky")
        steps += [("toybox", "place=" + dump, 8), ("press", "a", 4), ("press", "b", 4), ("press", "a", 5),
                  ("press", "b", 4), ("press", "a", 5)]  # twice: once was not always enough
        moves = moves[len("pack,"):]
    steps += [("press", m, 2.5) for m in moves.split(",") if m]
    steps += [("press", "a", 4)] * 7
    proof = (re.compile(r"^\[level\] actionpacks/ap[als]\d/ap[als]\d starts", re.M) if zone == "*"
             else marker("superchargers", "level", zone))
    for kind, line, pause in steps:
        (run.toybox if kind == "toybox" else run.press)(line)
        if not driver.wait(pause):
            return -1
        if proof.search(run.text):
            break
    if not driver.wait(60, lambda text: proof.search(text) is not None):
        return -1
    return run.now()


def sfarena(driver, arg):
    """Swap Force's Arena Modes with two pads, "<downs to the mode>/<arena zone>": the title, Arena Modes, a new save,
    Easy, the mode, pad 2 joins, the first arena (its zone renamed to the one asked for: SKY_LEVEL_FROM, a trailing
    slash), then each player's Skylander: player 1 the first figure; player 2 the second, put back on the portal once
    the arena loads (two at once are "too many Skylanders for player 1"), given by the Toy Box the game's prompt
    opens. Reached: the round's countdown."""
    run = driver.run
    downs, _, target = arg.partition("/")
    if not driver.wait(120, lambda text: "mainmenubackground starts" in text):
        return -1
    driver.wait(20)
    steps = [("a", 7), ("down", 2), ("a", 7), ("a", 7), ("a", 7)] + [("down", 2)] * int(downs)
    steps += [("a", 6), ("a", 3), ("2 lleft:0.2", 2), ("2 start", 3), ("a", 7), ("a", 7), ("a", 7), ("left", 2),
              ("a", 3)]
    for line, pause in steps:
        run.press(line)
        if not driver.wait(pause):
            return -1
    # The players' Skylanders are chosen before the arena loads.
    driver.wait(8)
    second = os.path.join(run.dir, "collection", "new_14.sky")
    for kind, line, pause in (("toybox", "remove=1", 5), ("press", "a", 4), ("toybox", "place=" + second, 4),
                              ("toybox", "Return", 5), ("press", "2 a", 2)):
        (run.toybox if kind == "toybox" else run.press)(line)
        if not driver.wait(pause):
            return -1
    proof = marker("swapforce", "level", target)
    if not driver.wait(90, lambda text: proof.search(text) is not None):
        return -1
    reached = run.now()
    if driver.wait(60, lambda text: "countdown.pak" in text.lower()):
        return reached
    return -1


def snapshot(run):
    shots = sorted(glob.glob(os.path.join(run.dir, "shots", "s_*.ppm")),
                   key=lambda p: int(re.sub(r"\D", "", os.path.basename(p)) or 0))
    return shots[-1] if shots else ""


def figure_list(path):
    rows = []
    for line in open(path, errors="replace"):
        fields = line.rstrip("\n").split("\t")
        if len(fields) >= 3:
            rows.append((fields[0], fields[1].split(","), fields[2].split(",")))
    return rows


def main():
    args = sys.argv[1:]
    if len(args) < 5:
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        sys.exit(2)
    game, iso, rundir, route, arg = args[:5]
    play, fsr, seed, save_to = 45.0, False, "", ""
    rest = args[5:]
    while rest:
        flag = rest.pop(0)
        if flag == "--play":
            play = float(rest.pop(0))
        elif flag == "--fsr":
            fsr = True
        elif flag == "--seed":
            seed = rest.pop(0)
        elif flag == "--save-to":
            save_to = rest.pop(0)
    os.makedirs(rundir, exist_ok=True)
    run = Run(game, iso, rundir, route, arg, fsr, seed)
    driver = Driver(run)
    results = []
    extra = {}
    timeout = {"superchargers": 600 if not seed else 240, "imaginators": 300, "trapteam": 200}.get(game, 150)
    if route in ("level", "hero", "seed"):
        extra = level_extra(game, route, arg)
        if game == "superchargers" and seed:
            extra["SKY_LEVEL_FROM"] = "academy/academy"
    elif route == "credits":
        if arg.startswith("movies:"):
            extra = {"SKY_MOVIE_SWAP": arg[7:], "SKY_SKIP_MOVIES": "0"}
        else:
            extra = {"SKY_LEVEL": arg}
    elif route == "figures":
        if game in ("ssa", "swapforce", "superchargers"):  # the others start their first level by themselves
            extra = {"SKY_LEVEL": FIGURE_LEVEL[game]}
        if game == "superchargers" and seed:
            extra["SKY_LEVEL_FROM"] = "academy/academy"
        extra["SKY_PORTAL_LOG"] = "1"
    elif route == "cali":
        extra = {"SKY_LEVEL": "level_hub_2_stage_1"}
    elif route == "sfarena":
        target = arg.partition("/")[2]
        extra = {"SKY_PORTAL_PLACE": "new:9,new:14", "SKY_LEVEL": target,
                 "SKY_LEVEL_FROM": "/".join(target.split("/")[:2]) + "/"}
    elif route == "scrace":
        trophy, vehicle, moves = arg.split("/")[:3]
        boot = "" if moves.startswith("pack,") else f",new:{trophy}"
        extra = {"SKY_PORTAL_PLACE": f"new:3400,new:{vehicle}{boot}"}
        if moves.startswith("pack,"):  # the trophy's dump, put on the portal at the race map
            build = os.path.join(ROOT, "build-superchargers")
            tool = next((os.path.join(base, "ArkeyanFigureTest") for base, _, files in os.walk(build)
                         if "ArkeyanFigureTest" in files), "")
            subprocess.run([tool, "--make", os.path.join(rundir, "collection", f"pack_{trophy}.sky"), trophy],
                           capture_output=True, check=False, cwd=ROOT)
    elif route == "battle":
        extra = {"SKY_PORTAL_PLACE": "new:14,new:9"}
        if game == "ssa":
            extra["SKY_PORTAL_PLACE"] = "new_14.sky,new:9"
            subprocess.run(["cp", os.path.join(ROOT, "private", "figures", "new_14.sky"),
                            os.path.join(rundir, "collection")], check=False)
        target = (arg.split("/") + ["", ""])[2]
        if target:
            extra["SKY_LEVEL"] = target
            extra["SKY_LEVEL_FROM"] = {"ssa": "pvp_level_007", "giants": "pvp_level_200_two_towers"}[game]
    if game == "ssa" and route != "battle":
        src = os.path.join(ROOT, "private", "figures", "new_14.sky")
        if os.path.exists(src):
            subprocess.run(["cp", src, os.path.join(rundir, "collection")], check=False)
            extra.setdefault("SKY_PORTAL_PLACE", "new_14.sky")
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(143))
    run.launch(extra)
    try:
        if route == "cali":
            reached = cali(driver, int(arg), 60)
        elif route == "battle":
            reached = battle(driver, game, arg)
        elif route == "sfarena":
            reached = sfarena(driver, arg)
        elif route == "scrace":
            reached = scrace(driver, arg)
        elif route == "credits":
            reached = credits(driver, game, arg)
        elif route == "figures":
            level = trapteam_tutorial(iso) if game == "trapteam" else FIGURE_LEVEL[game]
            reached = reach(driver, game, "level", level, timeout, bool(seed))
        else:
            reached = reach(driver, game, route, arg, timeout, bool(seed))
        if reached >= 0 and route == "figures":
            results = figures(driver, game, arg, reached)
        elif reached >= 0 and route != "credits":
            settle = max(reached + 15, 64.0) if game == "ssa" and route != "battle" else run.now() + 8
            driver.play(settle, play)
            if route in ("battle", "sfarena"):
                driver.play(settle + 2, play, pad="2 ")
            if route == "scrace":  # the accelerator held through the race
                for t in range(int(settle), int(settle + play), 5):
                    driver.at(t, "rt:5")
            driver.wait(settle + play - run.now())
            if route == "seed" and save_to:
                driver.wait(10)
    finally:
        run.stop()
    elapsed = time.time() - run.start
    if route == "seed" and save_to:
        shutil.rmtree(save_to, ignore_errors=True)
        shutil.copytree(os.path.join(rundir, "save"), save_to)
        for name in ("pipelines.bin", "portal.txt"):
            try:
                os.remove(os.path.join(save_to, name))
            except OSError:
                pass
    shot = snapshot(run)
    if route == "figures":
        if not results:
            why = describe(driver, reached, run)
            results = [{"name": label, "hint": why[0], "reached": -1, "seconds": 0, "detail": why[1]}
                       for label, _, _ in figure_list(arg)]
        for r in results:
            r["shot"] = shot
            print(json.dumps(r))
        return
    hint, detail = describe(driver, reached, run)
    if route == "scrace":
        raced = re.findall(r"^\[level\] (actionpacks/ap[als]\d/ap[als]\d) starts", run.text, re.M)
        detail = (f"{raced[-1]} started; " if raced else "no race started; ") + detail
    if route == "cali":
        opened = re.findall(r"^\[level\] (challenge_level_\d+) starts", run.text, re.M)
        detail = (f"{opened[-1]} opened; " if opened else "no challenge level opened; ") + detail
    if route == "credits" and hint == "ok":
        hint, detail = credits_verdict(game, arg, run, reached)
    print(json.dumps({"name": arg, "hint": hint, "reached": round(reached, 1), "seconds": round(elapsed),
                      "detail": detail, "shot": shot}))


def describe(driver, reached, run):
    """hint and detail from how the run ended."""
    tail = " / ".join(line.strip()[:160] for line in run.text.strip().splitlines()[-3:])
    if driver.status == "exited":
        code = run.proc.returncode if run.proc else None
        return "crash", f"the game ended by itself at {run.now():.0f} s (exit {code}); last log lines: {tail}"
    if driver.status == "hang":
        return "hang", f"no frame for {driver.hang_after:.0f} s at {run.now():.0f} s; last log lines: {tail}"
    if reached < 0:
        return "not_reached", f"the entry's proof line never appeared ({run.now():.0f} s); last log lines: {tail}"
    return "ok", f"reached at {reached:.0f} s"


def trapteam_tutorial(iso):
    out = subprocess.run([sys.executable, "-I", os.path.join(ROOT, "tools", "regress", "trapteam.py"), "levels", iso],
                         capture_output=True, text=True).stdout.split()
    tutorials = [name for name in out if re.match(r"^level_.*_tutorial$", name)]
    return tutorials[0] if tutorials else "level_313_tutorial"


def credits(driver, game, arg):
    """The credits played to their end: SSA's two movies (each one's end is logged), a credits level (its next level,
    or the title, loads after the roll)."""
    run = driver.run
    if arg.startswith("movies:"):  # no press: a press could end a movie
        last = arg[7:].split(",")[-1].split(":")[0]
        done = lambda text: f"[movie] {last} ended after" in text
        if driver.wait(float(os.environ.get("COVERAGE_CREDITS_SECONDS", 760)), done):
            return run.now()
        return -1
    menus(driver, game, "level")
    proof = marker(game, "level", arg)
    if not driver.wait(200, lambda text: proof.search(text) is not None):
        return -1
    reached = run.now()
    driver.queue = [item for item in driver.queue if item[0] < reached]  # no presses during the roll
    after = re.compile(r"\\level\\(?!" + re.escape(arg) + r")[a-z0-9_]+\.arc -> 0x00000000", re.I)
    def rolled(text):
        position = proof.search(text)
        return position is not None and after.search(text, position.end()) is not None
    driver.wait(float(os.environ.get("COVERAGE_CREDITS_SECONDS", 1500)), rolled)
    return reached


def credits_verdict(game, arg, run, reached):
    if arg.startswith("movies:"):
        return "ok", "both movies played to their end (swapped in for the opening's: the story's end is not played)"
    proof = marker(game, "level", arg)
    position = proof.search(run.text)
    after = re.search(r"\\level\\([a-z0-9_]+)\.arc -> 0x00000000", run.text[position.end():] if position else "", re.I)
    if after and after.group(1).lower() != arg.lower():
        return "ok", (f"the roll ended: {after.group(1)} loaded after it ({run.now() - reached:.0f} s of credits; started "
                      "with SKY_LEVEL, the story's end is not played)")
    return "not_reached", f"the credits level ran {run.now() - reached:.0f} s; its end (the next level) never came"


def figures(driver, game, listing, reached):
    """Each figure of the list in turn: the portal cleared, its dumps placed, the game's read of its slot awaited, then
    COVERAGE_FIGURE_SECONDS (default 8) of play."""
    run = driver.run
    seconds = float(os.environ.get("COVERAGE_FIGURE_SECONDS", 8))
    out = []
    driver.wait(10)
    for label, files, types in figure_list(listing):
        t0 = run.now()
        start = len(run.text)
        driver.at(t0, "remove=all", "toybox")
        for f in files:
            driver.at(t0 + 1.5, "place=" + f, "toybox")
        driver.play(t0 + 2, seconds)
        alive = driver.wait(seconds + 2)
        new = run.text[start:]
        problems = []
        for i, (f, toy) in enumerate(zip(files, types)):
            placed = re.search(r"\[portal\] placed " + re.escape(f) + r" \(toy type " + toy + r"\) in slot (\d+)", new)
            if not placed:
                problems.append(f"{os.path.basename(f)} not placed")
                continue
            slot = placed.group(1)
            if not re.search(r"\[portal\] -> Q [0-9A-F]{2} [0-9A-F]" + format(int(slot), "X") + " ", new[placed.end():]):
                problems.append(f"slot {slot} never read by the game")
        faults = [line for line in new.splitlines() if FAULT.search(line)]
        if not alive or faults:
            why = driver.status or "fault"
            hint = "hang" if driver.status == "hang" else "crash"
            out.append({"name": label, "hint": hint, "reached": round(reached, 1), "seconds": round(run.now() - t0),
                        "detail": f"{why} while on the portal: " + " | ".join(faults[:2])[:400]})
            break
        hint = "not_reached" if problems else "ok"
        detail = "; ".join(problems) if problems else f"placed, read by the game, {seconds:.0f} s of play"
        out.append({"name": label, "hint": hint, "reached": round(reached, 1), "seconds": round(run.now() - t0),
                    "detail": detail})
    return out


if __name__ == "__main__":
    main()
