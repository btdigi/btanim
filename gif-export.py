#!/usr/bin/env python3

raster = 32
border = 3
rotate = 0

from PIL import Image, ImageDraw
from itertools import count
from glob import glob

def rot(i, j):
    if rotate == 0:
        return i, j
    elif rotate == 1:
        return j, 7-i
    elif rotate == 2:
        return 7-i, 7-j
    elif rotate == 3:
        return 7-j, i
    else:
        assert False

def unique_file():
    used = set(glob('anim*.gif'))
    for i in count(1):
        fn = 'anim{:04d}.gif'.format(i)
        if fn not in used:
            return fn

while True:
  print("Paste animation data received from Arduino & press Enter:")
  dt, di = '', 'x'
  while di:
    di = input().strip()
    dt += di
  if dt.startswith('load:'):
      dt = dt[5:]
  assert len(dt) % 16 == 0
  fc = len(dt) // 16
  frames = []
  colorBg = (0, 0, 0)
  colorZero = (48, 48, 48)
  colorOne = (255, 0, 0)
  for f in range(fc):
      frame = Image.new('RGB', (8*raster, 8*raster), colorBg)
      fd = ImageDraw.Draw(frame)
      for j in range(8):
          ch = int(dt[16*f+2*j:16*f+2*j+2], 16)
          for i in range(8):
              px = (ch << i) & 0x80 == 0x80
              ir, jr = rot(i, j)
              fd.ellipse((ir*raster+border, jr*raster+border,
                (ir+1)*raster-1-border, (jr+1)*raster-1-border),
                fill=(colorOne if px else colorZero))
      frames.append(frame.convert("P", palette=Image.ADAPTIVE))
  fn = unique_file()
  frames[0].save(fn, 'gif', append_images=frames[1:],
    save_all=True, duration=200, loop=0)
  print("Animation saved to {}".format(fn))
  print()
