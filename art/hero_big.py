# The title screen's tire guy, bigger and more detailed than the sound-screen one: 50x49, frames 0 smile, 1 blink.
# Tread blocks round the edge, a groove and a dotted lettering ring on the sidewall, eyes with glints, eyebrows,
# a toothy grin, little arms with white gloves, and sneakers with laces.
import os, numpy as np
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
W, H, S = 50, 49, 8
def frame(kind):
    ys, xs = np.mgrid[0:H * S, 0:W * S]; X = (xs + .5) / S; Y = (ys + .5) / S
    cx, cy, R = 25.0, 21.5, 20.5
    r = np.hypot(X - cx, Y - cy); th = np.arctan2(Y - cy, X - cx)
    u = (th / (2 * np.pi) * 20) % 1.0                          # 20 chunky tread blocks
    ink = np.zeros_like(r, bool); sol = np.zeros_like(ink)
    body = (r <= R) & ~((u < 0.34) & (r > R - 2.6))             # deep notches between tread blocks
    ink |= body
    sol |= (np.abs(r - (R - 4.6)) < 0.55) & body                # groove between tread and sidewall (the rim edge)
    sol |= (np.abs(r - (R - 7.4)) < 0.55) & (((th / (2 * np.pi) * 40) % 1.0) < 0.45)   # dotted lettering ring
    # face
    ey = cy - 3.2
    for side in (-1, 1):
        ex = cx + side * 5.0
        if kind == 0:
            eye = ((X - ex) / 3.3) ** 2 + ((Y - ey) / 4.3) ** 2 <= 1
            pupil = np.hypot(X - (ex + 0.7), Y - (ey + 1.0)) < 1.9
            glint = np.hypot(X - (ex + 0.1), Y - (ey + 0.3)) < 0.6
            sol |= eye & (~pupil | glint)
        else:
            sol |= (np.abs(Y - (ey + 0.9 + 0.25 * (X - ex) ** 2 * -1 + 0.9)) < 0.6) & (np.abs(X - ex) < 2.7)
        sol |= (np.abs(Y - (ey - 5.0 + 0.32 * (X - ex) * side)) < 0.6) & (np.abs(X - ex) < 2.6)   # eyebrows
    my = cy + 4.4
    grin = (((X - cx) / 6.2) ** 2 + ((Y - my + 0.6) / 5.0) ** 2 <= 1) & (Y >= my - 0.6)
    teeth = grin & (Y < my + 1.6)
    sol |= grin & ~(np.abs(Y - (my + 1.6)) < 0.5) & ~(teeth & (np.abs(((X - cx) % 2.6) - 1.3) < 0.25) & False)
    sol &= body
    # arms with white gloves
    for side in (-1, 1):
        gx, gy = cx + side * (R + 1.6), cy + 10.0
        glove = np.hypot(X - gx, Y - gy) < 2.3
        cuff = np.hypot(X - gx, Y - gy) < 3.0
        # arm: a thick line from the tire's side down and out to the glove
        ax, ay = cx + side * (R - 3.0), cy + 4.0
        t = np.clip(((X - ax) * (gx - ax) + (Y - ay) * (gy - ay)) / ((gx - ax) ** 2 + (gy - ay) ** 2), 0, 1)
        arm = np.hypot(X - (ax + t * (gx - ax)), Y - (ay + t * (gy - ay))) < 1.0
        ink |= (cuff | arm) & ~glove
        sol |= glove
        sol &= ~(glove & (np.abs(Y - gy) < 0.35) & (np.abs(X - gx) < 1.6))     # finger line
    # sneakers
    by = cy + R - 1.0
    for side in (-1, 1):
        sx = cx + side * 8.5
        shoe = (((X - sx - side * 1.6) / 6.6) ** 2 + ((Y - by - 2.8) / 3.4) ** 2 <= 1) & (Y < by + 5.4)
        sole = shoe & (Y > by + 3.9)
        ink |= shoe & ~sole; sol |= sole
        ink |= (np.abs(Y - (by + 5.7)) < 0.55) & (np.abs(X - sx - side * 1.6) < 6.2)
        for k in (-1.2, 1.0):
            sol |= shoe & (np.abs(Y - (by + 1.6 + 0.0)) < 0.45) & (np.abs(X - (sx + side * 0.6 + k)) < 0.6)   # laces
    def down(m): return m.reshape(H, S, W, S).mean(axis=(1, 3)) > 0.5
    i, so = down(ink), down(sol); i &= ~so
    return np.stack([i, so])
F = np.stack([frame(k) for k in range(2)])
U = F.any(axis=(0, 1)); ys, xs = np.where(U); F = F[:, :, ys.min():ys.max() + 1, xs.min():xs.max() + 1]   # trim to the drawing
H, W = F.shape[2:]
np.save(os.path.join(ART, 'hero_big.npy'), F)
row = []
for f in F:
    im = np.full((H, W), 200, np.uint8); im[f[1]] = 255; im[f[0]] = 0; row.append(np.pad(im, 2, constant_values=120))
a = np.hstack(row); Image.fromarray(a).resize((a.shape[1] * 8, a.shape[0] * 8), 0).save('/tmp/claude-0/-home-claude/49fb13ae-6b6b-5afd-b74e-937abad25348/scratchpad/hero_big.png')
