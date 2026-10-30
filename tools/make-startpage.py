#!/usr/bin/env python3
"""Imagenes de la pagina de inicio (NetSurf no tiene gradientes ni bordes redondeados en CSS).
Uso: python tools/make-startpage.py  (escribe en startpage/img/)"""
import math, os
from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = os.path.join(os.path.dirname(__file__), '..', 'startpage', 'img')
FONT = os.path.join(os.path.dirname(__file__), '..', 'port', 'ps3', 'fonts', 'NotoSans-Bold.ttf')
BG = (12, 15, 23)
S = 3

def hero():
    w, h = 1160, 230
    img = Image.new('RGB', (w, h), BG)
    glow = Image.new('RGB', (w, h), BG)
    d = ImageDraw.Draw(glow)
    d.ellipse((-200, -260, 700, 330), fill=(0, 110, 170))
    d.ellipse((560, -120, 1400, 420), fill=(90, 40, 170))
    glow = glow.filter(ImageFilter.GaussianBlur(110))
    img = Image.blend(img, glow, 0.85)
    d = ImageDraw.Draw(img)
    f = ImageFont.truetype(FONT, 76)
    d.text((48, 52), 'EonSurf', font=f, fill=(255, 255, 255))
    f2 = ImageFont.truetype(FONT.replace('-Bold', '-Regular'), 24)
    d.text((52, 150), 'La web moderna en tu PlayStation 3', font=f2, fill=(200, 220, 240))
    img.save(os.path.join(OUT, 'hero.png'))

def card(name, accent):
    w, h = 360, 92
    img = Image.new('RGB', (w * S, h * S), BG)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w * S - 1, h * S - 1), 16 * S, fill=(24, 29, 41),
                        outline=(40, 48, 64), width=S)
    d.rounded_rectangle((18 * S, 18 * S, 74 * S, 74 * S), 14 * S, fill=accent)
    f = ImageFont.truetype(FONT, 30 * S)
    d.text((46 * S, 46 * S), name[0], font=f, fill=(255, 255, 255), anchor='mm')
    img.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, 'card-%s.png' % name.lower()))

def search():
    w, h = 1160, 72
    img = Image.new('RGB', (w * S, h * S), BG)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle((0, 0, w * S - 1, h * S - 1), 36 * S, fill=(24, 29, 41),
                        outline=(0, 200, 255), width=2 * S)
    cx, cy, r = 40 * S, 36 * S, 11 * S
    d.ellipse((cx - r, cy - r - 3 * S, cx + r, cy + r - 3 * S), outline=(140, 155, 180), width=3 * S)
    d.line((cx + 7 * S, cy + 5 * S, cx + 15 * S, cy + 13 * S), fill=(140, 155, 180), width=3 * S)
    img.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, 'search.png'))

os.makedirs(OUT, exist_ok=True)
hero(); search()
for n, c in (('DuckDuckGo', (222, 88, 51)), ('Wikipedia', (90, 100, 120)), ('NPR', (40, 110, 220)),
             ('CNN', (204, 0, 0)), ('HackerNews', (255, 102, 0)), ('PSX', (0, 112, 209))):
    card(n, c)
print('listo')
