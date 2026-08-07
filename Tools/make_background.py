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
KNOB_R = 98
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
a('<!-- BRANDING ZONE: 0,0 640x92. logo.svg is drawn at 26,20 240x52 -->')
a(f'<rect x="0" y="0" width="{W}" height="92" fill="{INK}" fill-opacity="0.28"/>')
a(f'<line x1="16" y1="92" x2="{W-16}" y2="92" stroke="{MOSS}" stroke-opacity="0.35" stroke-width="1"/>')

# perforated grille to the right of the logo
a('<g id="grille" fill="' + INK + '" fill-opacity="0.5">')
for row in range(5):
    for col in range(27):
        cx = 300 + col * 11.6
        cy = 24 + row * 11.0 + (5.8 if row % 2 else 0)
        if cx < 616:
            a(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="2.6"/>')
a('</g>')
label(W - 26, 82, "PUBLIC ADDRESS PROCESSOR", 8, MOSS, "end", op=0.9)

# ---------------------------------------------------------------- knob engraving
def knob_engraving(centre, name, caption, ticks):
    cx, cy = centre
    a(f'<!-- control cut-out: {name} -->')
    a(f'<circle cx="{cx}" cy="{cy}" r="{KNOB_R+16}" fill="{INK}" fill-opacity="0.22"/>')
    a(f'<circle cx="{cx}" cy="{cy}" r="{KNOB_R+16}" fill="none" stroke="{MOSS}" stroke-opacity="0.22" stroke-width="1"/>')

    n = len(ticks)
    for i, txt in enumerate(ticks):
        t = i / (n - 1)
        x, y = polar(centre, KNOB_R + 26, t)
        label(x, y + 3, txt, 9, CREAM, "middle", op=0.85)

    label(cx, cy + KNOB_R + 44, name, 15, CREAM, "middle", "bold")
    label(cx, cy + KNOB_R + 60, caption, 8, MOSS, "middle", op=0.95)


knob_engraving(VINTAGE_C, "VINTAGE", "ERA PROFILE",
               ["MODERN", "1970s", "1960s", "1950s"])
knob_engraving(SIZE_C, "SIZE", "DISTANCE &#183; TAIL",
               ["5", "30", "60", "90", "115"])
label(SIZE_C[0], SIZE_C[1] - KNOB_R - 40, "METRES", 8, MOSS, "middle", op=0.8)

# ---------------------------------------------------------------- readout recess
a('<!-- READOUT WINDOW: nameplate.svg is drawn at 262,120 116x180 -->')
a(f'<rect x="256" y="114" width="128" height="192" rx="4" fill="{INK}" fill-opacity="0.35"/>')
a(f'<rect x="256" y="114" width="128" height="192" rx="4" fill="none" stroke="{MOSS}" stroke-opacity="0.25"/>')

# ---------------------------------------------------------------- lower deck
a(f'<line x1="16" y1="356" x2="{W-16}" y2="356" stroke="{MOSS}" stroke-opacity="0.35" stroke-width="1"/>')
a(f'<rect x="0" y="356" width="{W}" height="{H-356}" fill="{INK}" fill-opacity="0.18"/>')

for cx, nm, cap in [(96, "DRIVE", "INPUT TRIM"), (245, "HOWL", "FEEDBACK"),
                    (395, "MIX", "DRY / PA"), (544, "OUTPUT", "MAKE-UP")]:
    a(f'<circle cx="{cx}" cy="404" r="44" fill="{INK}" fill-opacity="0.20"/>')
    label(cx, 460, nm, 11, CREAM, "middle", "bold")
    label(cx, 472, cap, 7, MOSS, "middle", op=0.9)

# ---------------------------------------------------------------- guides
a('<g id="placeholder-guides" stroke="' + GUIDE + '" fill="none" stroke-width="1" stroke-dasharray="5 4" opacity="0.75">')
for (gx, gy, gw, gh) in [(26, 20, 240, 52), (262, 120, 116, 180),
                         (46, 116, 196, 196), (398, 116, 196, 196),
                         (64, 372, 64, 64), (213, 372, 64, 64),
                         (363, 372, 64, 64), (512, 372, 64, 64)]:
    a(f'<rect x="{gx}" y="{gy}" width="{gw}" height="{gh}"/>')
a('</g>')
a(f'<text x="320" y="106" {FONT} font-size="8" fill="{GUIDE}" text-anchor="middle" '
  f'letter-spacing="1.5">PLACEHOLDER &#183; RESOURCES/BACKGROUND.SVG &#183; 640 &#215; 480</text>')

a('</svg>')

here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
path = os.path.join(here, "Resources", "background.svg")
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(out) + "\n")
print("wrote", path)
