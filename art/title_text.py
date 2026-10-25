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
a = render("TIRE", 17, 90); b = render("TESTER", 17, 90)
W = max(a.shape[1], b.shape[1]); H = a.shape[0] + 3 + b.shape[0]
title = np.zeros((H, W), bool); title[:a.shape[0], :a.shape[1]] = a; title[a.shape[0] + 3:, :b.shape[1]] = b
np.savez(os.path.join(ART, 'lettering.npz'), title=title,
         gameover=render("GAME OVER", 13, 108), newrec=render("NEW RECORD", 12, 108))
