"""Makes the Android launcher icon (android/app/src/main/res/mipmap-*/ic_launcher.png) from the Deafening Blast
art (Q + W + E: all three elements), cut out like the skill icons and centred on a dark rounded tile.

    python art/make_launcher_icon.py
"""
import os
from PIL import Image, ImageDraw
import make_skill_icons as icons

SOURCE = 'Blast.png'
SIZES = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}
RES_DIR = os.path.join(icons.ROOT, '..', 'android', 'app', 'src', 'main', 'res')
BASE = 512  # drawn once at this size, then scaled down for each density


def main():
    art_path = os.path.join(icons.ROOT, 'launcher-art.tmp.png')  # make_icon writes a file; this one is temporary
    art = icons.make_icon(os.path.join(icons.SOURCE_DIR, SOURCE), art_path, BASE)
    os.remove(art_path)

    tile = Image.new('RGBA', (BASE, BASE), (0, 0, 0, 0))
    ImageDraw.Draw(tile).rounded_rectangle((0, 0, BASE - 1, BASE - 1), radius=BASE // 6, fill=(17, 20, 28, 255))
    inner = int(BASE * 0.86)
    tile.alpha_composite(art.resize((inner, inner), Image.LANCZOS), ((BASE - inner) // 2, (BASE - inner) // 2))

    for density, size in SIZES.items():
        out_dir = os.path.join(RES_DIR, 'mipmap-' + density)
        os.makedirs(out_dir, exist_ok=True)
        tile.convert('RGBa').resize((size, size), Image.LANCZOS).convert('RGBA').save(
            os.path.join(out_dir, 'ic_launcher.png'), optimize=True)
        print('mipmap-%-8s %d px' % (density, size))


if __name__ == '__main__':
    main()
