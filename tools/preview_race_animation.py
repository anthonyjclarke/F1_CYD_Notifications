#!/usr/bin/env python3
"""Run host checks and produce a browser preview using actual animation pixels."""

import argparse
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[1]


def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))


def png(frames, width, height):
    animated = len(frames) > 1
    result = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    if animated:
        result += chunk(b"acTL", struct.pack(">II", len(frames), 1))
    sequence = 0
    for index, frame in enumerate(frames):
        if animated:
            result += chunk(b"fcTL", struct.pack(">IIIIIHHBB", sequence, width, height, 0, 0, 40, 1000, 0, 0))
            sequence += 1
        scanlines = b"".join(b"\0" + frame[y * width * 3:(y + 1) * width * 3] for y in range(height))
        compressed = zlib.compress(scanlines)
        if index == 0:
            result += chunk(b"IDAT", compressed)
        else:
            result += chunk(b"fdAT", struct.pack(">I", sequence) + compressed)
            sequence += 1
    return result + chunk(b"IEND", b"")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path(tempfile.gettempdir()) / "f1-race-preview")
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="f1-animation-frames-") as temp:
        raw = Path(temp) / "frames.rgb"
        subprocess.run([sys.executable, str(ROOT / "tests/host/run_tests.py"), "--frames", str(raw)], check=True)
        pixels = raw.read_bytes()
    size = 320 * 56 * 3
    assert len(pixels) % size == 0
    frames = [pixels[i:i + size] for i in range(0, len(pixels), size)]
    # The test exports t=40..4160 ms; prepend its identical blank t=0 frame.
    frames.insert(0, frames[0])
    (output / "animation.png").write_bytes(png(frames, 320, 56))
    samples = [frames[i] for i in (25, 50, 65, 80)]
    (output / "contact-sheet.png").write_bytes(png([b"".join(samples)], 320, 56 * len(samples)))
    (output / "index.html").write_text('''<!doctype html>
<html lang="en"><meta charset="utf-8"><title>F1 live-session animation preview</title>
<style>
body{background:#202124;color:#eee;font:16px system-ui;margin:40px auto;max-width:680px}
.screen{position:relative;width:320px;height:240px;background:black;margin:24px 0;overflow:hidden}
.header{height:24px;background:#f80000;text-align:center;font:bold 15px/24px Arial}
.name{position:absolute;top:35px;width:100%;text-align:center;font:23px Arial}
.location{position:absolute;top:57px;width:100%;text-align:center;font:16px Arial;color:#ccc}
img{position:absolute;top:94px;left:0;width:320px;height:56px;image-rendering:pixelated}
.on{position:absolute;top:153px;width:100%;text-align:center;color:red;font:bold 48px monospace}
.now{position:absolute;top:201px;width:100%;text-align:center;font:17px Arial}
button{padding:8px 16px;cursor:pointer}p{line-height:1.5;color:#ccc}
</style>
<h1>Live-session car animation</h1>
<p>The moving strip uses pixels exported from the firmware renderer. Text uses browser fonts;
check the physical TFT for final typography, colors and smoothness.</p>
<div class="screen" id="screen"></div><button id="replay">Replay</button>
<p>One 4.2-second pass, then event details. The firmware repeats the animation on the next
live-countdown visit. Timing and state transitions are covered by the host tests.</p>
<script>
let timer;
function play(){clearTimeout(timer);document.getElementById('screen').innerHTML=
'<div class="header">SESSION ON NOW</div><div class="name">Test Grand Prix</div>'+
'<div class="location">Test circuit</div><img alt="Car and smoke" src="animation.png?'+Date.now()+'">'+
'<div class="on">ON</div><div class="now">NOW!</div>';
timer=setTimeout(()=>document.getElementById('screen').innerHTML=
'<div class="header">RACE DETAILS</div><div class="name">Test Grand Prix</div>'+
'<div class="location">Next screen</div>',4200)}
document.getElementById('replay').onclick=play;play();
</script></html>''', encoding="utf-8")
    print(f"Preview: {output / 'index.html'}")


if __name__ == "__main__":
    main()
