#!/usr/bin/env python3
"""Dibuja los iconos, punteros y la animacion de carga de EonSurf (tema oscuro).

Se dibuja a 4x y se reduce, para que los bordes queden suaves.
Uso: python tools/make-theme.py  (escribe en port/ps3/theme/)
"""
import math
import os
from PIL import Image, ImageDraw

S = 4                      # sobremuestreo
OUT = os.path.join(os.path.dirname(__file__), '..', 'port', 'ps3', 'theme')
BAR = (27, 31, 42, 255)    # fondo de la barra (#1B1F2A), igual que FB_FRAME_COLOUR
FG = (232, 236, 242, 255)  # icono activo
DIM = (98, 108, 125, 255)  # icono desactivado
ACCENT = (0, 200, 255, 255)
ICON = 40


def canvas(w, h, bg=(0, 0, 0, 0)):
    return Image.new('RGBA', (w * S, h * S), bg)


def save(img, path, w, h):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.resize((w, h), Image.LANCZOS).save(path)


def line(d, pts, col, width):
    pts = [(x * S, y * S) for x, y in pts]
    d.line(pts, fill=col, width=int(width * S), joint='curve')
    r = width * S / 2
    for x, y in (pts[0], pts[-1]):
        d.ellipse((x - r, y - r, x + r, y + r), fill=col)


def arc(d, cx, cy, r, a0, a1, col, width):
    d.arc(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), a0, a1,
          fill=col, width=int(width * S))
    for a in (a0, a1):
        x = cx + r * math.cos(math.radians(a)) - 0.0
        y = cy + r * math.sin(math.radians(a))
        # el trazo del arco queda hacia adentro: centrar la punta redonda
        rr = r - width / 2
        x = cx + rr * math.cos(math.radians(a))
        y = cy + rr * math.sin(math.radians(a))
        q = width * S / 2
        d.ellipse((x * S - q, y * S - q, x * S + q, y * S + q), fill=col)


def icon_back(d, c):
    line(d, [(27, 20), (13, 20)], c, 3)
    line(d, [(19, 13), (12.5, 20), (19, 27)], c, 3)


def icon_forward(d, c):
    line(d, [(13, 20), (27, 20)], c, 3)
    line(d, [(21, 13), (27.5, 20), (21, 27)], c, 3)


def icon_reload(d, c):
    arc(d, 20, 20, 9.5, -60, 250, c, 3)
    x, y = 20 + 8 * math.cos(math.radians(-60)), 20 + 8 * math.sin(math.radians(-60))
    d.polygon([((x + 4.5) * S, (y - 4) * S), ((x + 5) * S, (y + 4.5) * S),
               ((x - 3.5) * S, (y + 3) * S)], fill=c)


def icon_stop(d, c):
    line(d, [(14, 14), (26, 26)], c, 3)
    line(d, [(26, 14), (14, 26)], c, 3)


def icon_home(d, c):
    line(d, [(11, 20), (20, 12), (29, 20)], c, 3)
    line(d, [(14, 18), (14, 28), (26, 28), (26, 18)], c, 3)


def icon_history(d, c):
    arc(d, 20, 20, 10, 0, 360, c, 3)
    line(d, [(20, 14), (20, 20.5), (24.5, 23)], c, 3)


def make_icons():
    draw = {'back': icon_back, 'forward': icon_forward, 'reload': icon_reload,
            'stop': icon_stop, 'home': icon_home, 'history': icon_history}
    for name, fn in draw.items():
        for suffix, col in (('', FG), ('_g', DIM)):
            img = canvas(ICON, ICON)
            fn(ImageDraw.Draw(img), col)
            save(img, os.path.join(OUT, 'icons', name + suffix + '.png'), ICON, ICON)
    # flechas de las barras de desplazamiento (14 px)
    for name, pts in (('scrollu', [(4, 9), (7, 5), (10, 9)]),
                      ('scrolld', [(4, 5), (7, 9), (10, 5)]),
                      ('scrolll', [(9, 4), (5, 7), (9, 10)]),
                      ('scrollr', [(5, 4), (9, 7), (5, 10)])):
        img = canvas(14, 14)
        line(ImageDraw.Draw(img), pts, DIM, 1.6)
        save(img, os.path.join(OUT, 'icons', name + '.png'), 14, 14)
    # teclado en pantalla (no se usa en PS3, pero el Makefile lo pide)
    img = canvas(14, 14)
    ImageDraw.Draw(img).rounded_rectangle((2 * S, 4 * S, 12 * S, 10 * S), S, outline=DIM, width=S)
    save(img, os.path.join(OUT, 'icons', 'osk.png'), 14, 14)


def make_throbber():
    # anillo con un arco de color que gira; throbber0 = quieto
    for i in range(9):
        img = canvas(ICON, ICON)
        d = ImageDraw.Draw(img)
        arc(d, 20, 20, 11, 0, 360, (52, 60, 76, 255), 3.4)
        if i > 0:
            a = (i - 1) * 45 - 90
            arc(d, 20, 20, 11, a, a + 110, ACCENT, 3.4)
        save(img, os.path.join(OUT, 'throbber', 'throbber%d.png' % i), ICON, ICON)


def pointer(path, shape, hot=None):
    w = h = 32
    img = Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = [(x * S, y * S) for x, y in shape]
    # sombra, borde blanco y relleno oscuro (como los cursores actuales)
    d.polygon([(x + 1.5 * S, y + 1.5 * S) for x, y in pts], fill=(0, 0, 0, 90))
    d.polygon(pts, fill=(255, 255, 255, 255))
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    inner = [(cx + (x - cx) * 0.78, cy + (y - cy) * 0.78) for x, y in pts]
    d.polygon(inner, fill=(20, 22, 28, 255))
    save(img, path, w, h)


def make_pointers():
    arrow = [(2, 2), (2, 24), (8, 18.5), (12, 27), (16, 25.2), (12.2, 17), (20, 17)]
    base = os.path.join(OUT, 'pointers')
    pointer(os.path.join(base, 'default.png'), arrow)
    # mano: un circulo de acento (en TV se ve mejor que una mano chica)
    for name, col in (('point', ACCENT), ('caret', FG)):
        img = Image.new('RGBA', (32 * S, 32 * S), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        if name == 'point':
            d.ellipse((3 * S, 3 * S, 27 * S, 27 * S), fill=(0, 200, 255, 70),
                      outline=(255, 255, 255, 255), width=int(2.2 * S))
            d.ellipse((12 * S, 12 * S, 18 * S, 18 * S), fill=(255, 255, 255, 255))
        else:
            for x0 in (14.5,):
                d.rectangle(((x0 - 1) * S, 4 * S, (x0 + 2) * S, 27 * S), fill=(0, 0, 0, 255))
                d.rectangle((x0 * S, 4 * S, (x0 + 1) * S, 27 * S), fill=(255, 255, 255, 255))
                d.rectangle((10 * S, 4 * S, 20 * S, 5 * S), fill=(255, 255, 255, 255))
                d.rectangle((10 * S, 26 * S, 20 * S, 27 * S), fill=(255, 255, 255, 255))
        save(img, os.path.join(base, name + '.png'), 32, 32)


if __name__ == '__main__':
    make_icons()
    make_throbber()
    make_pointers()
    print('tema en', os.path.abspath(OUT))
