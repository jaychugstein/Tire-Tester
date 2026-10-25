# Draws the in-game props for Gettin' Tired as masked FX sprites -> sprites.npz
# Each sprite: 'X' = black ink, '.' = white (solid), ' ' = see-through.
import os, numpy as np
from PIL import Image, ImageDraw
ART = os.path.dirname(os.path.abspath(__file__))
def canvas(w, h): return Image.new('L', (w, h), 0), Image.new('L', (w, h), 0)   # ink, solid
def done(ink, sol): return np.stack([np.array(ink) > 0, np.array(sol) > 0])

# bird: 2 flaps, 17x11
birds = []
for up in (True, False):
    ink, sol = canvas(17, 11); di, ds = ImageDraw.Draw(ink), ImageDraw.Draw(sol)
    body = [5, 5, 12, 8]; di.ellipse(body, fill=255); ds.ellipse(body, fill=255)
    di.polygon([(12, 5), (16, 6), (12, 7)], fill=255); ds.polygon([(12, 5), (16, 6), (12, 7)], fill=255)   # beak
    di.polygon([(5, 6), (1, 4), (2, 8)], fill=255); ds.polygon([(5, 6), (1, 4), (2, 8)], fill=255)          # tail
    wing = [(7, 6), (10, 6), (5, 0)] if up else [(7, 6), (10, 6), (6, 10)]
    di.polygon(wing, fill=255); ds.polygon(wing, fill=255)
    sol.putpixel((11, 6), 255)                                                       # eye stays white
    ink.putpixel((11, 6), 0)
    birds.append(done(ink, sol))

# cloud: puffy outline, white inside, 46x18
ink, sol = canvas(46, 18); di, ds = ImageDraw.Draw(ink), ImageDraw.Draw(sol)
puffs = [(2, 7, 18, 17), (10, 2, 26, 17), (20, 0, 36, 17), (30, 5, 45, 17)]
for p in puffs: ds.ellipse(p, fill=255)
for p in puffs: di.ellipse(p, outline=255, width=1)
for p in puffs: di.ellipse((p[0] + 1, p[1] + 1, p[2] - 1, p[3] - 1), fill=0)
di.line([(4, 17), (43, 17)], fill=255)
cloud = done(ink, sol)

# blimp: big, 60x24, white stripe, fins and a gondola
ink, sol = canvas(60, 24); di, ds = ImageDraw.Draw(ink), ImageDraw.Draw(sol)
env = [0, 0, 52, 17]
di.ellipse(env, fill=255); ds.ellipse(env, fill=255)
for pts in ([(46, 8), (59, 1), (57, 8)], [(46, 9), (59, 16), (57, 9)]): di.polygon(pts, fill=255); ds.polygon(pts, fill=255)
di.line([(6, 8), (46, 8)], fill=0); di.line([(8, 9), (44, 9)], fill=0)               # white stripe
for x in range(10, 44, 6): di.point((x, 5), fill=0)                                   # little windows
g = [20, 18, 32, 22]; di.rectangle(g, fill=255); ds.rectangle(g, fill=255)
for x in range(22, 31, 3): di.point((x, 20), fill=0)
di.line([(22, 16), (22, 18)], fill=255); di.line([(30, 16), (30, 18)], fill=255)
blimp = done(ink, sol)

# pickup truck: 50x22 (bounce off the roof or the bed)
ink, sol = canvas(50, 22); di, ds = ImageDraw.Draw(ink), ImageDraw.Draw(sol)
for shape in ([0, 9, 49, 16], [14, 1, 32, 9]): ds.rectangle(shape, fill=255); di.rectangle(shape, fill=255)
di.rectangle([17, 3, 22, 7], fill=0); di.rectangle([25, 3, 30, 7], fill=0)             # windows
di.rectangle([34, 6, 49, 9], outline=255); ds.rectangle([34, 6, 49, 9], fill=255)       # bed sides
di.line([(2, 12), (47, 12)], fill=0)                                                    # trim line
for cx in (10, 40):
    ds.ellipse([cx - 5, 12, cx + 5, 21], fill=255); di.ellipse([cx - 5, 12, cx + 5, 21], fill=255)
    di.ellipse([cx - 2, 15, cx + 2, 19], fill=0); di.point((cx, 17), fill=255)
