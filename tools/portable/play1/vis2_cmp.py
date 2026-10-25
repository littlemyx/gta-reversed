#!/usr/bin/env python3
# MAC-VIS2: region numbers native vs Win32 mark BMPs.  usage: vis2_cmp.py <nativeDir> <win32Dir> mark1 mark2 ...   (1280x720 frames; boxes are for the Grove scene)
import sys
from PIL import Image, ImageStat
R = {'sky': (700, 0, 900, 60), 'ground': (64, 480, 480, 640), 'roof': (380, 225, 620, 245), 'hud': (992, 128, 1248, 160), 'full': (0, 0, 1280, 720)}
def mean(im, b): return tuple(round(v) for v in ImageStat.Stat(im.crop(b)).mean[:3])
A, B = sys.argv[1:3]
for m in sys.argv[3:]:
    a = Image.open(f'{A}/mark_{m}.bmp').convert('RGB'); b = Image.open(f'{B}/mark_{m}.bmp').convert('RGB')
    print(m, ' '.join(f'{k} {mean(a, bx)}/{mean(b, bx)}' for k, bx in R.items()))
