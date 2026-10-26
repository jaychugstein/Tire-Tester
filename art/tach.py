# Big rev counter face for Tire Tester: 62x62, numbers 0-8 (x1000 RPM) in the game's 5x7 font, red zone with the
# white "perfect" notch (7.1-7.8, matching P_LO/P_HI). -> tach.npy (1, 2, 62, 62) [ink, solid]; the game draws the
# needle from the centre (31, 31).
import os, re, math, numpy as np
from PIL import Image
ART = os.path.dirname(os.path.abspath(__file__))
src = open(os.path.join(ART, '..', 'TireTester' if os.path.isdir(os.path.join(ART, '..', 'TireTester')) else 'game', 'font.h')).read()
FONT = [int(v) for v in re.search(r'FONT\[\] PROGMEM = \{(.*?)\};', src, re.S).group(1).replace(' ', '').split(',') if v]
S = 8; W = 62; c = 30.5; R = 30.5
ys, xs = np.mgrid[0:W * S, 0:W * S]; X = (xs + .5) / S - c - .5; Y = (ys + .5) / S - c - .5
r = np.hypot(X, Y); ang = (np.degrees(np.arctan2(-Y, X)) + 360) % 360
def down(m): return m.reshape(W, S, W, S).mean(axis=(1, 3)) > 0.5
def a_of(v): return (225 - v / 8 * 270) % 360                 # 0 rpm lower left .. 8 lower right, like the game's needle
def between(v0, v1):
    a0, a1 = a_of(v1), a_of(v0)
    return ((ang - a0) % 360) <= ((a1 - a0) % 360)
solid = down(r <= R)
ink = down((r <= R) & (r > R - 2.4))                              # rim
ink |= down(between(7, 8) & (r <= R - 2.4) & (r > R - 9.0))       # red zone band
ink &= ~down(between(7.1, 7.8) & (r <= R - 3.4) & (r > R - 8.0))  # the perfect notch stays white
for i in range(17):                                              # ticks every 500
    v = i / 2
    if v >= 7: continue
    a = math.radians(a_of(v)); L = 6.0 if i % 2 == 0 else 3.5
    for t in np.linspace(R - 2.4 - L, R - 2.4, 30):
        x, y = c + t * math.cos(a), c - t * math.sin(a); ink[int(y), int(x)] = True
ink &= solid
def glyph(ch, x0, y0):
    g = ord(ch) - 32
    for i in range(5):
        col = FONT[g * 5 + i]
        for j in range(7):
            if col >> j & 1: ink[y0 + j, x0 + i] = True
for v in range(9):                                               # numbers inside the ticks
    a = math.radians(a_of(v)); rr = 16.5
    x, y = c + rr * math.cos(a), c - rr * math.sin(a)
    glyph(str(v), int(round(x - 2.5)), int(round(y - 3.5)))
for k, ch in enumerate('RPM'): glyph(ch, 22 + k * 6, 48)
np.save(os.path.join(ART, 'tach.npy'), np.stack([ink, solid])[None])
im = np.full((W, W), 160, np.uint8); im[solid] = 255; im[ink] = 0
Image.fromarray(im).resize((W * 5, W * 5), 0).save('/tmp/tach_new.png')
