#!/usr/bin/env python3
"""Generate an F1 racing car side-view pixel art as RGB565 PROGMEM header."""

from PIL import Image, ImageDraw

W, H = 100, 40
TRANSPARENT = (0, 0, 0)    # Black background matches COLOR_BG - no transparency needed

# Colors
RED = (60, 60, 70)        # Car body (dark charcoal livery)
DARK_RED = (35, 35, 42)   # Body shadow/detail
BLACK = (0, 0, 0)         # Tires, outlines (same as background — tire outline handled by DARK_GRAY)
DARK_GRAY = (40, 40, 40)  # Tire detail
MID_GRAY = (80, 80, 80)   # Tire sidewall
LIGHT_GRAY = (180, 180, 180)  # Wheel rim
WHITE = (255, 255, 255)   # Number, highlights
YELLOW = (255, 200, 0)    # Nose tip accent
ACCENT_RED = (220, 0, 0)  # Accent stripes on black body

img = Image.new("RGB", (W, H), TRANSPARENT)
d = ImageDraw.Draw(img)

# --- Rear wing (tall, at rear of car) ---
# Vertical pillar
d.rectangle([83, 6, 85, 20], fill=DARK_RED)
# Top element
d.rectangle([81, 4, 92, 7], fill=RED)
# Bottom element
d.rectangle([82, 9, 91, 11], fill=RED)

# --- Main body / chassis ---
# Floor plank (flat bottom)
d.rectangle([10, 26, 88, 28], fill=DARK_RED)

# Main body polygon - side profile
body_poly = [
    (8, 25),    # front wing junction
    (8, 22),    # nose lower
    (5, 21),    # nose tip
    (5, 19),    # nose top
    (12, 18),   # nose rising
    (20, 16),   # front chassis
    (30, 15),   # ahead of cockpit
    (38, 15),   # cockpit front
    (40, 12),   # windscreen / halo front
    (42, 11),   # halo top
    (50, 11),   # halo top
    (52, 12),   # halo rear
    (54, 14),   # headrest
    (56, 14),   # engine cover start
    (60, 13),   # air intake top
    (65, 13),   # engine cover
    (75, 15),   # engine cover taper
    (83, 17),   # rear body
    (88, 19),   # rear end
    (88, 26),   # rear bottom
    (85, 27),   # rear floor
    (10, 27),   # front floor
]
d.polygon(body_poly, fill=RED)

# Cockpit opening (dark)
cockpit = [
    (41, 13), (43, 12), (49, 12), (51, 13),
    (52, 15), (40, 15)
]
d.polygon(cockpit, fill=BLACK)

# Driver helmet
d.ellipse([44, 12, 49, 16], fill=(0, 100, 200))  # Blue helmet

# Air intake above driver
d.rectangle([55, 12, 59, 15], fill=DARK_RED)

# Body stripe / detail line
d.line([(15, 22), (85, 22)], fill=ACCENT_RED, width=1)

# Number area
d.rectangle([25, 18, 35, 24], fill=WHITE)
d.text((27, 17), "1", fill=ACCENT_RED)

# --- Front wing ---
d.rectangle([3, 26, 18, 27], fill=RED)       # Main element
d.rectangle([2, 28, 16, 29], fill=RED)        # Lower element
d.rectangle([2, 25, 5, 26], fill=RED)         # Endplate
d.rectangle([15, 24, 17, 29], fill=DARK_RED)  # Endplate inner

# Nose tip accent
d.rectangle([3, 19, 6, 21], fill=YELLOW)

# --- Rear diffuser ---
d.rectangle([86, 24, 90, 28], fill=DARK_RED)

# --- Front tire ---
tire_cx, tire_cy, tire_rx, tire_ry = 18, 30, 6, 6
d.ellipse([tire_cx - tire_rx, tire_cy - tire_ry,
           tire_cx + tire_rx, tire_cy + tire_ry], fill=BLACK)
d.ellipse([tire_cx - tire_rx + 1, tire_cy - tire_ry + 1,
           tire_cx + tire_rx - 1, tire_cy + tire_ry - 1], fill=DARK_GRAY)
d.ellipse([tire_cx - 3, tire_cy - 3,
           tire_cx + 3, tire_cy + 3], fill=MID_GRAY)
d.ellipse([tire_cx - 2, tire_cy - 2,
           tire_cx + 2, tire_cy + 2], fill=LIGHT_GRAY)

# --- Rear tire ---
tire_cx2, tire_cy2 = 80, 30
d.ellipse([tire_cx2 - tire_rx, tire_cy2 - tire_ry,
           tire_cx2 + tire_rx, tire_cy2 + tire_ry], fill=BLACK)
d.ellipse([tire_cx2 - tire_rx + 1, tire_cy2 - tire_ry + 1,
           tire_cx2 + tire_rx - 1, tire_cy2 + tire_ry - 1], fill=DARK_GRAY)
d.ellipse([tire_cx2 - 3, tire_cy2 - 3,
           tire_cx2 + 3, tire_cy2 + 3], fill=MID_GRAY)
d.ellipse([tire_cx2 - 2, tire_cy2 - 2,
           tire_cx2 + 2, tire_cy2 + 2], fill=LIGHT_GRAY)

# Flip horizontally so car faces right (nose on right side)
img = img.transpose(Image.FLIP_LEFT_RIGHT)

# --- Convert to RGB565 and write header ---
def rgb888_to_rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

# Ensure no pixel accidentally matches transparent green exactly
# (shift green channel slightly if needed)
pixels = []
for y in range(H):
    for x in range(W):
        r, g, b = img.getpixel((x, y))
        val = rgb888_to_rgb565(r, g, b)
        # Background is 0x0000 (black) - car pixels that happen to be pure black
        # get nudged to 0x0841 (very dark, imperceptible on screen) so they render
        if val == 0x0000 and (r, g, b) != TRANSPARENT:
            val = 0x0841  # Near-black, visually identical
        pixels.append(val)

# Save preview PNG
img_preview = img.copy()
preview_path = "/Users/ant/PlatformIO/Projects/F1_CYD_Notifications/tools/f1_car_preview.png"
img_preview.save(preview_path)
# Also save a 4x scaled version for easier viewing
img_big = img.resize((W * 4, H * 4), Image.NEAREST)
img_big.save(preview_path.replace(".png", "_4x.png"))

# Write header file
header_path = "/Users/ant/PlatformIO/Projects/F1_CYD_Notifications/include/f1_race_car.h"
with open(header_path, "w") as f:
    f.write("// F1 Racing Car - RGB565 format (side-view pixel art)\n")
    f.write(f"// Size: {W}x{H} pixels\n")
    f.write('// Background is 0x0000 (black) = COLOR_BG - draw with pushImage, no transparency param needed\n')
    f.write('// Generated by tools/generate_f1_car.py\n')
    f.write("\n#pragma once\n#include <Arduino.h>\n\n")
    f.write(f"#define F1_CAR_WIDTH  {W}\n")
    f.write(f"#define F1_CAR_HEIGHT {H}\n\n")
    f.write(f"const uint16_t f1_race_car[{W * H}] PROGMEM = {{\n")

    for i, val in enumerate(pixels):
        if i % 16 == 0:
            f.write("  ")
        f.write(f"0x{val:04X}")
        if i < len(pixels) - 1:
            f.write(",")
            if i % 16 == 15:
                f.write("\n")
            else:
                f.write(" ")
    f.write("\n};\n")

print(f"Generated {header_path}")
print(f"Preview saved to {preview_path}")
print(f"Array size: {len(pixels)} pixels ({W}x{H})")
