"""Shrinks the owner's large effect sheets to the size the game draws them at.

    python art/make_vfx.py        (needs Pillow)

Originals: art/vfx/ (full quality, never packed into the game).
  Ice Wall-spritesheet.png   2560 x 2560, 4 x 4 frames of 640 px  ->  assets/Skills/IceWall-spritesheet.png,
                             1024 x 1024 (frames of 256 px, like the other effect sheets), 256-colour palette.
                             The pillar does not stand in the same place in every frame of the original (it is
                             about 15 px further left from the seventh frame on), which showed as a jerk: every
                             frame is shifted so that the pillar's centre is at the same x (`steady`).
Ghost.png and tornado_icon.png in art/vfx/ are pictures the game does not use.
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def steady(image, grid, band):
    """Shifts each frame sideways so that what stands in the rows `band` (the pillar, above the ice that spreads
    on the ground) has its centre where it is in the last frame."""
    cell = image.width // grid
    frames = [image.crop(((i % grid) * cell, (i // grid) * cell, (i % grid + 1) * cell, (i // grid + 1) * cell))
              for i in range(grid * grid)]

    def centre(frame):
        box = frame.split()[3].crop((0, band[0], cell, band[1])).point(lambda v: 255 if v > 40 else 0).getbbox()
        return (box[0] + box[2]) // 2

    target = centre(frames[-1])
    out = Image.new('RGBA', image.size, (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        moved = Image.new('RGBA', (cell, cell), (0, 0, 0, 0))
        moved.paste(frame, (target - centre(frame), 0))
        out.paste(moved, ((i % grid) * cell, (i // grid) * cell))
    return out


def shrink(source, target, size, grid=0, band=None):
    image = Image.open(os.path.join(ROOT, 'art', 'vfx', source)).convert('RGBA')
    if grid:
        image = steady(image, grid, band)
    image = image.convert('RGBa')  # premultiplied: clean edges
    image = image.resize((size, size), Image.LANCZOS).convert('RGBA')
    path = os.path.join(ROOT, 'Three Elements', 'assets', 'Skills', target)
    image.quantize(colors=256, method=Image.FASTOCTREE, dither=Image.NONE).save(path, optimize=True)
    print('%s: %d x %d, %d KB' % (target, size, size, os.path.getsize(path) // 1024))


if __name__ == '__main__':
    shrink('Ice Wall-spritesheet.png', 'IceWall-spritesheet.png', 1024, grid=4, band=(60, 300))
