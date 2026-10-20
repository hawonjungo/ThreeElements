"""Builds the images of the website (web/site/media) from the originals.

  python art/make_site_media.py        (needs Pillow)

Sources:
  art/store/Elemental Wizard Feature Banner.png   key art (1024 x 512): the hero, and the link preview (og.jpg)
  web/icons/icon-512.png, art/store/icon-512.png   the app icon, rounded / full square (art/make_injoker.py makes
                                                   both): header and favicons / the iPhone home-screen icon
  art/store/web-shots/*.png                        screenshots of the web build, taken at 2x in Chrome
                                                   (928 x 544 game canvas drawn at 1856 x 1088 inside a 2 px border)
  Three Elements/assets/skill/*.png                the ten skill icons
  Three Elements/assets/enemies/immortals/*_run.png the Immortals (4 x 4 sheets; the first frame is used)

The screenshots are stored at the game's own 928 x 544 (an exact halving of the 2x capture, so nothing is lost)
and shown enlarged with image-rendering: pixelated on the page.
"""
import os
import shutil
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STORE = os.path.join(ROOT, 'art', 'store')
ASSETS = os.path.join(ROOT, 'Three Elements', 'assets')
OUT = os.path.join(ROOT, 'web', 'site', 'media')

SHOTS = ['menu', 'play', 'play_cast', 'immortal_tornado', 'immortal_combo', 'immortals_list', 'recipes', 'shop']
SKILLS = ['ColdSnap', 'GhostWalk', 'IceWall', 'Tornado', 'Blast', 'ForgeSpirit', 'EMP', 'Alacrity', 'Meteor',
          'SunStrike']
IMMORTALS = ['rimefang', 'cindermaw', 'gravehorn', 'voltara', 'hollowking']
GRID = 4


def save_jpg(image, name, quality=86):
    image.convert('RGB').save(os.path.join(OUT, name), quality=quality, optimize=True, progressive=True)


def main():
    os.makedirs(os.path.join(OUT, 'shots'), exist_ok=True)
    os.makedirs(os.path.join(OUT, 'skills'), exist_ok=True)
    os.makedirs(os.path.join(OUT, 'immortals'), exist_ok=True)

    # key art: the hero as it is, and a 1200 x 630 link preview cut from its middle
    banner = Image.open(os.path.join(STORE, 'Elemental Wizard Feature Banner.png')).convert('RGB')
    save_jpg(banner, 'hero.jpg', 88)
    scale = 630 / banner.height
    wide = banner.resize((round(banner.width * scale), 630), Image.LANCZOS)
    left = (wide.width - 1200) // 2
    save_jpg(wide.crop((left, 0, left + 1200, 630)), 'og.jpg', 86)

    # rounded for the page itself; iOS rounds the home-screen icon on its own and shows transparency as black
    icon = Image.open(os.path.join(ROOT, 'web', 'icons', 'icon-512.png')).convert('RGBA')
    for size in (32, 192):
        icon.resize((size, size), Image.LANCZOS).save(os.path.join(OUT, 'icon-%d.png' % size), optimize=True)
    square = Image.open(os.path.join(STORE, 'icon-512.png')).convert('RGB')
    square.resize((180, 180), Image.LANCZOS).save(os.path.join(OUT, 'icon-180.png'), optimize=True)

    for name in SHOTS:
        shot = Image.open(os.path.join(STORE, 'web-shots', name + '.png')).convert('RGB')
        border = (shot.width - 1856) // 2
        shot = shot.crop((border, border, border + 1856, border + 1088)).resize((928, 544), Image.NEAREST)
        shot.save(os.path.join(OUT, 'shots', name + '.png'), optimize=True)

    for name in SKILLS:
        shutil.copyfile(os.path.join(ASSETS, 'skill', name + '.png'), os.path.join(OUT, 'skills', name + '.png'))

    for name in IMMORTALS:
        sheet = Image.open(os.path.join(ASSETS, 'enemies', 'immortals', name + '_run.png')).convert('RGBA')
        cell_w, cell_h = sheet.width // GRID, sheet.height // GRID
        frame = sheet.crop((0, 0, cell_w, cell_h))
        frame = frame.crop(frame.getbbox())
        frame.save(os.path.join(OUT, 'immortals', name + '.png'), optimize=True)

    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(OUT) for f in fs)
    print('web/site/media: %.0f KB' % (total / 1024))


if __name__ == '__main__':
    main()
