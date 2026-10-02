"""Prepares the owner's effect sheets for the game.

    python art/make_vfx.py        (needs Pillow)

Originals: art/vfx/ (full quality, never packed into the game).
  Ice Wall-spritesheet.png   1024 x 1024, 4 x 4 frames of 256 px. The owner's third sheet (2026-10-03): a wall of
                             ice crystals in plain side view, its base one level line (row 191 of a frame), a crack
                             in the ground under it, glints. The first (a single pillar, 2560 px) and the second (a
                             row of shards seen at an angle, which could not stand on the flat ground) are in the
                             git history.  ->  assets/Skills/IceWall-spritesheet.png, the same picture; a sheet
                             that already has a palette is copied as it is, any other gets a 256-colour palette.
Ghost.png and tornado_icon.png in art/vfx/ are pictures the game does not use.
"""
import os
import shutil
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def prepare(source, target, size):
    source = os.path.join(ROOT, 'art', 'vfx', source)
    path = os.path.join(ROOT, 'Three Elements', 'assets', 'Skills', target)
    image = Image.open(source)
    if image.mode == 'P' and image.size == (size, size):
        shutil.copyfile(source, path)  # exactly the owner's file
    else:
        image = image.convert('RGBA')
        if image.size != (size, size):
            image = image.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')  # premultiplied: clean edges
        image.quantize(colors=256, method=Image.FASTOCTREE, dither=Image.NONE).save(path, optimize=True)
    print('%s: %d x %d, %d KB' % (target, size, size, os.path.getsize(path) // 1024))


if __name__ == '__main__':
    prepare('Ice Wall-spritesheet.png', 'IceWall-spritesheet.png', 1024)
