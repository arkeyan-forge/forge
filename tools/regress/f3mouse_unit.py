#!/usr/bin/env python3
"""Compile host UI unit fixtures with the configured build's flags; no game or device is started.

Usage: python3 -I tools/regress/f3mouse_unit.py [build-directory]
Only functions reachable from each fixture are linked, allowing recording stubs for host services.
"""
import json
import pathlib
import shlex
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
build = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / 'build'
entries = json.loads((build / 'compile_commands.json').read_text())


def run(command, **kwargs):
    result = subprocess.run(command, capture_output=True, text=True, **kwargs)
    if result.returncode:
        sys.stderr.write(result.stdout + result.stderr)
        raise SystemExit(1)


def compile_source(original, source, output):
    entry = next(e for e in entries if pathlib.Path(e['file']) == root / original)
    command = shlex.split(entry['command'])
    command[command.index('-o') + 1] = str(output)
    command[command.index('-c') + 1] = str(root / source)
    run(command + ['-ffunction-sections', '-fdata-sections'], cwd=entry['directory'])
    return command[0]


with tempfile.TemporaryDirectory(prefix='f3mouse-unit-') as temporary:
    scratch = pathlib.Path(temporary)
    cases = [
        ('runtime/host/gamemenu.cpp', 'runtime/host/gamemenu_input_test.cpp', True),
        ('runtime/host/toybox/draw.cpp', 'runtime/host/toybox/mouse_test.cpp', False),
    ]
    for index, (original, fixture, needs_original) in enumerate(cases):
        obj = scratch / f'fixture{index}.o'
        compiler = compile_source(original, fixture, obj)
        objects = [str(obj)]
        if needs_original:
            obj = scratch / f'original{index}.o'
            compile_source(original, original, obj)
            objects.append(str(obj))
        binary = scratch / f'test{index}'
        strip = '-Wl,-dead_strip' if sys.platform == 'darwin' else '-Wl,--gc-sections'
        run([compiler, *objects, strip, '-o', str(binary)])
        run([str(binary)])
print('PASS: startup Toy Box keys and 15 menu/Settings/rail/card/Commands hover, click and hover focus checks')
