#!/usr/bin/env python3
"""Builds Tools/panel-preview.html — a single self-contained file that renders
the panel artwork with working dials, so you can iterate on the SVGs without
opening a compiler.

    python3 Tools/make_preview.py     # then open Tools/panel-preview.html

You can also drag any replacement .svg straight onto the preview: it matches on
filename, so dropping your own background.svg swaps the panel immediately.
Re-run this script to bake changes back in.
"""

import os

here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
res = os.path.join(here, "Resources")

names = ["background.svg", "knob_large.svg", "knob_pointer.svg",
         "knob_small.svg", "logo.svg", "nameplate.svg"]

svgs = {}
for n in names:
    with open(os.path.join(res, n), encoding="utf-8") as f:
        svgs[n] = f.read().replace("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", "").strip()

# design-space geometry, mirroring Layout:: in PluginEditor.cpp
controls = [
    ("vintage", "knob_large.svg", 54, 124, 180, 180, 0.72),
    ("size", "knob_large.svg", 406, 124, 180, 180, 0.40),
    ("drive", "knob_small.svg", 68, 368, 64, 64, 0.5),
    ("howl", "knob_small.svg", 178, 368, 64, 64, 0.0),
    ("room", "knob_small.svg", 288, 368, 64, 64, 0.5),
    ("mix", "knob_small.svg", 398, 368, 64, 64, 1.0),
    ("output", "knob_small.svg", 508, 368, 64, 64, 0.66),
]

parts = []
parts.append("""<!DOCTYPE html>
<meta charset="utf-8">
<title>TannoyBox — panel preview</title>
<style>
  html,body{margin:0;height:100%;background:#101210;display:grid;place-items:center;
            font:12px/1.4 "Helvetica Neue",Arial,sans-serif;color:#7e8a7b}
  #wrap{position:relative;width:min(90vw,90vh*1.3333);aspect-ratio:4/3;
        box-shadow:0 24px 60px #0008}
  #wrap>*{position:absolute}
  .layer{inset:0}
  .layer svg{width:100%;height:100%;display:block}
  .knob{cursor:ns-resize;touch-action:none}
  .knob .pointer{transform-box:fill-box;transform-origin:50% 50%}
  #hint{margin-top:14px;opacity:.7;letter-spacing:1px;text-transform:uppercase}
  #drop{position:fixed;inset:0;pointer-events:none;border:3px dashed #e5195e;opacity:0}
</style>
<div id="wrap">""")

parts.append('<div class="layer" data-file="background.svg">%s</div>' % svgs["background.svg"])
parts.append('<div class="layer" data-file="logo.svg" style="left:%.4f%%;top:%.4f%%;right:auto;bottom:auto;width:%.4f%%;height:%.4f%%">%s</div>'
             % (26 / 640 * 100, 16 / 480 * 100, 240 / 640 * 100, 52 / 480 * 100, svgs["logo.svg"]))
parts.append('<div class="layer" data-file="nameplate.svg" style="left:%.4f%%;top:%.4f%%;right:auto;bottom:auto;width:%.4f%%;height:%.4f%%">%s</div>'
             % (262 / 640 * 100, 124 / 480 * 100, 116 / 640 * 100, 180 / 480 * 100, svgs["nameplate.svg"]))

for cid, facefile, x, y, w, h, val in controls:
    parts.append(
        '<div class="knob" id="%s" data-value="%s" style="left:%.4f%%;top:%.4f%%;width:%.4f%%;height:%.4f%%">'
        '<div class="face" data-file="%s" style="position:absolute;inset:0">%s</div>'
        '<div class="pointerwrap" style="position:absolute;inset:0" data-file="knob_pointer.svg">%s</div>'
        '</div>'
        % (cid, val, x / 640 * 100, y / 480 * 100, w / 640 * 100, h / 480 * 100,
           facefile, svgs[facefile], svgs["knob_pointer.svg"]))

parts.append("""</div>
<div id="hint">drag a dial &middot; drop an svg to swap it live</div>
<div id="drop"></div>
<script>
const START = 225, SWEEP = 270;

document.querySelectorAll('.knob').forEach(k => {
  const wrap = k.querySelector('.pointerwrap');
  let v = parseFloat(k.dataset.value);
  const draw = () => { wrap.style.transform = 'rotate(' + (START + v * SWEEP) + 'deg)'; };
  draw();

  k.addEventListener('pointerdown', e => {
    k.setPointerCapture(e.pointerId);
    let last = e.clientY;
    const move = ev => {
      v = Math.min(1, Math.max(0, v + (last - ev.clientY) / 200));
      last = ev.clientY;
      draw();
      report(k.id, v);
    };
    const up = () => { k.removeEventListener('pointermove', move); k.removeEventListener('pointerup', up); };
    k.addEventListener('pointermove', move);
    k.addEventListener('pointerup', up);
  });
});

function report(id, v) {
  const era = ['MODERN', '1970s', '1960s', '1950s'];
  const s = Math.min(2, Math.floor(v * 3)), t = v * 3 - s;
  const name = id === 'vintage'
      ? (t < 0.06 ? era[s] : t > 0.94 ? era[s + 1] : era[s] + ' > ' + era[s + 1])
      : id === 'size' ? Math.round(5 + 110 * v) + ' m'
      : id === 'room' ? (v < 0.005 ? 'HORN ONLY' : v > 0.995 ? 'ROOM ONLY' : Math.round(v * 100) + '%')
      : Math.round(v * 100) + '%';
  document.getElementById('hint').textContent = id + ' — ' + name;
}

// drop a replacement svg anywhere to preview it
const drop = document.getElementById('drop');
addEventListener('dragover', e => { e.preventDefault(); drop.style.opacity = 1; });
addEventListener('dragleave', () => drop.style.opacity = 0);
addEventListener('drop', e => {
  e.preventDefault(); drop.style.opacity = 0;
  for (const file of e.dataTransfer.files) {
    if (!file.name.endsWith('.svg')) continue;
    const r = new FileReader();
    r.onload = () => document.querySelectorAll('[data-file="' + file.name + '"]')
                             .forEach(el => el.innerHTML = r.result);
    r.readAsText(file);
  }
});
</script>""")

path = os.path.join(here, "Tools", "panel-preview.html")
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(parts) + "\n")
print("wrote", path)
