#!/usr/bin/env python3
"""Regenerates the placeholder knob artwork.

    python3 Tools/make_knobs.py

knob_large.svg / knob_small.svg are drawn centred and never rotate.
knob_pointer.svg is rotated about the centre of its own frame, so whatever you
put in it must point STRAIGHT UP at rest.
"""

import math, os

INK = "#171a17"
CREAM = "#d8cfb8"
RUST = "#b24a28"
MOSS = "#7e8a7b"
GUIDE = "#e5195e"

here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def write(name, body, size):
    doc = [f'<?xml version="1.0" encoding="UTF-8"?>',
           f'<svg xmlns="http://www.w3.org/2000/svg" width="{size}" height="{size}" '
           f'viewBox="0 0 {size} {size}">',
           f'<!-- Yellowcoat placeholder: {name} ({size}x{size}) -->']
    doc += body
    doc.append('</svg>')
    path = os.path.join(here, "Resources", name)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(doc) + "\n")
    print("wrote", path)


def face(size, flutes, guides):
    c = size / 2.0
    r = size * 0.478
    b = []
    b.append('<defs>')
    b.append('<linearGradient id="skirt" x1="0" y1="0" x2="0" y2="1">'
             '<stop offset="0" stop-color="#3e443c"/><stop offset="1" stop-color="#171b16"/>'
             '</linearGradient>')
    b.append('<linearGradient id="cap" x1="0" y1="0" x2="0.3" y2="1">'
             '<stop offset="0" stop-color="#262b24"/><stop offset="1" stop-color="#101210"/>'
             '</linearGradient>')
    b.append('</defs>')

    b.append(f'<circle cx="{c}" cy="{c}" r="{r:.1f}" fill="url(#skirt)"/>')
    b.append(f'<circle cx="{c}" cy="{c}" r="{r:.1f}" fill="none" stroke="{CREAM}" '
             f'stroke-opacity="0.22" stroke-width="{size*0.008:.2f}"/>')

    b.append(f'<g stroke="{INK}" stroke-opacity="0.6" stroke-width="{size*0.016:.2f}" stroke-linecap="round">')
    for i in range(flutes):
        a = 2 * math.pi * i / flutes
        x1, y1 = c + math.sin(a) * r * 0.84, c - math.cos(a) * r * 0.84
        x2, y2 = c + math.sin(a) * r * 0.98, c - math.cos(a) * r * 0.98
        b.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}"/>')
    b.append('</g>')

    b.append(f'<circle cx="{c}" cy="{c}" r="{r*0.70:.1f}" fill="url(#cap)"/>')
    b.append(f'<circle cx="{c}" cy="{c}" r="{r*0.70:.1f}" fill="none" stroke="{MOSS}" '
             f'stroke-opacity="0.20" stroke-width="1"/>')
    b.append(f'<circle cx="{c}" cy="{c}" r="{r*0.10:.1f}" fill="{RUST}" fill-opacity="0.85"/>')

    if guides:
        b.append(f'<g id="placeholder-guides">')
        b.append(f'<circle cx="{c}" cy="{c}" r="{r:.1f}" fill="none" stroke="{GUIDE}" '
                 f'stroke-width="1" stroke-dasharray="5 4" opacity="0.7"/>')
        b.append(f'<text x="{c}" y="{c + r*0.45:.1f}" font-family="Helvetica Neue, Arial, sans-serif" '
                 f'font-size="9" fill="{GUIDE}" text-anchor="middle" letter-spacing="1.2">REPLACE</text>')
        b.append('</g>')
    return b


def pointer(size):
    c = size / 2.0
    w = size * 0.036
    b = []
    b.append(f'<!-- must point UP at rest; it is rotated about ({c}, {c}) -->')
    b.append(f'<rect x="{c - w/2:.1f}" y="{size*0.075:.1f}" width="{w:.1f}" '
             f'height="{size*0.20:.1f}" rx="{w/2:.1f}" fill="{CREAM}"/>')
    b.append(f'<circle cx="{c}" cy="{size*0.315:.1f}" r="{size*0.022:.1f}" fill="{RUST}"/>')
    return b


# 180, not 196: the large dials shrank when the fifth small one arrived. These
# are scaled to fit whatever rectangle they land in, so the number is really
# just the authoring size — but it should match Layout:: or it misleads.
write("knob_large.svg", face(180, 18, True), 180)
write("knob_small.svg", face(64, 12, False), 64)
write("knob_pointer.svg", pointer(180), 180)
