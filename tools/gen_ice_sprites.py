#!/usr/bin/env python3
"""Bakes 3IS's two sprites (the ice-wizard mascot, the 3IS crystal logo) into firmware/src/flora/ice_sprites.h.
Pixels are palette indices (0xFF = transparent); the slots are flora_pal.h's FL_I_* (52..55), FL_L0.. (56..59) and
FL_S0.. (60..63) plus a few of the ramps. Run: python3 tools/gen_ice_sprites.py"""
import os
from PIL import Image, ImageDraw

T = 255
OUT, ICE, ICE2, ICE3 = 52, 53, 54, 55
DARK, SKY1, SKY2 = 56, 57, 58
HAT, HAT2, HAT3, CRY = 60, 61, 62, 63
WHITE = 4                                  # FL_INK
GOLD = 8 + 0 * 4 + 0                       # FL_C(FIRE, 0)
PINK = 8 + 7 * 4 + 0                       # FL_C(ROSE, 0)
WOOD, WOOD2 = 8 + 8 * 4 + 2, 8 + 8 * 4 + 3  # FL_C(SAND, 2), FL_C(SAND, 3)


def mascot(d, cx, cy, blink=False):
    for fx in (cx - 7, cx + 7):
        d.ellipse([fx - 4, cy + 14, fx + 4, cy + 19], fill=ICE3, outline=OUT)
    sx = cx + 22
    d.line([sx, cy + 16, sx, cy - 16], fill=WOOD, width=1)
    d.line([sx + 1, cy + 16, sx + 1, cy - 16], fill=WOOD2, width=1)
    d.polygon([(sx, cy - 30), (sx + 4, cy - 23), (sx, cy - 14), (sx - 4, cy - 23)], fill=CRY, outline=OUT)
    d.polygon([(sx, cy - 30), (sx - 4, cy - 23), (sx, cy - 14)], fill=ICE)
    d.line([sx, cy - 27, sx, cy - 18], fill=WHITE)
    d.ellipse([cx - 17, cy - 14, cx + 17, cy + 18], fill=OUT)
    d.ellipse([cx - 16, cy - 13, cx + 16, cy + 17], fill=ICE)
    d.pieslice([cx - 16, cy - 13, cx + 16, cy + 17], 20, 150, fill=ICE2)
    d.pieslice([cx - 16, cy - 13, cx + 16, cy + 17], 50, 120, fill=ICE3)
    d.ellipse([cx - 11, cy - 10, cx - 5, cy - 5], fill=WHITE)
    d.ellipse([cx - 23, cy + 1, cx - 14, cy + 9], fill=ICE2, outline=OUT)
    d.ellipse([cx + 14, cy - 2, cx + 24, cy + 7], fill=ICE2, outline=OUT)
    d.rectangle([sx - 1, cy + 1, sx + 2, cy + 4], fill=ICE)
    for ex in (cx - 7, cx + 5):
        if blink:                                   # eyes shut: a line
            d.rectangle([ex - 1, cy + 5, ex + 3, cy + 5], fill=DARK)
            continue
        d.rectangle([ex, cy + 1, ex + 2, cy + 6], fill=DARK)
        d.point((ex, cy + 1), fill=WHITE)
    d.rectangle([cx - 2, cy + 8, cx + 1, cy + 8], fill=DARK)
    d.point((cx - 3, cy + 7), fill=DARK)
    d.point((cx + 2, cy + 7), fill=DARK)
    for px in (cx - 11, cx + 9):
        d.rectangle([px, cy + 7, px + 2, cy + 8], fill=PINK)
    d.ellipse([cx - 18, cy - 17, cx + 18, cy - 8], fill=OUT)
    d.ellipse([cx - 17, cy - 16, cx + 17, cy - 9], fill=HAT)
    d.arc([cx - 17, cy - 16, cx + 17, cy - 9], 180, 360, fill=ICE)
    d.polygon([(cx - 11, cy - 14), (cx + 11, cy - 14), (cx + 9, cy - 26), (cx + 11, cy - 36), (cx + 19, cy - 41),
               (cx + 14, cy - 30), (cx - 3, cy - 27)], fill=OUT)
    d.polygon([(cx - 9, cy - 15), (cx + 9, cy - 15), (cx + 7, cy - 26), (cx + 9, cy - 35), (cx + 16, cy - 39),
               (cx + 12, cy - 30), (cx - 1, cy - 27)], fill=HAT)
    d.polygon([(cx - 9, cy - 15), (cx - 3, cy - 15), (cx - 1, cy - 27), (cx + 7, cy - 26), (cx + 5, cy - 30),
               (cx - 1, cy - 27)], fill=HAT3)
    d.line([cx + 4, cy - 15, cx + 6, cy - 26], fill=HAT2)
    d.rectangle([cx - 9, cy - 18, cx + 9, cy - 15], fill=ICE2)
    d.polygon([(cx, cy - 21), (cx + 3, cy - 17), (cx, cy - 13), (cx - 3, cy - 17)], fill=CRY, outline=OUT)
    d.point((cx + 16, cy - 40), fill=WHITE)
    d.point((cx + 19, cy - 41), fill=WHITE)
    for sp in ((cx - 5, cy - 24), (cx + 2, cy - 29)):
        d.point(sp, fill=GOLD)


