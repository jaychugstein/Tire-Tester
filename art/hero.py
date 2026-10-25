# Big detailed tire guy for the title and sound screens. 40x46, frames: 0 smile, 1 blink, 2 squashed (landing), 3 asleep.
import os, numpy as np
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
W, H, S = 40, 46, 8
def frame(kind):
    ys, xs = np.mgrid[0:H * S, 0:W * S]; X = (xs + .5) / S; Y = (ys + .5) / S
    sq = kind == 2
    cx, cy, rx, ry = 20.0, (21.5 if not sq else 24.0), (17.5 if not sq else 19.0), (17.5 if not sq else 15.0)
    nx, ny = (X - cx) / rx, (Y - cy) / ry; e = np.hypot(nx, ny)        # 1.0 = outer edge
    th = np.arctan2(ny, nx); u = (th / (2 * np.pi) * 16) % 1.0
    d = (1 - e) * 17.5                                                 # px in from the edge
    body = (e <= 1) & ~((u < 0.32) & (d < 1.4))                         # rubber with tread notches on the edge
    white = np.zeros_like(body)
    rr = np.hypot((X - cx) * 17.5 / rx, (Y - cy) * 17.5 / ry)
    ta = np.arctan2(Y - cy, X - cx)
    white |= (np.abs(rr - 12.6) < 0.55) & (((ta / (2 * np.pi) * 36) % 1.0) < 0.5)   # dotted hubcap ring
    white |= (np.abs(rr - 3.0 - 11.0) < 0.0)
    ey = cy - 3.0 * ry / 17.5
    for side in (-1, 1):
        ex = cx + side * 4.8
        if kind in (0, 2):
            eye = ((X - ex) / 2.9) ** 2 + ((Y - ey) / 3.7) ** 2 <= 1
            pupil = np.hypot(X - (ex + 0.9), Y - (ey + 0.6)) < 1.5
            glint = pupil & False
            white |= eye & (~pupil | glint)
        else:                                                           # eyes shut
            white |= (np.abs(Y - (ey + 0.8 - 0.3 * (X - ex) ** 2 * -1)) < 0.6) & (np.abs(X - ex) < 2.4)
        if kind != 3:
            white |= (np.abs(Y - (ey - 4.6 + 0.3 * (X - ex) * side)) < 0.55) & (np.abs(X - ex) < 2.3)   # eyebrows
    my = cy + 3.6 * ry / 17.5
    if kind == 3:
        white |= np.hypot((X - cx) / 1.4, (Y - my - 0.5) / 1.0) < 1.0   # small sleepy "o"
    else:
        grin = (((X - cx) / 5.4) ** 2 + ((Y - my + 0.5) / 4.6) ** 2 <= 1) & (Y >= my - 0.5)
        white |= grin & ~(np.abs(Y - (my + 1.4)) < 0.45)                # big grin, teeth line across
    white &= body
    ink = body & ~white
    sol = white.copy()
    # sneakers
    if True:
        by = cy + ry - 1.0
        for side in (-1, 1):
            sx = cx + side * 8.0
            shoe = (((X - sx - side * 1.5) / 6.0) ** 2 + ((Y - by - 2.5) / 3.0) ** 2 <= 1) & (Y < by + 5.0)
            sole = shoe & (Y > by + 3.6)
            ink |= shoe & ~sole
            sol |= sole
            ink |= (np.abs(Y - (by + 5.2)) < 0.5) & (np.abs(X - sx - side * 1.5) < 5.6)
            lace = shoe & (np.abs(X - sx - side * 0.5) < 0.6) & (Y < by + 2.6) & (Y > by + 0.6)
            sol |= lace; ink &= ~lace
    def down(m): return m.reshape(H, S, W, S).mean(axis=(1, 3)) > 0.5
    i, so = down(ink), down(sol); so &= ~i
    return np.stack([i, so])
F = np.stack([frame(k) for k in range(4)])
np.save(os.path.join(ART, 'hero.npy'), F)
row = []
for f in F:
    im = np.full((H, W), 160, np.uint8); im[f[1]] = 255; im[f[0]] = 0; row.append(np.pad(im, 2, constant_values=100))
a = np.hstack(row); Image.fromarray(a).resize((a.shape[1] * 6, a.shape[0] * 6), 0).save('/tmp/hero.png')
