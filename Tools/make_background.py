#!/usr/bin/env python3
"""Regenerates Resources/background.svg.

Kept in the repo so the engraved legends stay in sync with Layout:: in
PluginEditor.cpp. Edit the constants below and re-run:

    python3 Tools/make_background.py
"""

import math, os

W, H = 640, 480

PANEL = "#2e332e"
INK = "#171a17"
CREAM = "#d8cfb8"
RUST = "#b24a28"
MOSS = "#7e8a7b"
GUIDE = "#e5195e"

FONT = "font-family=\"Helvetica Neue, Helvetica, Arial, sans-serif\""

# knob geometry must match Layout:: in PluginEditor.cpp
VINTAGE_C = (144, 214)
SIZE_C = (496, 214)
KNOB_R = 90
TICK_R = KNOB_R + 26
HEADER_Y = 86
DIVIDER_Y = 356
SMALL_C = [100, 210, 320, 430, 540]
START, END = 1.25 * math.pi, 2.75 * math.pi

out = []
a = out.append


def polar(c, r, t):
    ang = START + t * (END - START)
    return (c[0] + math.sin(ang) * r, c[1] - math.cos(ang) * r)


def label(x, y, s, size=9, fill=CREAM, anchor="middle", weight="normal", op=1.0):
    a(f'<text x="{x:.1f}" y="{y:.1f}" {FONT} font-size="{size}" font-weight="{weight}" '
      f'fill="{fill}" fill-opacity="{op}" text-anchor="{anchor}" '
      f'letter-spacing="1.2">{s}</text>')


a(f'<?xml version="1.0" encoding="UTF-8"?>')
a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">')
a('<!-- ==========================================================================')
a('     TannoyBox — PLACEHOLDER PANEL ARTWORK (640 x 480, 4:3)')
a('     Replace this file wholesale. Keep the viewBox at 640x480 and keep the')
a('     control cut-outs where they are, or update Layout:: in PluginEditor.cpp.')
a('     Delete <g id="placeholder-guides"> to lose the pink construction marks.')
a('     ========================================================================== -->')

# ---------------------------------------------------------------- panel
a('<defs>')
a('<linearGradient id="panelGrad" x1="0" y1="0" x2="0" y2="1">')
a(f'<stop offset="0" stop-color="#363c36"/><stop offset="1" stop-color="#252a25"/>')
a('</linearGradient>')
a('<linearGradient id="recess" x1="0" y1="0" x2="0" y2="1">')
a(f'<stop offset="0" stop-color="#0f110f"/><stop offset="1" stop-color="#1e221e"/>')
a('</linearGradient>')
a('</defs>')

a(f'<rect width="{W}" height="{H}" fill="url(#panelGrad)"/>')
a(f'<rect x="6" y="6" width="{W-12}" height="{H-12}" fill="none" stroke="{INK}" stroke-opacity="0.55" stroke-width="2"/>')
a(f'<rect x="10" y="10" width="{W-20}" height="{H-20}" fill="none" stroke="{MOSS}" stroke-opacity="0.20" stroke-width="1"/>')

# corner screws
for (sx, sy) in [(20, 20), (W - 20, 20), (20, H - 20), (W - 20, H - 20)]:
    a(f'<circle cx="{sx}" cy="{sy}" r="6" fill="{INK}" fill-opacity="0.5"/>')
    a(f'<circle cx="{sx}" cy="{sy}" r="4.5" fill="{MOSS}" fill-opacity="0.35"/>')
    a(f'<line x1="{sx-3}" y1="{sy-3}" x2="{sx+3}" y2="{sy+3}" stroke="{INK}" stroke-width="1.4"/>')

# ---------------------------------------------------------------- header
a(f'<!-- BRANDING ZONE: 0,0 640x{HEADER_Y}. logo.svg is drawn at 26,16 240x52 -->')
a(f'<rect x="0" y="0" width="{W}" height="{HEADER_Y}" fill="{INK}" fill-opacity="0.28"/>')
a(f'<line x1="16" y1="{HEADER_Y}" x2="{W-16}" y2="{HEADER_Y}" stroke="{MOSS}" stroke-opacity="0.35" stroke-width="1"/>')