F3 = [".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."]
FS = [".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###."]


def glyph(d, g, x, y, c, cell):
    for j, row in enumerate(g):
        for i, ch in enumerate(row):
            if ch == '#':
                d.rectangle([x + i * cell, y + j * cell, x + i * cell + cell - 1, y + j * cell + cell - 1], fill=c)


def crystal(d, cx, top, h, w):
    mid = top + h // 2
    hexa = [(cx, top), (cx + w // 2, top + h // 5), (cx + w // 2, top + h - h // 5), (cx, top + h),
            (cx - w // 2, top + h - h // 5), (cx - w // 2, top + h // 5)]
    d.polygon([(x + 1, y + 1) for x, y in hexa], fill=SKY2)
    d.polygon(hexa, fill=ICE3, outline=OUT)
    d.polygon([(cx, top), (cx - w // 2, top + h // 5), (cx - w // 2, top + h - h // 5), (cx, top + h), (cx, mid)], fill=ICE)
    d.polygon([(cx, top), (cx + w // 2, top + h // 5), (cx + w // 2, top + h - h // 5), (cx, top + h)], fill=ICE3)
    d.polygon([(cx, top), (cx + w // 2, top + h // 5), (cx, top + h // 3)], fill=ICE2)
    d.line([cx, top + 2, cx, top + h - 2], fill=ICE2)
    d.line([cx - w // 4, top + h // 4, cx - w // 4, top + h // 2], fill=WHITE)
    for sx, sy in ((cx - w // 2 - 3, top + 4), (cx + w // 2 + 3, top + h - 8)):
        d.line([sx - 2, sy, sx + 2, sy], fill=WHITE)
        d.line([sx, sy - 2, sx, sy + 2], fill=WHITE)


def logo():
    cell, cw, gap = 5, 16, 6
    gw = 5 * cell
    total = gw + gap + cw + gap + gw
    W, H = total + 10, 7 * cell + 12 + 10
    im = Image.new('P', (W, H), T)
    d = ImageDraw.Draw(im)
    x0, y0 = 5, 11
    glyph(d, F3, x0 + 2, y0 + 2, SKY2, cell)
    glyph(d, F3, x0, y0, ICE, cell)
    crystal(d, x0 + gw + gap + cw // 2, y0 - 6, 7 * cell + 12, cw)
    xs = x0 + gw + gap + cw + gap
    glyph(d, FS, xs + 2, y0 + 2, SKY2, cell)
    glyph(d, FS, xs, y0, ICE, cell)
    d.rectangle([x0, y0 + 7 * cell + 8, x0 + total - 1, y0 + 7 * cell + 8], fill=ICE3)
    return im


def emit(name, im, out):
    w, h = im.size
    px = list(im.getdata())
    out.append(f"#define {name}_W {w}\n#define {name}_H {h}\n")
    out.append(f"static const uint8_t {name.lower()}[{w * h}] = {{\n")
    for y in range(h):
        out.append("    " + ",".join(str(p) for p in px[y * w:(y + 1) * w]) + ",\n")
    out.append("};\n")


m = Image.new('P', (64, 64), T)
mascot(ImageDraw.Draw(m), 28, 44)
mb = Image.new('P', (64, 64), T)
mascot(ImageDraw.Draw(mb), 28, 44, blink=True)
# trim to the drawing's box; the anchor is the ball's centre
box = m.point(lambda v: 0 if v == T else 255).getbbox()
m = m.crop(box)
mb = mb.crop(box)
cxa, cya = 28 - box[0], 44 - box[1]
out = ["/* SPDX-License-Identifier: GPL-3.0-only */\n/* generated by tools/gen_ice_sprites.py: do not edit. 0xFF = transparent */\n"]
out.append(f"#define ICE_MASCOT_AX {cxa}\n#define ICE_MASCOT_AY {cya}\n")
emit("ICE_MASCOT", m, out)
emit("ICE_MASCOT_B", mb, out)
emit("ICE_LOGO", logo(), out)
path = os.path.join(os.path.dirname(__file__), "..", "firmware", "src", "flora", "ice_sprites.h")
open(path, "w").write("".join(out))
print("sprites", m.size, logo().size)
