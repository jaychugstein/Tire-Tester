# Pressure-screen tire for each tire type: takes psi_tire.npy (31 PSI frames, all-season) and re-cuts the
# outer edge with each type's tread (same patterns as tread.py). -> psi_types.npy (5, 31, 40, 51), white = 1
import os, numpy as np
from scipy import ndimage as nd
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
F0 = np.load(os.path.join(ART, 'psi_tire.npy'))
PAD = 6                                                     # room on top so a round tire and M/T lugs aren't cut off
F = np.ones((F0.shape[0], F0.shape[1] + PAD, F0.shape[2]), bool); F[:, PAD:] = F0
for f in F:                                                 # the old capture was clipped at its top row: rebuild the top
    S = ~f
    if not S[PAD].any(): continue
    ys, xs = np.nonzero(S); cx = xs.mean()
    hw = np.array([(np.nonzero(S[y])[0][-1] - np.nonzero(S[y])[0][0] + 1) / 2 if S[y].any() else 0 for y in range(S.shape[0])])
    cy = int(np.argmax(hw[:PAD + 30])); a = hw[cy]          # widest row = centre height, half width
    bs = [(cy - y) / np.sqrt(1 - (hw[y] / a) ** 2) for y in range(PAD + 2, cy - 2) if 0 < hw[y] < 0.97 * a]
    b = float(np.median(bs))
    yy, xx = np.mgrid[0:f.shape[0], 0:f.shape[1]]
    ell = ((xx + 0.5 - (np.nonzero(S[cy])[0][0] + a)) / a) ** 2 + ((yy + 0.5 - cy - 0.5) / b) ** 2 <= 1
    top = yy < PAD + 4
    f[ell & top & (yy < PAD + 4)] = False                   # fill the missing top with rubber

N = [16, 32, 16, 16, 8]; s = 1.1
out_all = []
for t in range(5):
    fr = []
    for f in F:
        black = ~f
        S = nd.binary_fill_holes(black)                     # tire silhouette
        H, W = S.shape; yy, xx = np.mgrid[0:H, 0:W]
        cy, cx = yy[S].mean(), xx[S].mean(); bottom = yy[S].max()
        r = np.hypot(yy - cy, xx - cx); th = np.arctan2(yy - cy, xx - cx)
        bins = ((th + np.pi) / (2 * np.pi) * 180).astype(int) % 180     # outline radius at each angle
        rho = np.zeros(180)
        np.maximum.at(rho, bins[S], r[S] + 0.5)
        rho = nd.gaussian_filter1d(nd.maximum_filter1d(rho, 3, mode='wrap'), 2.0, mode='wrap')
        d = rho[bins] - r                                   # depth in from the outline (negative = outside)
        din = np.where(S, d, -1)
        u = (np.arctan2(yy - cy, xx - cx) / (2 * np.pi) * N[t]) % 1.0
        img = f.copy()
        img[S & (din < 2.6)] = False                        # wipe the old dotted tread
        white = np.zeros_like(S); outs = np.zeros_like(S)
        ring = np.abs(d - 4.0) < 0.5
        if t == 0: white |= ring
        if t == 1: white |= (u < 0.4) & (d >= 0) & (d < 2.2 * s)
        if t == 2: white |= ((u < 0.28) & (d >= 0) & (d < 1.3 * s)) | ring
        if t == 3:
            u3 = (u + d * 0.07 / s) % 1.0
            white |= (u3 < 0.4) & (d >= 0) & (d < 2.4 * s)
            white |= (np.abs(u3 - 0.7) < 0.12) & (d > 2.4 * s) & (d < 3.8 * s)
        if t == 4:
            lug = (u > 0.35) & (u < 0.95)
            outs |= lug & (d < 0) & (d > -1.7 * s) & (yy < bottom)
            white |= ~lug & (d >= 0) & (d < 2.2 * s)
        white &= S & (yy < bottom)                          # leave the flat patch on the ground alone
        ring_only = ring & S
        img[white] = True
        img[outs] = False
        fr.append(img)
    out_all.append(np.stack(fr))
A = np.stack(out_all); np.save(os.path.join(ART, 'psi_types.npy'), A)   # (5, 31, 46, 51)
rows = [np.hstack([np.pad(A[t, i].astype(np.uint8) * 255, 2, constant_values=128) for i in (0, 10, 20, 30)]) for t in range(5)]
a = np.vstack(rows); Image.fromarray(a).resize((a.shape[1] * 3, a.shape[0] * 3), 0).save('/tmp/psi_types.png')
