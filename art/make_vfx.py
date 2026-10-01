"""Shrinks the owner's large effect sheets to the size the game draws them at.

    python art/make_vfx.py        (needs Pillow)

Originals: art/vfx/ (full quality, never packed into the game).
  Ice Wall-spritesheet.png   2560 x 2560, 4 x 4 frames of 640 px  ->  assets/Skills/IceWall-spritesheet.png,
                             1024 x 1024 (frames of 256 px, like the other effect sheets), 256-colour palette.
Ghost.png and tornado_icon.png in art/vfx/ are pictures the game does not use.
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def shrink(source, target, size):
    image = Image.open(os.path.join(ROOT, 'art', 'vfx', source)).convert('RGBa')  # premultiplied: clean edges
    image = image.resize((size, size), Image.LANCZOS).convert('RGBA')
    path = os.path.join(ROOT, 'Three Elements', 'assets', 'Skills', target)
    image.quantize(colors=256, method=Image.FASTOCTREE, dither=Image.NONE).save(path, optimize=True)
    print('%s: %d x %d, %d KB' % (target, size, size, os.path.getsize(path) // 1024))


if __name__ == '__main__':
    shrink('Ice Wall-spritesheet.png', 'IceWall-spritesheet.png', 1024)