truck = done(ink, sol)

np.savez(os.path.join(ART, 'sprites.npz'), bird=np.stack(birds), cloud=cloud[None], blimp=blimp[None], truck=truck[None])
# preview
row = []
for name, arr in (('b', np.stack(birds)), ('c', cloud[None]), ('l', blimp[None]), ('t', truck[None])):
    for f in arr:
        ink, sol = f; im = np.full(ink.shape, 160, np.uint8); im[sol] = 255; im[ink] = 0
        row.append(np.pad(im, 2, constant_values=160))
h = max(r.shape[0] for r in row); row = [np.pad(r, ((0, h - r.shape[0]), (0, 0)), constant_values=160) for r in row]
Image.fromarray(np.hstack(row)).resize((sum(r.shape[1] for r in row) * 4, h * 4), 0).save(os.path.join(ART, 'sprites_preview.png'))

# tire personalities, 32x12, drawn over the tire (frames: summer, winter, all season, A/T, M/T)
# summer + A/T sit on the face (top row = tire centre - 8); winter + M/T sit on top of the tire (top row = tire top - 8)
def gear():
    fr = []
    for t in range(5):
        ink = np.zeros((12, 32), bool); sol = np.zeros((12, 32), bool)
        if t == 0:                                     # sunglasses: white frames, black lenses with a glint
            for c0 in (9, 18): sol[4:8, c0:c0 + 6] = True
            sol[4, 15:18] = True
            for c0 in (10, 19): ink[5:7, c0:c0 + 4] = True
            ink[5, 10] = False; ink[5, 19] = False
        if t == 1:                                     # toque with a pom-pom and a white stripe
            ink[0, 15:18] = ink[1:4, 14:19] = ink[4, 15:18] = True
            ink[5, 11:22] = True; ink[6:10, 9:24] = True
            sol[8, 9:24] = True; ink[8, 9:24] = False
        if t == 3:                                     # headband with tails flying off the back
            sol[1, 7:26] = True
            for i, (r, c) in enumerate([(1, 3), (1, 2), (2, 1), (2, 0), (2, 3), (3, 2), (4, 1)]): ink[r, c] = True
        if t == 4:                                     # mohawk: three spikes
            for cx, h in ((11, 3), (16, 5), (21, 3)):
                for r in range(h + 1): ink[9 - r, cx - (h - r) // 2 - 1: cx + (h - r) // 2 + 1] = True
        fr.append(np.stack([ink, sol]))
    return np.stack(fr)
d = dict(np.load(os.path.join(ART, 'sprites.npz'))); d['gear'] = gear()
np.savez(os.path.join(ART, 'sprites.npz'), **d)

# hubcap pickup, 9x9: black disc, white ring, white centre dot
def hubcap():
    ink = np.zeros((9, 9), bool); sol = np.zeros((9, 9), bool)
    yy, xx = np.mgrid[0:9, 0:9]; r = np.hypot(xx - 4, yy - 4)
    disc = r <= 4.3; ring = (np.abs(r - 2.1) < 0.6) | (r < 0.5)
    ink[disc & ~ring] = True; sol[disc & ring] = True
    return np.stack([ink, sol])[None]
d = dict(np.load(os.path.join(ART, 'sprites.npz'))); d['hubcap'] = hubcap()
np.savez(os.path.join(ART, 'sprites.npz'), **d)

# small course props, same pixels the game used to draw by hand (black ink only), 2 frames each
def line(m, x0, y0, x1, y1):
    dx, dy = abs(x1 - x0), -abs(y1 - y0); sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1); e = dx + dy
    while True:
        if 0 <= y0 < m.shape[0] and 0 <= x0 < m.shape[1]: m[y0, x0] = True
        if x0 == x1 and y0 == y1: break
        e2 = 2 * e
        if e2 >= dy: e += dy; x0 += sx
        if e2 <= dx: e += dx; y0 += sy
