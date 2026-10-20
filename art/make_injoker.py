"""Builds every Injoker image the game and the stores use from the two source pictures in art/character/.

    python art/make_injoker.py

Sources (supplied by the owner):
  art/character/injoker_character.png  the character alone          -> in-game player sprite
  art/character/Injoker.png            character, 3 orbs, magic ring -> title logo, old feature graphic
  art/store/Gaming App Icon Character.png  the app icon (820 px, 2026-10-20) -> every icon. It comes with black
                                       rounded corners and a 1 px white line on two edges: both are removed (the
                                       corners filled with the colours around them) to get a clean full square.

Outputs:
  Three Elements/assets/player/injoker.png        player sprite (drawn at PLAYER_DRAW_H px high, 2x for smoothing)
  Three Elements/assets/title/injoker_logo.png    Ready-screen logo
  android/app/src/main/res/mipmap-*/ic_launcher.png  Android 7 launcher icon (rounded tile)
  android/app/src/main/res/mipmap-*/ic_launcher_foreground.png, ic_launcher_background.png  Android 8+ adaptive icon
  web/icons/icon-192.png, icon-512.png, favicon-32.png  web / Add to Home Screen icons (rounded)
  art/store/icon-512.png                           Google Play store icon (full-bleed square, Play rounds it)

Clean-up: specks left by the background remover (tiny separate blobs) are dropped; real parts of the picture,
like the orbs and the magic ring, are much larger and kept. Requires Pillow.
"""
from collections import deque
import os
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.join(ROOT, '..')
CHARACTER = os.path.join(ROOT, 'character', 'injoker_character.png')
LOGO = os.path.join(ROOT, 'character', 'Injoker.png')
APP_ICON = os.path.join(ROOT, 'store', 'Gaming App Icon Character.png')

PLAYER_DRAW_H = 120          # must match PLAYER_DRAW_H in GameManager.h
LOGO_DRAW_H = 170            # must match TITLE_LOGO_H in GameManager.h
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


def clean_icon(path):
    """The owner's app icon as a clean full square: the 1 px white line on the bottom and right edges cut off, the
    black rounded corners filled with the colours around them (a normalised blur of the rest of the picture)."""
    img = Image.open(path).convert('RGB')
    side = min(img.width, img.height) - 1
    img = img.crop((0, 0, side, side))
    # corner mask: the dark pixels connected to each corner, grown a little to take the anti-aliased edge too
    mask = Image.new('L', img.size, 0)
    px, mp = img.load(), mask.load()
    for start in ((0, 0), (side - 1, 0), (0, side - 1), (side - 1, side - 1)):
        todo = deque([start])
        while todo:
            x, y = todo.popleft()
            if not (0 <= x < side and 0 <= y < side) or mp[x, y] or sum(px[x, y]) > 120:
                continue
            mp[x, y] = 255
            todo.extend(((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)))
    mask = mask.filter(ImageFilter.MaxFilter(7))
    mp = mask.load()
    keep = ImageChops.invert(mask)
    # normalised convolution: blur(picture * keep) / blur(keep) fills the corners from their surroundings
    radius = side // 12
    weighted = Image.composite(img, Image.new('RGB', img.size), keep).filter(ImageFilter.GaussianBlur(radius))
    weight = keep.filter(ImageFilter.GaussianBlur(radius))
    wp, ww = weighted.load(), weight.load()
    fill = Image.new('RGB', img.size)
    fp = fill.load()
    for y in range(side):
        for x in range(side):
            if mp[x, y]:
                w = max(ww[x, y], 1) / 255
                fp[x, y] = tuple(min(255, round(c / w)) for c in wp[x, y])
    return Image.composite(fill, img, mask.filter(ImageFilter.GaussianBlur(2))).convert('RGBA')