# perforated grille, between the logo and the switches
a('<g id="grille" fill="' + INK + '" fill-opacity="0.5">')
for row in range(4):
    for col in range(11):
        cx = 300 + col * 11.6
        cy = 22 + row * 11.0 + (5.8 if row % 2 else 0)
        if cx < 424:
            a(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="2.6"/>')
a('</g>')
label(28, 80, "PUBLIC ADDRESS PROCESSOR", 8, MOSS, "start", op=0.9)

# switch legends. The switches themselves are drawn by the plugin into these
# rectangles — see Layout:: in PluginEditor.cpp.
for sx, sw, nm in [(436, 80, "MIC KEY"), (528, 80, "STATION CHIME")]:
    a(f'<rect x="{sx-5}" y="13" width="{sw+10}" height="48" rx="7" fill="{INK}" fill-opacity="0.18"/>')
    label(sx + sw / 2, 72, nm, 7, MOSS, "middle", op=0.95)

# ---------------------------------------------------------------- knob engraving
def knob_engraving(centre, name, caption, ticks):
    cx, cy = centre
    a(f'<!-- control cut-out: {name} -->')
    a(f'<circle cx="{cx}" cy="{cy}" r="{KNOB_R+16}" fill="{INK}" fill-opacity="0.22"/>')
    a(f'<circle cx="{cx}" cy="{cy}" r="{KNOB_R+16}" fill="none" stroke="{MOSS}" stroke-opacity="0.22" stroke-width="1"/>')

    n = len(ticks)
    for i, txt in enumerate(ticks):
        t = i / (n - 1)
        x, y = polar(centre, TICK_R, t)
        label(x, y + 3, txt, 9, CREAM, "middle", op=0.85)

    label(cx, cy + KNOB_R + 30, name, 15, CREAM, "middle", "bold")
    label(cx, cy + KNOB_R + 42, caption, 8, MOSS, "middle", op=0.95)


knob_engraving(VINTAGE_C, "VINTAGE", "ERA PROFILE",
               ["MODERN", "1970s", "1960s", "1950s"])

# Four index legends, not five. The middle of an odd number of them lands dead
# above the dial, which is exactly where the header rule is; the unit moved into
# the caption at the same time.
knob_engraving(SIZE_C, "SIZE", "METRES &#183; DISTANCE &amp; TAIL",
               ["5", "40", "80", "115"])

# ---------------------------------------------------------------- readout recess
a('<!-- READOUT WINDOW: nameplate.svg is drawn at 262,124 116x180 -->')
a(f'<rect x="256" y="118" width="128" height="192" rx="4" fill="{INK}" fill-opacity="0.35"/>')
a(f'<rect x="256" y="118" width="128" height="192" rx="4" fill="none" stroke="{MOSS}" stroke-opacity="0.25"/>')

# ---------------------------------------------------------------- lower deck
# Five 64px dials on 110px centres, 68..572. The second line of lettering under
# each one went when the fifth dial arrived: at this pitch there is no vertical
# room for it above the panel border at y=470, and the row reads better without.
a(f'<line x1="16" y1="{DIVIDER_Y}" x2="{W-16}" y2="{DIVIDER_Y}" stroke="{MOSS}" stroke-opacity="0.35" stroke-width="1"/>')
a(f'<rect x="0" y="{DIVIDER_Y}" width="{W}" height="{H-DIVIDER_Y}" fill="{INK}" fill-opacity="0.18"/>')

for cx, nm in zip(SMALL_C, ["DRIVE", "HOWL", "ROOM", "MIX", "OUTPUT"]):
    a(f'<circle cx="{cx}" cy="400" r="40" fill="{INK}" fill-opacity="0.20"/>')
    label(cx, 456, nm, 11, CREAM, "middle", "bold")

# ---------------------------------------------------------------- guides
a('<g id="placeholder-guides" stroke="' + GUIDE + '" fill="none" stroke-width="1" stroke-dasharray="5 4" opacity="0.75">')
GUIDES = [(26, 16, 240, 52), (262, 124, 116, 180),
          (54, 124, 180, 180), (406, 124, 180, 180),
          (436, 18, 80, 38), (528, 18, 80, 38)]
GUIDES += [(cx - 32, 368, 64, 64) for cx in SMALL_C]
for (gx, gy, gw, gh) in GUIDES:
    a(f'<rect x="{gx}" y="{gy}" width="{gw}" height="{gh}"/>')
a('</g>')
a(f'<text x="320" y="104" {FONT} font-size="8" fill="{GUIDE}" text-anchor="middle" '
  f'letter-spacing="1.5">PLACEHOLDER &#183; RESOURCES/BACKGROUND.SVG &#183; 640 &#215; 480</text>')

a('</svg>')

here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
path = os.path.join(here, "Resources", "background.svg")
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(out) + "\n")
print("wrote", path)
