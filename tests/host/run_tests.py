#!/usr/bin/env python3
"""Compile actual display headers against deterministic host hardware stubs."""

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--frames", type=Path, help="Write raw 320x56 RGB animation frames")
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="f1-display-tests-") as temp:
    build = Path(temp)
    # Header-defined modules use quoted sibling includes. Stage unchanged source
    # headers alongside stubs so the host never pulls in ESP32 network drivers.
    for name in ("config.h", "types.h", "time_utils.h", "timezone_ntp_options.h",
                 "display_renderer.h", "display_states.h", "race_animation.h",
                 "f1_logo.h", "f1_race_car.h"):
        shutil.copy2(ROOT / "include" / name, build / name)
    for stub in (ROOT / "tests/host/stubs").glob("*.h"):
        shutil.copy2(stub, build / stub.name)
    executable = build / "test_display"
    subprocess.run([
        "c++", "-std=c++11", "-Wall", "-Wextra", "-Wno-unused-function",
        "-Wno-unused-variable", "-Wno-sign-compare", "-fsanitize=address,undefined", "-g",
        "-I", str(build), str(ROOT / "tests/host/test_display.cpp"),
        "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)] + ([str(args.frames.resolve())] if args.frames else []), check=True)