def props():
    out = {}
    T = []                                             # trampoline: top row = ground - 7, 24x9; frame 1 sags
    for sag in (0, 3):
        m = np.zeros((9, 24), bool); m[0:2, :] = True; m[1:7, 2] = True; m[1:7, 21] = True
        for k in range(5): line(m, 3 + k * 4, 2, 5 + k * 4, 5 + sag)
        T.append(np.stack([m, np.zeros_like(m)]))
    out['tramp'] = np.stack(T)
    B = []                                             # boost arrows: top row = ground - 4, 18x7
    for an in (0, 1):
        m = np.zeros((7, 18), bool)
        for k in range(3):
            ax = k * 6 + an
            for o in (0, 1): line(m, ax + o, 0, ax + o + 3, 3); line(m, ax + o + 3, 3, ax + o, 6)
        B.append(np.stack([m, np.zeros_like(m)]))
    out['boost'] = np.stack(B)
    R = []                                             # boost ring: top row = centre - 14, 7x25
    for an in (0, 1):
        m = np.zeros((25, 7), bool)
        m[4, 0:7] = m[23, 0:7] = True; m[4:24, 0] = m[4:24, 6] = True          # drawRect(0, -10, 7, 20)
        m[3, 1:6] = m[24, 1:6] = True; m[3:25, 1] = m[3:25, 5] = True          # drawRect(1, -11, 5, 22)
        m[5:23, 2] = True; m[1 - an, 3] = True
        R.append(np.stack([m, np.zeros_like(m)]))
    out['ring'] = np.stack(R)
    return out
d = dict(np.load(os.path.join(ART, 'sprites.npz'))); d.update(props())
np.savez(os.path.join(ART, 'sprites.npz'), **d)

def rrect(m, x, y, w, h):                              # filled rounded rectangle, radius 2 (like Arduboy fillRoundRect)
    m[y:y + h, x:x + w] = True
    for (cy, cx) in ((y, x), (y, x + w - 1), (y + h - 1, x), (y + h - 1, x + w - 1)): m[cy, cx] = False
    m[y, x + 1] = m[y, x + w - 2] = m[y + h - 1, x + 1] = m[y + h - 1, x + w - 2] = False
    m[y + 1, x] = m[y + 1, x + w - 1] = m[y + h - 2, x] = m[y + h - 2, x + w - 1] = False
def stacks():                                         # tire stacks 3, 4 and 5 high, 20x35, bottom row = ground - 1
    F = []
    for n in (3, 4, 5):
        ink = np.zeros((35, 20), bool); wh = np.zeros_like(ink)
        for k in range(n):
            ty = 35 - 7 - k * 7
            rrect(ink, 0, ty, 20, 7); wh[ty + 3, 5:15] = True; wh[ty + 1, 3 + (k & 1) * 13] = True
        ink &= ~wh; F.append(np.stack([ink, wh]))
    return np.stack(F)
def bump():                                           # speed bump 12x5 with white stripes
    ink = np.zeros((5, 12), bool); wh = np.zeros_like(ink); rrect(ink, 0, 0, 12, 5)
    for k in (2, 5, 8): line(wh, k, 3, k + 1, 1)
    wh &= ink; ink &= ~wh
    return np.stack([ink, wh])[None]
d = dict(np.load(os.path.join(ART, 'sprites.npz'))); d['stack'] = stacks(); d['bump'] = bump()
np.savez(os.path.join(ART, 'sprites.npz'), **d)
