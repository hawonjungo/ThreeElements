"""Builds the Immortals' sprite sheets from the owner's originals.

    python art/make_bosses.py        (needs Pillow)

Originals: art/bosses/<Name>/ - two sheets of 1024 x 1024, a 4 x 4 grid of 256 px frames, the character facing
right: one for moving (run / walk) and one for being hit, plus the concept picture. They stay here, at full
quality, and are never packed into the game.

Output: Three Elements/assets/enemies/immortals/<name>_run.png and <name>_hit.png. For each character:
  - the two sheets are brought to the same scale (some were exported at different sizes) and to the size the
    character has on screen, so the game draws every frame 1:1;
  - every frame is cut to one common cell, feet on the bottom row, the body centred;
  - the frames are mirrored: in the game the enemies come from the right and face left;
  - the sheet is stored with a 256-colour palette (a third of the size, no visible difference at this size).

The script prints the cell size and the body size of each character: the body goes into kBosses
(Three Elements/Practice/Boss.cpp), the cell size is read from the texture by the game.
"""
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, 'art', 'bosses')
OUT = os.path.join(ROOT, 'Three Elements', 'assets', 'enemies', 'immortals')
GRID = 4
ALPHA_MIN = 40   # fainter pixels (smoke, soft edges) do not count as body

# name, folder, moving sheet, its scale, hit sheet, its scale. The scales bring both sheets to one size: the size
# on screen (the player is about 106 px tall).
BOSSES = [
    ('rimefang', 'Rimefang', 'Rimefang-run.png', 1.40, 'Rimefang-hit_react.png', 1.08),
    ('cindermaw', 'Cindermaw', 'Cindermaw-run.png', 1.45, 'Cindermaw-hit_react.png', 0.85),
    ('gravehorn', 'Gravehorn', 'Gravehorn-walk.png', 1.00, 'Gravehorn-hit_react.png', 1.00),
]


def frames_of(path, scale):
    sheet = Image.open(path).convert('RGBA')
    cell = sheet.width // GRID
    out = []
    for i in range(GRID * GRID):
        frame = sheet.crop(((i % GRID) * cell, (i // GRID) * cell, (i % GRID + 1) * cell, (i // GRID + 1) * cell))
        if frame.getbbox() is None:
            continue
        if scale != 1.0:
            size = int(round(cell * scale))
            # premultiplied alpha, so the transparent pixels' colour does not bleed into the edges
            frame = frame.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')
        out.append(frame)
    return out


def body_box(frame):
    return frame.split()[3].point(lambda v: 255 if v > ALPHA_MIN else 0).getbbox()


def median(values):
    values = sorted(values)
    return values[len(values) // 2]


def measure(frames):
    boxes = [body_box(f) for f in frames]
    centre = median([(b[0] + b[2]) // 2 for b in boxes])
    feet = median([b[3] for b in boxes])
    left = max(centre - min(f.getbbox()[0] for f in frames), max(f.getbbox()[2] for f in frames) - centre)
    up = feet - min(f.getbbox()[1] for f in frames)
    down = max(f.getbbox()[3] for f in frames) - feet
    return boxes, centre, feet, left, up, down


def build(frames, centre, feet, cell_w, cell_h, below):
    sheet = Image.new('RGBA', (cell_w * GRID, cell_h * GRID), (0, 0, 0, 0))
    for i, frame in enumerate(frames):
        cell = Image.new('RGBA', (cell_w, cell_h), (0, 0, 0, 0))
        cell.paste(frame, (cell_w // 2 - centre, cell_h - below - feet))
        cell = cell.transpose(Image.FLIP_LEFT_RIGHT)
        sheet.paste(cell, ((i % GRID) * cell_w, (i // GRID) * cell_h))
    return sheet.quantize(colors=256, method=Image.FASTOCTREE, dither=Image.NONE)


def main():
    os.makedirs(OUT, exist_ok=True)
    for name, folder, run_file, run_scale, hit_file, hit_scale in BOSSES:
        run = frames_of(os.path.join(SRC, folder, run_file), run_scale)
        hit = frames_of(os.path.join(SRC, folder, hit_file), hit_scale)
        run_boxes, run_c, run_feet, run_half, run_up, run_down = measure(run)
        hit_boxes, hit_c, hit_feet, hit_half, hit_up, hit_down = measure(hit)
        below = max(run_down, hit_down)                       # a toe or a shadow under the feet line
        cell_w = 2 * max(run_half, hit_half) + 2
        cell_h = max(run_up, hit_up) + below + 1
        for kind, frames, centre, feet in (('run', run, run_c, run_feet), ('hit', hit, hit_c, hit_feet)):
            path = os.path.join(OUT, '%s_%s.png' % (name, kind))
            build(frames, centre, feet, cell_w, cell_h, below).save(path, optimize=True)
            print('%s: %d frames, cell %d x %d, feet %d px above the bottom, %d KB'
                  % (os.path.relpath(path, ROOT), len(frames), cell_w, cell_h, below, os.path.getsize(path) // 1024))
        for kind, boxes in (('run', run_boxes), ('hit', hit_boxes)):
            print('    %s body: width %d, height %d (medians)'
                  % (kind, median([b[2] - b[0] for b in boxes]), median([b[3] - b[1] for b in boxes])))


if __name__ == '__main__':
    main()
