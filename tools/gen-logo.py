# basalt logo: hexagonal basalt columns rising in front of a moon,
# clipped to a circle. Gaps between shapes are transparent cut-outs.
import math, sys
K = 0.5          # squash of the hexagon tops (fake perspective)
GAPW = 12        # width of the transparent gap between shapes
TOP, L, C, R = "#9fb0d6", "#4a5a85", "#3a486f", "#2b3657"
MOON = "#f59e4c"

def P(pts): return " ".join(f"{x:.1f},{y:.1f}" for x, y in pts)
def ring(cx, y, r):
    return [(cx + r*math.cos(math.radians(a)), y + r*K*math.sin(math.radians(a))) for a in range(0, 360, 60)]

r = 74
dy = r*K*math.sqrt(3)/2
# (x, depth, height): back row then front row
spec = [(256-1.5*r, 0, 205), (256+1.5*r, 0, 365), (256-3*r, dy, 165), (256, dy, 235), (256+3*r, dy, 330)]
cols = sorted([(x, 500 - h + d, d) for x, d, h in spec], key=lambda c: c[2])

shapes = []  # (silhouette points list, svg for the shape)
for x, top, _ in cols:
    t, b = ring(x, top, r - 3), ring(x, 640, r - 3)
    sil = [t[3], t[4], t[5], t[0], b[0], b[1], b[2], b[3]]
    parts = [f'<polygon points="{P([t[i], t[j], b[j], b[i]])}" fill="{c}"/>' for i, j, c in [(3, 2, L), (2, 1, C), (1, 0, R)]]
    parts.append(f'<polygon points="{P(t)}" fill="{TOP}"/>')
    shapes.append((sil, "".join(parts)))

defs, body = [], []
layers = [(None, f'<circle cx="185" cy="175" r="120" fill="{MOON}"/>')] + shapes
for k, (_, svg) in enumerate(layers):
    front = [s for s, _ in layers[k+1:]]
    if front:
        cut = "".join(f'<polygon points="{P(s)}" fill="#000" stroke="#000" stroke-width="{GAPW}" stroke-linejoin="round"/>' for s in front)
        defs.append(f'<mask id="m{k}" maskUnits="userSpaceOnUse" x="0" y="0" width="512" height="512"><rect width="512" height="512" fill="#fff"/>{cut}</mask>')
        body.append(f'<g mask="url(#m{k})">{svg}</g>')
    else:
        body.append(svg)

out = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" width="512" height="512">
<defs>
<clipPath id="c"><circle cx="256" cy="256" r="252"/></clipPath>
{chr(10).join(defs)}
</defs>
<g clip-path="url(#c)">
{chr(10).join(body)}
</g>
</svg>
'''
open(sys.argv[1], "w").write(out)
