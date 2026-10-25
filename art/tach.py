# Big rev counter face for Tire Tester: 48x48, numbers 0-8 (x1000 RPM), red zone with the white "perfect" notch.
# -> tach.npy (1, 2, 48, 48) [ink, solid]; the needle is drawn by the game (centre 24,24).
import os, math, numpy as np
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
D = {'0': ['111', '101', '101', '101', '111'], '1': ['010', '110', '010', '010', '111'], '2': ['111', '001', '111', '100', '111'],
     '3': ['111', '001', '111', '001', '111'], '4': ['101', '101', '111', '001', '001'], '5': ['111', '100', '111', '001', '111'],
     '6': ['111', '100', '111', '101', '111'], '7': ['111', '001', '010', '010', '010'], '8': ['111', '101', '111', '101', '111'],
     'R': ['110', '101', '110', '101', '101'], 'P': ['110', '101', '110', '100', '100'], 'M': ['101', '111', '111', '101', '101'],
     'x': ['000', '101', '010', '101', '000']}
S = 8; W = 48; c = 23.5
ys, xs = np.mgrid[0:W * S, 0:W * S]; X = (xs + .5) / S - c - .5 + .5; Y = (ys + .5) / S - c - .5 + .5
r = np.hypot(X, Y); ang = (np.degrees(np.arctan2(-Y, X)) + 360) % 360
def down(m): return m.reshape(W, S, W, S).mean(axis=(1, 3)) > 0.5
def a_of(v): return (225 - v / 8 * 270) % 360                 # 0 rpm lower left .. 8 lower right, like the game's needle
def between(v0, v1):
    a0, a1 = a_of(v1), a_of(v0)                                  # angle runs backwards as rpm goes up
    return ((ang - a0) % 360) <= ((a1 - a0) % 360)
solid = down(r <= 23.5)
ink = down((r <= 23.5) & (r > 21.6))                              # rim
ink |= down(between(7, 8) & (r <= 21.6) & (r > 16.0))            # red zone band
ink &= ~down(between(7.1, 7.8) & (r <= 20.6) & (r > 17.0))       # the perfect notch stays white
for i in range(17):                                              # ticks every 500
    v = i / 2; a = math.radians(a_of(v)); L = 4.5 if i % 2 == 0 else 2.5
    if v >= 7: continue
    for t in np.linspace(21.6 - L, 21.6, 20):
        x, y = c + t * math.cos(a), c - t * math.sin(a); ink[int(y), int(x)] = True
ink = ink & solid
def text(s, x0, y0, colour=True):
    for k, ch in enumerate(s):
        for j, row in enumerate(D[ch]):
            for i, b in enumerate(row):
                if b == '1': ink[y0 + j, x0 + k * 4 + i] = colour
for v in range(9):                                               # numbers inside the ticks
    a = math.radians(a_of(v)); rr = 12.0
    x, y = c + rr * math.cos(a), c - rr * math.sin(a)
    text(str(v), int(round(x - 1.0)), int(round(y - 2.0)))
text("RPM", 18, 37); text('x1000'[:1], 22, 36) if False else None
np.save(os.path.join(ART, 'tach.npy'), np.stack([ink, solid])[None])
im = np.full((W, W), 160, np.uint8); im[solid] = 255; im[ink] = 0
Image.fromarray(im).resize((W * 6, W * 6), 0).save('/tmp/tach_new.png')
