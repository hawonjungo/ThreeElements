"""Turns the source skill-icon art in art/skill-icons/ into the small game icons in
Three Elements/assets/skill/ (the paths the Core skill catalog uses).

    python art/make_skill_icons.py

For each source image (any size, RGB or RGBA):
  1. Background removal: the background is whatever touches the image border and looks like the border
     (plain white, a baked-in "transparency" checkerboard, or a dark backdrop). It is flood-filled from the
     edges by colour distance to the border palette, so the artwork in the middle is never touched even where
     it shares those colours. Sources that already have real transparency keep it.
  2. The edge of the cut-out is softened by 1 px so it does not look jagged when scaled.
  3. Downscaled to ICON_SIZE with alpha-correct (premultiplied) Lanczos filtering and saved as RGBA PNG.

Only the small icons are packed into the web build; the large sources stay here, out of the game assets.
Requires Pillow (pip install pillow).
"""
from collections import deque
import os
from PIL import Image, ImageFilter

ICON_SIZE = 128          # drawn at 64 px in the D/F slots and ~40 px in the HUD / recipe list
WORK_SIZE = 512          # background detection resolution (fast, and fine enough for the edge)
THRESHOLD = 34           # colour distance to the border palette that still counts as background

ROOT = os.path.dirname(os.path.abspath(__file__))
SOURCE_DIR = os.path.join(ROOT, 'skill-icons')
TARGET_DIR = os.path.join(ROOT, '..', 'Three Elements', 'assets', 'skill')

# source file (as supplied) -> game icon file (the path in Core/Invoker.cpp)
ICONS = {
    'Cold Snap.png':   'ColdSnap.png',
    'Ghost Walk.png':  'GhostWalk.png',
    'Ice Wall.png':    'IceWall.png',
    'EMP.png':         'EMP.png',
    'Tonado.png':      'Tornado.png',
    'Alacity.png':     'Alacrity.png',
    'Sun Strike.png':  'SunStrike.png',
    'Forge Sprit.png': 'ForgeSpirit.png',
    'Meteor.png':      'Meteor.png',
    'Blast.png':       'Blast.png',
}


MAX_BG_SATURATION = 40  # max - min channel: backgrounds are white / grey / checker / dull dark, never vivid
MIN_BG_SHARE = 0.004    # a border colour must cover this share of the border to count as background
# Per-icon: share of the size whose centre disc is always artwork. Only for art whose white core is joined to a
# white background through its rays (the fill would eat the core); elsewhere it would keep background patches.
PROTECT_RADIUS = {'Blast.png': 0.28}


def border_palette(img):
    """The background's colours: frequent, dull colours along the border. Vivid colours (rays or sparks of
    the artwork that reach the edge) and rare ones are left out, so the fill cannot run into the artwork."""
    w, h = img.size
    px = img.load()
    counts = {}
    total = 0
    points = [(x, y) for x in range(w) for y in (0, 1, h - 2, h - 1)] + \
             [(x, y) for y in range(h) for x in (0, 1, w - 2, w - 1)]
    for x, y in points:
        r, g, b = px[x, y][:3]
        key = (r // 6 * 6, g // 6 * 6, b // 6 * 6)
        counts[key] = counts.get(key, 0) + 1
        total += 1
    return [c for c, n in counts.items()
            if n >= total * MIN_BG_SHARE and max(c) - min(c) <= MAX_BG_SATURATION]


def background_mask(img, protect_radius=0.0):
    """255 where the pixel is background (connected to the border and close to the border palette)."""
    w, h = img.size
    px = img.load()
    palette = border_palette(img)
    limit = THRESHOLD * THRESHOLD

    cx, cy, keep = w / 2.0, h / 2.0, (protect_radius * w) ** 2

    def is_bg(x, y):
        if (x - cx) ** 2 + (y - cy) ** 2 <= keep:
            return False
        r, g, b = px[x, y][:3]
        for pr, pg, pb in palette:
            if (r - pr) ** 2 + (g - pg) ** 2 + (b - pb) ** 2 <= limit:
                return True
        return False

    mask = Image.new('L', (w, h), 0)
    m = mask.load()
    queue = deque()
    for x in range(w):
        queue.append((x, 0)); queue.append((x, h - 1))
    for y in range(h):
        queue.append((0, y)); queue.append((w - 1, y))
    while queue:
        x, y = queue.popleft()
        if m[x, y] or not is_bg(x, y):
            continue
        m[x, y] = 255
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if 0 <= nx < w and 0 <= ny < h and not m[nx, ny]:
                queue.append((nx, ny))
    return mask


def make_icon(source_path, target_path, size=ICON_SIZE):
    src = Image.open(source_path)
    has_alpha = src.mode in ('RGBA', 'LA') or 'transparency' in src.info
    work = src.convert('RGBA').resize((WORK_SIZE, WORK_SIZE), Image.LANCZOS)
    if not has_alpha:
        bg = background_mask(work.convert('RGB'), PROTECT_RADIUS.get(os.path.basename(source_path), 0.0))
        alpha = Image.eval(bg, lambda v: 255 - v).filter(ImageFilter.GaussianBlur(1))
        work.putalpha(alpha)
    icon = work.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')
    icon.save(target_path, optimize=True)
    return icon


def main():
    for source, target in ICONS.items():
        source_path = os.path.join(SOURCE_DIR, source)
        if not os.path.exists(source_path):
            print('missing source:', source)
            continue
        icon = make_icon(source_path, os.path.join(TARGET_DIR, target))
        opaque = sum(1 for a in icon.getchannel('A').get_flattened_data() if a > 128) / (ICON_SIZE * ICON_SIZE)
        print('%-16s -> %-16s %3d%% opaque' % (source, target, opaque * 100))


if __name__ == '__main__':
    main()
