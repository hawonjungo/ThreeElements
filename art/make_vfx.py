"""Prepares the owner's effect sheets for the game.

    python art/make_vfx.py        (needs Pillow)

Originals: art/vfx/ (full quality, never packed into the game).
  Ice Wall-spritesheet.png   1024 x 1024, 4 x 4 frames of 256 px (the owner's second sheet, 2026-10-03: a row of ice
                             shards seen at an angle, glinting; the first one, a single pillar of 2560 x 2560, is
                             in the git history)  ->  assets/Skills/IceWall-spritesheet.png, the same size with a
                             256-colour palette (a third of the bytes).
Ghost.png and tornado_icon.png in art/vfx/ are pictures the game does not use.
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def prepare(source, target, size):
    image = Image.open(os.path.join(ROOT, 'art', 'vfx', source)).convert('RGBA')
    if image.size != (size, size):
        image = image.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')  # premultiplied: clean edges
    path = os.path.join(ROOT, 'Three Elements', 'assets', 'Skills', target)
    image.quantize(colors=256, method=Image.FASTOCTREE, dither=Image.NONE).save(path, optimize=True)
    print('%s: %d x %d, %d KB' % (target, size, size, os.path.getsize(path) // 1024))


if __name__ == '__main__':
    prepare('Ice Wall-spritesheet.png', 'IceWall-spritesheet.png', 1024)
