"""basalt logo: hexagonal basalt columns rising out of lava, clipped to a circle.

Shapes are drawn back to front. Each shape is masked by the silhouettes of
everything in front of it (stroked a little wider), which leaves transparent
gaps between shapes so the logo works on light and dark backgrounds.

Usage: python3 gen-logo.py out.svg
"""
import math
import sys

SIZE = 512
GAP = 12         # width of the transparent gaps
SQUASH = 0.5     # flattening of the hexagon tops (fake perspective)
R = 74           # column radius

COLUMN = {"top": "#9fb0d6", "left": "#4a5a85", "mid": "#3a486f", "right": "#2b3657"}
GLOW = {"left": "#c4573f", "mid": "#a8463a", "right": "#7e3a3d"}
LAVA = {"glow": "#ffc857", "surface": "#f7932f", "deep": "#e14b2b"}


def pts(points):
    return " ".join(f"{x:.1f},{y:.1f}" for x, y in points)


def hexagon(cx, cy, r):
    return [(cx + r * math.cos(math.radians(a)), cy + r * SQUASH * math.sin(math.radians(a)))
            for a in range(0, 360, 60)]


def column(cx, top, r):
    t, b = hexagon(cx, top, r), hexagon(cx, SIZE + 120, r)
    faces = [(3, 2, "url(#glow-left)"), (2, 1, "url(#glow-mid)"), (1, 0, "url(#glow-right)")]
    body = [t[3], t[2], t[1], t[0], b[0], b[1], b[2], b[3]]
    draw = f'<polygon points="{pts(body)}" fill="{COLUMN["mid"]}"/>'
    draw += "".join(f'<polygon points="{pts([t[i], t[j], b[j], b[i]])}" fill="{c}"/>' for i, j, c in faces)
    draw += f'<polygon points="{pts(t)}" fill="{COLUMN["top"]}"/>'
    silhouette = f'<polygon points="{pts([t[3], t[4], t[5], t[0], b[0], b[1], b[2], b[3]])}"/>'
    return silhouette, draw


def wave(y, amp, phase=0.0, slope=-0.09):
    """Closed path: a gentle wave at height y, filled down past the bottom edge."""
    d = f"M-20,{y + slope * (-20 - 256):.1f}"
    steps = 4
    w = (SIZE + 40) / steps
    for k in range(steps):
        x0 = -20 + k * w
        sign = 1 if (k + phase) % 2 == 0 else -1
        ym, ye = y + slope * (x0 + w / 2 - 256), y + slope * (x0 + w - 256)
        d += f" Q{x0 + w / 2:.1f},{ym + sign * amp:.1f} {x0 + w:.1f},{ye:.1f}"
    return d + f" L{SIZE + 20},{SIZE + 40} L-20,{SIZE + 40} Z"


def build():
    dy = R * SQUASH * math.sqrt(3) / 2
    # (x offset in radii, row depth, height above the lava line)
    spec = [(-1.5, 0, 250), (1.5, 0, 280), (-3, 1, 150), (0, 1, 345), (3, 1, 175)]
    base = 470
    layers = []
    for xo, row, h in sorted(spec, key=lambda s: s[1]):
        layers.append(column(256 + xo * R, base - h + row * dy, R - 3))

    lava_y = 410
    lava_draw = (f'<path d="{wave(lava_y, 9)}" fill="{LAVA["surface"]}"/>'
                 f'<path d="{wave(lava_y + 34, 9, 1)}" fill="{LAVA["deep"]}"/>'
                 f'<path d="{wave(lava_y + 92, 9)}" fill="#c73a24"/>')
    layers.append((f'<path d="{wave(lava_y, 9)}"/>', lava_draw))

    # embers: small square sparks drifting up from the lava (a nod to Minecraft pixels)
    embers = [(92, 128, 15, LAVA["surface"]), (128, 72, 9, LAVA["glow"]), (408, 96, 12, LAVA["glow"]), (448, 168, 8, LAVA["surface"])]
    ember_draw = "".join(f'<rect x="{x - s / 2}" y="{y - s / 2}" width="{s}" height="{s}" fill="{c}" transform="rotate(45 {x} {y})"/>'
                         for x, y, s, c in embers)
    layers.insert(0, ("", ember_draw))

    defs, body = [], []
    for name in ("left", "mid", "right"):
        defs.append(f'<linearGradient id="glow-{name}" gradientUnits="userSpaceOnUse" x1="0" y1="{lava_y - 150}" x2="0" y2="{lava_y + 10}">'
                    f'<stop offset="0" stop-color="{COLUMN[name]}"/>'
                    f'<stop offset="0.55" stop-color="{COLUMN[name]}"/>'
                    f'<stop offset="1" stop-color="{GLOW[name]}"/></linearGradient>')
    for k, (_, draw) in enumerate(layers):
        front = [sil for sil, _ in layers[k + 1:]]
        if not front:
            body.append(draw)
            continue
        cut = (f'<g fill="#000" stroke="#000" stroke-width="{GAP}" stroke-linejoin="round">'
               + "".join(front) + "</g>")
        defs.append(f'<mask id="m{k}" maskUnits="userSpaceOnUse" x="0" y="0" width="{SIZE}" height="{SIZE}">'
                    f'<rect width="{SIZE}" height="{SIZE}" fill="#fff"/>{cut}</mask>')
        body.append(f'<g mask="url(#m{k})">{draw}</g>')

    nl = "\n"
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {SIZE} {SIZE}" width="{SIZE}" height="{SIZE}">
<defs>
<clipPath id="badge"><circle cx="{SIZE / 2}" cy="{SIZE / 2}" r="{SIZE / 2 - 4}"/></clipPath>
{nl.join(defs)}
</defs>
<g clip-path="url(#badge)">
{nl.join(body)}
</g>
</svg>
'''


if __name__ == "__main__":
    with open(sys.argv[1], "w") as f:
        f.write(build())
