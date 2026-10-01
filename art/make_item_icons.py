"""Pixel icons for the shop items (GAMEPLAY_SPEC.md §27), the OVERLORD materials (§28) and the runes (§26), drawn
from scratch in the spirit of the Dota 2 items.

Each icon is a 16 x 16 pixel drawing on a dark tile, scaled x3 (nearest) to 48 x 48 and saved as
"Three Elements/assets/items/<key><level>.png" (level 1 and 2 for permanent items, 1 for consumables), and as
"mat_<name>.png" / "rune_<name>.png" for the materials and the runes.
Run: python art/make_item_icons.py   (needs Pillow)
"""
import math
import os
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'Three Elements', 'assets', 'items')
N = 16
SCALE = 3


class Canvas:
    def __init__(self, bg_top, bg_bottom, frame):
        self.px = [[None] * N for _ in range(N)]
        for y in range(N):  # a dark vertical gradient tile with a coloured frame, like an item slot
            t = y / (N - 1)
            c = tuple(int(bg_top[k] * (1 - t) + bg_bottom[k] * t) for k in range(3))
            for x in range(N):
                self.px[y][x] = c
        for i in range(N):
            for (x, y) in ((i, 0), (i, N - 1), (0, i), (N - 1, i)):
                self.px[y][x] = frame

    def set(self, x, y, c):
        if 1 <= x < N - 1 and 1 <= y < N - 1:
            self.px[y][x] = c

    def line(self, x0, y0, x1, y1, c):
        steps = max(abs(x1 - x0), abs(y1 - y0), 1)
        for i in range(steps + 1):
            self.set(round(x0 + (x1 - x0) * i / steps), round(y0 + (y1 - y0) * i / steps), c)

    def rect(self, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.set(x, y, c)

    def disc(self, cx, cy, r, c):
        for y in range(N):
            for x in range(N):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    self.set(x, y, c)

    def ring(self, cx, cy, r, c):
        for y in range(N):
            for x in range(N):
                d = math.hypot(x - cx, y - cy)
                if r - 0.6 <= d <= r + 0.4:
                    self.set(x, y, c)

    def save(self, name):
        img = Image.new('RGBA', (N, N))
        for y in range(N):
            for x in range(N):
                img.putpixel((x, y), self.px[y][x] + (255,))
        img = img.resize((N * SCALE, N * SCALE), Image.NEAREST)
        img.save(os.path.join(OUT, name + '.png'))


DARK = (18, 20, 30)


def blink(level):
    c = Canvas((40, 30, 70), (14, 12, 28), (150, 110, 220) if level == 1 else (90, 230, 200))
    blade = (200, 215, 240) if level == 1 else (150, 255, 230)
    edge = (255, 255, 255)
    c.line(4, 11, 12, 3, blade)
    c.line(5, 11, 12, 4, blade)
    c.line(4, 10, 11, 3, edge)
    c.line(3, 12, 5, 10, (120, 70, 170))   # guard
    c.line(2, 10, 6, 13, (120, 70, 170))
    c.line(2, 13, 3, 12, (90, 50, 60))     # grip
    c.set(1, 14, (90, 50, 60))
    spark = (255, 240, 150) if level == 1 else (200, 255, 240)
    for (x, y) in ((12, 7), (13, 8), (11, 8), (12, 9)):
        c.set(x, y, spark)
    if level == 2:  # motion lines
        c.line(2, 5, 6, 5, (90, 200, 180))
        c.line(1, 7, 4, 7, (90, 200, 180))
    return c


def refresher(level):
    c = Canvas((20, 40, 70), (10, 16, 32), (90, 170, 255) if level == 1 else (255, 210, 90))
    c.disc(7.5, 7.5, 5.2, (40, 110, 210) if level == 1 else (40, 170, 210))
    c.disc(6.5, 6.5, 3.2, (80, 160, 250) if level == 1 else (90, 220, 240))
    swirl = (230, 245, 255)
    for i in range(10):  # a circular arrow
        a = math.radians(40 + i * 28)
        c.set(round(7.5 + 3.2 * math.cos(a)), round(7.5 + 3.2 * math.sin(a)), swirl)
    c.set(10, 4, swirl); c.set(11, 5, swirl); c.set(9, 5, swirl)
    if level == 2:
        c.ring(7.5, 7.5, 6.3, (255, 210, 90))
    return c


def euls(level):
    c = Canvas((30, 55, 70), (12, 20, 28), (120, 220, 240) if level == 1 else (230, 250, 255))
    c.line(4, 14, 9, 5, (150, 110, 60))    # the staff
    c.line(5, 14, 10, 5, (110, 80, 40))
    wind = (150, 230, 250) if level == 1 else (240, 255, 255)
    for i, w in enumerate((5, 4, 3, 2)):   # a small cyclone on top
        y = 2 + i
        c.line(10 - w // 2 + i // 2, y, 10 + w - w // 2 + i // 2, y, wind)
    if level == 2:  # wings
        c.line(1, 6, 5, 9, (200, 240, 255))
        c.line(1, 8, 5, 10, (200, 240, 255))
        c.line(11, 11, 14, 8, (200, 240, 255))
    return c


def bkb(level):
    c = Canvas((70, 20, 20), (26, 8, 8), (240, 190, 60) if level == 1 else (255, 240, 140))
    gold = (240, 190, 60) if level == 1 else (255, 225, 110)
    c.rect(7, 2, 8, 10, gold)              # the blade
    c.set(7, 1, gold); c.set(8, 1, gold)
    c.rect(4, 10, 11, 11, (60, 50, 50))    # black guard
    c.rect(7, 12, 8, 14, (30, 26, 26))     # grip
    c.line(7, 3, 7, 9, (255, 245, 200))
    if level == 2:
        for (x, y) in ((5, 4), (10, 4), (4, 7), (11, 7)):
            c.set(x, y, (255, 250, 200))
    return c


def midas(level):
    c = Canvas((70, 50, 15), (28, 18, 6), (240, 200, 70) if level == 1 else (255, 240, 150))
    g = (235, 190, 60) if level == 1 else (255, 225, 90)
    d = (180, 130, 30)
    c.rect(5, 7, 10, 12, g)                # the palm
    for x in (5, 7, 9):                    # fingers
        c.rect(x, 3 if x == 7 else 4, x, 6, g)
        c.set(x + 1, 5, d)
    c.rect(11, 8, 12, 9, g)                # thumb
    c.rect(5, 13, 10, 14, d)               # cuff
    if level == 2:
        c.disc(12, 3, 1.6, (255, 240, 120))
        c.set(12, 3, (200, 150, 30))
    return c


def octarine(level):
    c = Canvas((30, 24, 50), (12, 10, 20), (255, 150, 60) if level == 1 else (120, 240, 255))
    for y in range(3, 13):                 # a diamond gem, orange left / cyan right
        half = 5 - abs(y - 7.5) if y <= 7 else 5 - abs(y - 7.5)
        half = int(round(max(0, half)))
        for x in range(8 - half, 8 + half):
            c.set(x, y, (255, 140, 40) if x < 8 else (80, 210, 240))
    c.set(6, 5, (255, 230, 180)); c.set(9, 5, (220, 255, 255))
    if level == 2:
        c.ring(7.5, 7.5, 6.4, (200, 240, 255))
    return c


def aghanim(level):
    c = Canvas((20, 30, 70), (8, 10, 30), (90, 140, 255) if level == 1 else (170, 210, 255))
    if level == 1:
        c.line(5, 14, 9, 6, (200, 160, 60))  # the scepter
        c.line(6, 14, 10, 6, (160, 120, 40))
        c.disc(10.5, 4.5, 2.4, (70, 130, 255))
        c.set(10, 3, (220, 235, 255))
    else:  # Aghanim's Blessing: a floating crystal
        for y in range(2, 14):
            half = int(round(3.5 - abs(y - 7.5) * 0.6))
            for x in range(8 - max(half, 0), 8 + max(half, 0)):
                c.set(x, y, (110, 170, 255) if x < 8 else (160, 210, 255))
        c.set(7, 4, (240, 250, 255))
        c.ring(7.5, 7.5, 6.3, (140, 190, 255))
    return c


def salve(level):
    c = Canvas((20, 50, 25), (8, 20, 10), (120, 220, 100))
    c.rect(6, 6, 9, 13, (90, 200, 90))     # the flask
    c.rect(5, 8, 10, 12, (90, 200, 90))
    c.rect(7, 3, 8, 5, (200, 200, 190))    # neck
    c.rect(6, 2, 9, 2, (150, 110, 70))     # cork
    c.set(6, 8, (200, 255, 200))
    return c


def cheese(level):
    c = Canvas((60, 50, 15), (24, 20, 6), (255, 220, 80))
    y0 = 5
    for y in range(y0, 13):                # a wedge
        w = y - y0 + 3
        c.line(3, y, 3 + w, y, (250, 210, 70))
    c.line(3, 12, 13, 12, (210, 160, 40))
    for (x, y) in ((5, 9), (8, 11), (6, 11)):
        c.set(x, y, (200, 150, 40))
    return c


def smoke(level, dark):
    c = Canvas((40, 40, 50) if not dark else (35, 20, 50), (14, 14, 18) if not dark else (12, 6, 20),
               (170, 170, 190) if not dark else (180, 110, 230))
    col = (170, 175, 190) if not dark else (130, 90, 180)
    hi = (215, 220, 230) if not dark else (190, 150, 230)
    for (x, y, r) in ((5, 9, 3), (9, 8, 3.5), (11, 11, 2.5), (7, 11, 2.5)):
        c.disc(x, y, r, col)
    c.disc(8, 6.5, 1.8, hi)
    c.disc(4.5, 8, 1.2, hi)
    return c


def point_booster():
    c = Canvas((45, 25, 70), (16, 8, 30), (200, 130, 255))
    c.disc(7.5, 7.5, 4.6, (120, 60, 200))      # the gem
    c.disc(7.0, 7.0, 3.2, (160, 100, 240))
    c.disc(6.2, 6.2, 1.4, (225, 190, 255))
    c.set(5, 5, (255, 255, 255))
    gold = (240, 200, 80)
    for (x, y) in ((7, 2), (8, 2), (2, 7), (2, 8), (13, 7), (13, 8), (7, 13), (8, 13)):  # four claws of the setting
        c.set(x, y, gold)
    for (x, y) in ((4, 4), (11, 4), (4, 11), (11, 11)):
        c.set(x, y, (180, 140, 50))
    return c


def mystic_staff():
    c = Canvas((18, 45, 70), (8, 16, 30), (110, 200, 255))
    c.line(3, 14, 9, 6, (120, 90, 170))        # the shaft
    c.line(4, 14, 10, 6, (80, 60, 130))
    c.ring(10.5, 4.5, 2.6, (150, 220, 255))    # the crescent head ...
    c.set(8, 5, (18, 45, 70)); c.set(8, 6, (18, 45, 70))
    c.disc(10.5, 4.5, 1.2, (230, 250, 255))    # ... around a bright crystal
    for (x, y) in ((13, 2), (6, 2), (13, 8)):
        c.set(x, y, (200, 240, 255))
    return c


def sacred_relic():
    c = Canvas((75, 55, 15), (30, 20, 6), (255, 225, 110))
    gold = (255, 215, 90)
    c.rect(7, 2, 8, 10, gold)                  # a broad golden blade
    c.set(7, 1, (255, 245, 190)); c.set(8, 1, gold)
    c.line(7, 2, 7, 10, (255, 245, 190))
    c.rect(4, 10, 11, 11, (220, 170, 50))      # winged guard
    c.set(3, 9, (220, 170, 50)); c.set(12, 9, (220, 170, 50))
    c.set(2, 8, (255, 225, 110)); c.set(13, 8, (255, 225, 110))
    c.rect(7, 12, 8, 13, (150, 100, 40))       # grip
    c.rect(6, 14, 9, 14, (255, 215, 90))       # pommel
    c.set(10, 3, (255, 250, 210)); c.set(11, 4, (255, 250, 210))  # a glint
    return c


def rune(name):
    colours = {'regen': ((20, 60, 30), (8, 22, 12), (110, 230, 120)),
               'frost': ((20, 50, 75), (8, 18, 30), (140, 220, 255)),
               'double': ((20, 30, 80), (8, 10, 34), (90, 140, 255)),
               'bounty': ((70, 55, 15), (26, 20, 6), (255, 210, 80)),
               'shield': ((25, 55, 65), (10, 20, 26), (120, 230, 240))}
    top, bottom, col = colours[name]
    c = Canvas(top, bottom, col)
    hi = tuple(min(255, v + 70) for v in col)
    c.ring(7.5, 7.5, 6.2, tuple(v // 2 for v in col))  # every rune sits in a stone ring
    if name == 'regen':        # a cross
        c.rect(7, 4, 8, 11, col)
        c.rect(4, 7, 11, 8, col)
        c.set(7, 4, hi); c.set(4, 7, hi)
    elif name == 'frost':      # a snowflake
        c.line(7, 3, 7, 12, col); c.line(3, 7, 12, 7, col)
        c.line(4, 4, 11, 11, col); c.line(11, 4, 4, 11, col)
        c.set(7, 7, hi); c.set(8, 8, hi)
    elif name == 'double':     # two chevrons
        for dy in (0, 4):
            c.line(4, 8 + dy - 2, 7, 5 + dy - 2, col)
            c.line(8, 5 + dy - 2, 11, 8 + dy - 2, col)
            c.line(4, 9 + dy - 2, 7, 6 + dy - 2, hi if dy == 0 else col)
            c.line(8, 6 + dy - 2, 11, 9 + dy - 2, hi if dy == 0 else col)
    elif name == 'bounty':     # a coin
        c.disc(7.5, 7.5, 3.6, col)
        c.disc(7.5, 7.5, 2.2, (200, 150, 40))
        c.rect(7, 6, 8, 9, hi)
    else:                      # a shield
        c.rect(5, 4, 10, 8, col)
        c.rect(6, 9, 9, 10, col)
        c.rect(7, 11, 8, 11, col)
        c.line(7, 4, 7, 10, hi)
    return c


EXTRA = [
    ('mat_pointbooster', point_booster()),
    ('mat_mysticstaff', mystic_staff()),
    ('mat_sacredrelic', sacred_relic()),
    ('rune_regen', rune('regen')),
    ('rune_frost', rune('frost')),
    ('rune_double', rune('double')),
    ('rune_bounty', rune('bounty')),
    ('rune_shield', rune('shield')),
]

ICONS = [
    ('blink', [blink(1), blink(2)]),
    ('refresher', [refresher(1), refresher(2)]),
    ('euls', [euls(1), euls(2)]),
    ('bkb', [bkb(1), bkb(2)]),
    ('midas', [midas(1), midas(2)]),
    ('octarine', [octarine(1), octarine(2)]),
    ('aghanim', [aghanim(1), aghanim(2)]),
    ('salve', [salve(1)]),
    ('cheese', [cheese(1)]),
    ('smoke', [smoke(1, False)]),
    ('greatersmoke', [smoke(1, True)]),
]

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for key, levels in ICONS:
        for i, canvas in enumerate(levels):
            canvas.save('%s%d' % (key, i + 1))
    for key, canvas in EXTRA:
        canvas.save(key)
    print('icons written to', OUT)
