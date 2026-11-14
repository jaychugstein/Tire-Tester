# Draws the lettering for Tire Tester (Inter Black Italic, SIL Open Font License) -> lettering.npz
import os
from PIL import Image, ImageDraw, ImageFont
import numpy as np
ART = os.path.dirname(os.path.abspath(__file__))
F = '/usr/share/fonts/opentype/inter/Inter-BlackItalic.otf'
def render(txt, h, maxw):
    for size in range(60, 8, -1):
        f = ImageFont.truetype(F, size * 4)
        im = Image.new('L', (2000, 600), 0); ImageDraw.Draw(im).text((20, 20), txt, font=f, fill=255)
        im = im.crop(im.getbbox()); w, hh = im.size; nw = round(w * h / hh)
        if nw <= maxw: break
    return np.array(im.resize((nw, h), Image.LANCZOS)) > 110
from scipy import ndimage as nd
def decal(h, gap):                                  # chrome decal: white letters, 2px black outline, solid shadow down-right, black shine line
    a = render("TIRE", h, 999); b = render("TESTER", h, 999)
    W = max(a.shape[1], b.shape[1]) + 8; H = a.shape[0] + gap + b.shape[0] + 8
    m = np.zeros((H, W), bool); m[2:2 + a.shape[0], 2:2 + a.shape[1]] = a; y2 = 2 + a.shape[0] + gap; m[y2:y2 + b.shape[0], 2:2 + b.shape[1]] = b
    ol = nd.binary_dilation(m, iterations=2)
    sh = np.zeros_like(ol); sh[2:, 2:] = ol[:-2, :-2]
    band = np.zeros_like(m)
    for r in (h * 9 // 16, h * 11 // 16): band[2 + r, :] = True; band[y2 + r, :] = True
    white = m & ~(band & nd.binary_erosion(m))
    ink_ = (ol | sh) & ~white
    ys, xs = np.where(ink_ | white); ink_ = ink_[:ys.max() + 1, :xs.max() + 1]; white = white[:ys.max() + 1, :xs.max() + 1]
    return np.stack([ink_, white])[None]          # one frame of (ink, solid)
title = decal(13, 4)
np.savez(os.path.join(ART, 'lettering.npz'), title=title,
         gameover=render("GAME OVER", 13, 108), newrec=render("NEW RECORD", 12, 108))