def rounded(square, size, radius_share=1 / 6):
    """`square` scaled to `size` with transparent rounded corners (the web and Android 7 icons)."""
    base = 1024
    big = square.resize((base, base), Image.LANCZOS)
    mask = Image.new('L', (base, base), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, base - 1, base - 1), radius=round(base * radius_share), fill=255)
    big.putalpha(mask)
    return big.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA')


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

    # every icon from the owner's app icon (2026-10-20); the old tiles of Injoker.png are no longer used
    icon = clean_icon(APP_ICON)
    for density, size in {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}.items():
        save(rounded(icon, size), 'android', 'app', 'src', 'main', 'res', 'mipmap-' + density, 'ic_launcher.png')
    save(rounded(icon, 192), 'web', 'icons', 'icon-192.png')
    save(rounded(icon, 512), 'web', 'icons', 'icon-512.png')
    save(rounded(icon, 32, 1 / 8), 'web', 'icons', 'favicon-32.png')
    save(icon.resize((512, 512), Image.LANCZOS), 'art', 'store', 'icon-512.png')

    # Adaptive launcher icon (Android 8+): both layers are 108 dp and the launcher shows the centre 72 dp through its
    # mask (circle, squircle...). The picture fills exactly that centre 72 dp (foreground); the background layer is
    # the same picture enlarged and blurred, seen only at the edges when the launcher moves or zooms the icon.
    for density, size in {'mdpi': 108, 'hdpi': 162, 'xhdpi': 216, 'xxhdpi': 324, 'xxxhdpi': 432}.items():
        inner = round(size * 72 / 108)
        fg = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        fg.alpha_composite(icon.resize((inner, inner), Image.LANCZOS), ((size - inner) // 2, (size - inner) // 2))
        save(fg, 'android', 'app', 'src', 'main', 'res', 'mipmap-' + density, 'ic_launcher_foreground.png')
        bg = icon.resize((size, size), Image.LANCZOS).filter(ImageFilter.GaussianBlur(size / 18)).convert('RGB')
        save(bg, 'android', 'app', 'src', 'main', 'res', 'mipmap-' + density, 'ic_launcher_background.png')

    save(feature_graphic(logo), 'art', 'store', 'feature-graphic-1024x500.png')


def pixel_font():
    """The game's own 5x7 font, read from PixelText.cpp so the store art uses the same letters as the game."""
    import re
    src = open(os.path.join(REPO, 'Three Elements', 'PixelText.cpp'), encoding='utf-8').read()
    glyphs = {}
    for ch, rows in re.findall(r"\{\s*'(.)',\s*\{([^}]*)\}", src):
        glyphs[ch] = [int(r.strip(), 2) for r in rows.split(',')]
    return glyphs


def draw_pixel_text(img, text, x, y, scale, colour, shadow=True):
    glyphs = pixel_font()
    d = ImageDraw.Draw(img)
    for pass_, (dx, col) in enumerate(((scale, (0, 0, 0, 255)), (0, colour)) if shadow else ((0, colour),)):
        for i, ch in enumerate(text):
            rows = glyphs.get(ch, glyphs.get('?'))
            for r, bits in enumerate(rows):
                for c in range(5):
                    if bits & (1 << (4 - c)):
                        px, py = x + (i * 6 + c) * scale + dx, y + r * scale + dx
                        d.rectangle((px, py, px + scale - 1, py + scale - 1), fill=col)


def feature_graphic(logo):
    """1024 x 500 Google Play feature graphic: the game's forest, the Injoker art, the name and a tagline."""
    w, h = 1024, 500
    bg_dir = os.path.join(REPO, 'Three Elements', 'assets', 'background')
    layers = sorted(f for f in os.listdir(bg_dir) if f.endswith('.png'))
    layers.sort(key=lambda f: int(f.split('_')[1][:4]), reverse=True)  # Layer_0011 (sky) first, 0000 last
    canvas = Image.new('RGBA', (w, h), (10, 12, 20, 255))
    for f in layers:
        layer = Image.open(os.path.join(bg_dir, f)).convert('RGBA')   # 928 x 793
        scale = w / layer.width
        layer = layer.resize((w, round(layer.height * scale)), Image.NEAREST)
        canvas.alpha_composite(layer, (0, h - layer.height + 40))
    shade = Image.new('RGBA', (w, h), (8, 10, 18, 120))
    canvas.alpha_composite(shade)
    art = fit_height(logo, 440)
    canvas.alpha_composite(art, (60, (h - art.height) // 2))
    gold, white = (255, 210, 90, 255), (235, 235, 240, 255)
    draw_pixel_text(canvas, 'INJOKER', 470, 150, 12, gold)
    draw_pixel_text(canvas, 'INVOKE THE ELEMENTS.', 474, 270, 4, white)
    draw_pixel_text(canvas, 'BEAT THE CLOCK.', 474, 310, 4, white)
    return canvas.convert('RGB')


if __name__ == '__main__':
    main()
