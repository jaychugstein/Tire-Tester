# Side-view tread for each tire type, 8 rotation frames per type (one tread pitch), small (r13) and big (r22).
# Output tread.npz: small (5,8,2,H,W), big (5,8,2,H,W); [ink, solid]. Only the outer band is drawn; the
# middle stays see-through so the game's hubcap and face show.
import os, numpy as np
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
N = [16, 32, 16, 16, 8]                       # tread blocks around the tire: SUMMER, WINTER, ALL SEASON, A/T, M/T
SS = 8                                        # supersampling

def tire(t, R, rot):
    pad = 3; W = 2 * (R + pad) + 1; c = R + pad
    ys, xs = np.mgrid[0:W * SS, 0:W * SS]
    X = (xs + 0.5) / SS - c - 0.5; Y = (ys + 0.5) / SS - c - 0.5
    r = np.hypot(X, Y); th = np.arctan2(Y, X)
    n = N[t]; u = ((th / (2 * np.pi) * n + rot) % 1.0)        # position inside one tread block, 0..1
    d = R - r                                                   # depth in from the outer edge (px)
    band = 4.5 if R < 20 else 6.5
    rubber = (d >= 0) & (d < band)
    white = np.zeros_like(rubber); out = np.zeros_like(rubber)
    s = 1 if R < 20 else 1.6                                    # bigger tire, bigger tread
    ring = np.abs(d - (3.0 if R < 20 else 4.6)) < 0.5            # where the tread meets the sidewall
    if t == 0:    # SUMMER: smooth slick, nothing cut into the edge
        white |= ring
    if t == 1:    # WINTER: lots of thin sipes cut straight in
        white |= (u < 0.4) & (d < 2.2 * s)
    if t == 2:    # ALL SEASON: small even notches, sidewall line
        white |= ((u < 0.28) & (d < 1.3 * s)) | ring
    if t == 3: u = (u + d * 0.07 / s) % 1.0                    # A/T blocks lean a little
    if t == 3:    # A/T: square blocks, deep notches, staggered slots behind them
        white |= (u < 0.4) & (d < 2.4 * s)
        white |= (np.abs(u - 0.7) < 0.12) & (d > 2.4 * s) & (d < 3.8 * s)
    if t == 4:    # M/T: big lugs sticking out with deep gaps between
        lug = (u > 0.35) & (u < 0.95)
        out |= lug & (d < 0) & (d > -1.7 * s)
        white |= ~lug & (d < 2.2 * s)
    ink = (rubber | out) & ~white
    sol = rubber & white
    def down(m): return m.reshape(W, SS, W, SS).mean(axis=(1, 3)) > 0.5
    ink, sol = down(ink), down(sol)
    sol &= ~ink
    return np.stack([ink, sol])

def frames(R): return np.stack([np.stack([tire(t, R, f / 8) for f in range(8)]) for t in range(5)])
small, big = frames(13), frames(22)
np.savez(os.path.join(ART, 'tread.npz'), small=small, big=big)

def show(F, R, path, fill=True):
    rows = []
    for t in range(5):
        row = []
        for f in (0, 2, 4, 6):
            ink, sol = F[t, f]; H, W = ink.shape; c = W // 2
            im = np.full((H, W), 255, np.uint8)
            if fill:
                yy, xx = np.mgrid[0:H, 0:W]; im[np.hypot(xx - c, yy - c) <= R - 0.5] = 0
            im[sol] = 255; im[ink] = 0
            row.append(np.pad(im, 2, constant_values=150))
        rows.append(np.hstack(row))
    a = np.vstack(rows); Image.fromarray(a).resize((a.shape[1] * 4, a.shape[0] * 4), 0).save(path)
show(small, 13, '/tmp/tread_small.png'); show(big, 22, '/tmp/tread_big.png')
