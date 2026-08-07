#!/usr/bin/env python3
"""Bakes a knob filmstrip from a single square frame.

    pip install pillow
    python3 Tools/make_filmstrip.py artwork/knob.png Resources/knob_large.png
    python3 Tools/make_filmstrip.py artwork/knob.png Resources/knob_small.png --size 128

The input should be one square PNG with a transparent background, the pointer
or index mark pointing STRAIGHT UP, and the knob centred in the frame. The
output is that frame rotated through the control's 270 degree sweep, stacked
vertically: frame 0 fully anticlockwise, the last frame fully clockwise.

The plugin infers the frame count from height / width, so nothing else needs
telling. 128 frames is inaudible-to-the-eye smooth and costs about 4 MB at
200 px square; drop to 64 if binary size matters more than smoothness.

Two things to watch:

  Rotate the WHOLE knob only if the lighting should rotate with it. Most real
  knobs have a fixed light source, so a nicer result comes from rendering the
  frames in your 3D or vector tool with the light held still. This script is
  the quick version, not the best version.

  Anti-aliasing at the frame edges is why the source needs a few pixels of
  transparent margin. Without it the rotated corners get clipped.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("This needs Pillow:  pip install pillow")

SWEEP = 270.0   # matches setRotaryParameters() in PluginEditor.cpp


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", help="square PNG, pointer pointing up")
    ap.add_argument("dest", help="output filmstrip PNG")
    ap.add_argument("--frames", type=int, default=128)
    ap.add_argument("--size", type=int, default=0,
                    help="output frame size in px; defaults to the source size")
    args = ap.parse_args()

    src = Image.open(args.source).convert("RGBA")

    if src.width != src.height:
        print(f"warning: source is {src.width}x{src.height}, not square — "
              f"it will be squashed to fit", file=sys.stderr)

    size = args.size or src.width
    if src.width != size:
        src = src.resize((size, size), Image.LANCZOS)

    # Supersample so the rotation does not chew the edges.
    work = src.resize((size * 4, size * 4), Image.LANCZOS)

    strip = Image.new("RGBA", (size, size * args.frames), (0, 0, 0, 0))

    for i in range(args.frames):
        t = i / (args.frames - 1) if args.frames > 1 else 0.5
        angle = -(-SWEEP / 2 + t * SWEEP)      # PIL rotates anticlockwise
        frame = work.rotate(angle, resample=Image.BICUBIC, expand=False)
        strip.paste(frame.resize((size, size), Image.LANCZOS), (0, i * size))

    os.makedirs(os.path.dirname(os.path.abspath(args.dest)), exist_ok=True)
    strip.save(args.dest, optimize=True)
    print(f"wrote {args.dest} — {args.frames} frames of {size}x{size} "
          f"({os.path.getsize(args.dest) / 1e6:.1f} MB)")


if __name__ == "__main__":
    main()
