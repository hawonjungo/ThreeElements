"""Builds every Injoker image the game and the stores use from the two source pictures in art/character/.

    python art/make_injoker.py

Sources (transparent PNGs supplied by the owner):
  art/character/injoker_character.png  the character alone          -> in-game player sprite
  art/character/Injoker.png            character, 3 orbs, magic ring -> title logo and every icon

Outputs:
  Three Elements/assets/player/injoker.png        player sprite (drawn at PLAYER_DRAW_H px high, 2x for smoothing)
  Three Elements/assets/title/injoker_logo.png    Ready-screen logo
  android/app/src/main/res/mipmap-*/ic_launcher.png  Android launcher icon (rounded dark tile)
  web/icons/icon-192.png, icon-512.png, favicon-32.png  web / Add to Home Screen icons
  art/store/icon-512.png                           Google Play store icon (full-bleed square, no rounding)

Clean-up: specks left by the background remover (tiny separate blobs) are dropped; real parts of the picture,
like the orbs and the magic ring, are much larger and kept. Requires Pillow.
"""
from collections import deque
import os
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(ROOT, '..')
CHARACTER = os.path.join(ROOT, 'character', 'injoker_character.png')
LOGO = os.path.join(ROOT, 'character', 'Injoker.png')

PLAYER_DRAW_H = 120          # must match PLAYER_DRAW_H in GameManager.h
LOGO_DRAW_H = 170            # must match TITLE_LOGO_H in GameManager.h
TILE_COLOUR = (17, 20, 28, 255)
MIN_BLOB_SHARE = 0.004       # blobs smaller than this share of all visible pixels are specks


def drop_specks(img):
    """Removes the small separate blobs (alpha > 40, 8-connected) that background removal leaves behind."""
    img = img.convert('RGBA')
    w, h = img.size
    alpha = img.getchannel('A').load()
    seen = bytearray(w * h)
    blobs = []
    total = 0
    for y in range(h):
        for x in range(w):
            if seen[y * w + x] or alpha[x, y] <= 40:
                continue
            pixels = []
            queue = deque([(x, y)])
            seen[y * w + x] = 1
            while queue:
                px, py = queue.popleft()
                pixels.append((px, py))
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        nx, ny = px + dx, py + dy
                        if 0 <= nx < w and 0 <= ny < h and not seen[ny * w + nx] and alpha[nx, ny] > 40:
                            seen[ny * w + nx] = 1
                            queue.append((nx, ny))
            blobs.append(pixels)
            total += len(pixels)
    out = img.copy()
    op = out.load()
    removed = 0
    for pixels in blobs:
        if len(pixels) < total * MIN_BLOB_SHARE:
            removed += 1
            for px, py in pixels:
                op[px, py] = (0, 0, 0, 0)
    # faint halo pixels (alpha <= 40) around removed specks are cleared too
    for y in range(h):
        for x in range(w):
            if op[x, y][3] <= 40:
                op[x, y] = (0, 0, 0, 0)
    return out, removed


def trimmed(img):
    return img.crop(img.getbbox())


def fit_height(img, height):
    w = max(1, round(img.width * height / img.height))
    return img.convert('RGBa').resize((w, height), Image.LANCZOS).convert('RGBA')


def tile(art, size, rounded, fill=0.88):
    """`art` centred on a dark square, scaled to fill `fill` of it."""
    base = 1024
    canvas = Image.new('RGBA', (base, base), (0, 0, 0, 0))
    draw = ImageDraw.Draw(canvas)
    if rounded:
        draw.rounded_rectangle((0, 0, base - 1, base - 1), radius=base // 6, fill=TILE_COLOUR)
    else:
        draw.rectangle((0, 0, base - 1, base - 1), fill=TILE_COLOUR)
    scale = fill * base / max(art.width, art.height)
    a = art.convert('RGBa').resize((round(art.width * scale), round(art.height * scale)), Image.LANCZOS).convert('RGBA')
    canvas.alpha_composite(a, ((base - a.width) // 2, (base - a.height) // 2))
    return canvas.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')


def save(img, *parts):
    path = os.path.join(REPO, *parts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path, optimize=True)
    print('%-58s %dx%d' % (os.path.join(*parts), img.width, img.height))


def main():
    character, n1 = drop_specks(Image.open(CHARACTER))
    logo, n2 = drop_specks(Image.open(LOGO))
    character, logo = trimmed(character), trimmed(logo)
    print('specks removed: character %d, logo %d' % (n1, n2))

    save(fit_height(character, PLAYER_DRAW_H * 2), 'Three Elements', 'assets', 'player', 'injoker.png')
    save(fit_height(logo, LOGO_DRAW_H * 2), 'Three Elements', 'assets', 'title', 'injoker_logo.png')

    for density, size in {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}.items():
        save(tile(logo, size, True), 'android', 'app', 'src', 'main', 'res', 'mipmap-' + density, 'ic_launcher.png')
    save(tile(logo, 192, True), 'web', 'icons', 'icon-192.png')
    save(tile(logo, 512, True), 'web', 'icons', 'icon-512.png')
    save(tile(logo, 32, True, 0.96), 'web', 'icons', 'favicon-32.png')
    save(tile(logo, 512, False), 'art', 'store', 'icon-512.png')


if __name__ == '__main__':
    main()
