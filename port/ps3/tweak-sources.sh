#!/bin/bash
# Ajustes de fuentes de netsurf-all-3.11 para PS3 que no conviene mantener como .patch.
# Se corre desde la raiz de netsurf-all-3.11, despues de port/fetch.sh.
set -e

# Superficie PS3 (RSX): compilarla, darle su propio tipo y usarla por defecto
cp ../../ps3/surface_ps3.c libnsfb/src/surface/ps3.c
cp ../../ps3/osk_ps3.c libnsfb/src/surface/ps3_osk.c
cp ../../ps3/osk_ps3.h libnsfb/src/surface/osk_ps3.h
sed -i 's/^SURFACE_HANDLER_yes := surface.c ram.c$/SURFACE_HANDLER_yes := surface.c ram.c ps3.c ps3_osk.c/' libnsfb/src/surface/Makefile
sed -i '1i CFLAGS += -std=gnu99 -I$(PS3DEV)/ppu/include -I$(PS3DEV)/portlibs/ppu/include' libnsfb/src/surface/Makefile
sed -i 's/^\( *\)NSFB_SURFACE_COUNT,/\1NSFB_SURFACE_PS3, \/**< PS3 RSX surface *\/\n\1NSFB_SURFACE_COUNT,/' libnsfb/include/libnsfb.h
sed -i 's/^\(\t*\)nsfb_enumerate_surface_types(framebuffer_pick_default_fename, NULL);/&\n\1fename = "ps3";/' netsurf/frontends/framebuffer/gui.c

# Imagenes en 0xAABBGGRR, el formato que leen los plotters de 32 bpp
sed -i 's|^#include "framebuffer/bitmap.h"|&\n#include "netsurf/bitmap.h"|' netsurf/frontends/framebuffer/gui.c
sed -i 's/^\([ \t]*\)ret = netsurf_register(&framebuffer_table);/&\n\1bitmap_set_format(\&(bitmap_fmt_t){ .layout = BITMAP_LAYOUT_ABGR8888 });/' netsurf/frontends/framebuffer/gui.c

grep -in "ps3" libnsfb/src/surface/Makefile libnsfb/include/libnsfb.h netsurf/frontends/framebuffer/gui.c
grep -n "bitmap_set_format\|netsurf/bitmap.h" netsurf/frontends/framebuffer/gui.c

# Recursos, cookies y certificados en USRDIR de la PS3 (TITLE_ID viene del entorno)
PS3RES="/dev_hdd0/game/${TITLE_ID:-EONS00001}/USRDIR/res"
echo "override NETSURF_FB_RESPATH := ${PS3RES}/" > netsurf/Makefile.config
echo "override NETSURF_FB_FONTPATH := ${PS3RES}/fonts" >> netsurf/Makefile.config
sed -i "s#~/.netsurf/Cookies#${PS3RES}/Cookies#g" netsurf/frontends/framebuffer/gui.c
# La PS3 no trae certificados raiz: se lleva el paquete de Mozilla del sistema de compilacion
cp /etc/ssl/certs/ca-certificates.crt netsurf/frontends/framebuffer/res/ca-bundle.crt
sed -i "s#^\([ \t]*\)nsoption_setnull_charp(cookie_jar, .*#&\n\1nsoption_setnull_charp(ca_bundle, strdup(\"${PS3RES}/ca-bundle.crt\"));#" netsurf/frontends/framebuffer/gui.c
grep -n "Cookies\|ca_bundle" netsurf/frontends/framebuffer/gui.c
# Pagina de inicio propia de EonSurf (startpage/ se copia a USRDIR al empaquetar)
sed -i "s#^\([ \t]*\)nsoption_setnull_charp(ca_bundle, .*#&\n\1nsoption_setnull_charp(homepage_url, strdup(\"file://${PS3RES%/res}/startpage/inicio.html\"));#" netsurf/frontends/framebuffer/gui.c
grep -n "homepage_url" netsurf/frontends/framebuffer/gui.c | head -3

# Teclas extra que manda el driver PS3 (ver EONSURF_KEY_* en surface_ps3.c):
#  - 0x10000 + caracter Unicode: acentos y enie desde el teclado en pantalla
#  - 0x7F01 / 0x7F02: zoom de la pagina con L2 / R2
python3 - <<'PY'
p = 'netsurf/frontends/framebuffer/fbtk/event.c'
s = open(p).read()
old = 'fbtk_keycode_to_ucs4(int code, fbtk_modifier_type mods)\n{\n\tint ucs4 = -1;\n'
assert old in s
s = s.replace(old, old + '\n\tif (code >= 0x10000)\n\t\treturn code - 0x10000; /* EonSurf: caracter Unicode directo */\n', 1)
open(p, 'w').write(s)

p = 'netsurf/frontends/framebuffer/gui.c'
s = open(p).read()
old = '\tcase NSFB_EVENT_KEY_DOWN:\n\t\tswitch (cbi->event->value.keycode) {\n'
# el mismo switch esta en el manejador de clics; se inserta en el de teclas (despues de "got value")
at = s.index(old, s.index('"got value %d"')) + len(old)
s = s[:at] + '''
		case 0x7F01: /* EonSurf: L2 = alejar */
			browser_window_set_scale(gw->bw, -0.25, false);
			break;

		case 0x7F02: /* EonSurf: R2 = acercar */
			browser_window_set_scale(gw->bw, 0.25, false);
			break;
''' + s[at:]
open(p, 'w').write(s)
print('teclas EonSurf agregadas')
PY

# Tema EonSurf: fuentes Noto con FreeType, barra oscura de 52 px, iconos y punteros nuevos
# (los PNG salen de tools/make-theme.py)
cp ../../ps3/theme/icons/*.png netsurf/frontends/framebuffer/res/icons/
cp ../../ps3/theme/throbber/*.png netsurf/frontends/framebuffer/res/throbber/
cp ../../ps3/theme/pointers/*.png netsurf/frontends/framebuffer/res/pointers/
mkdir -p netsurf/frontends/framebuffer/res/fonts
cp ../../ps3/fonts/*.ttf ../../ps3/fonts/OFL.txt netsurf/frontends/framebuffer/res/fonts/
cat >> netsurf/Makefile.config <<'MK'
override NETSURF_FB_FONTLIB := freetype
override NETSURF_FB_FONT_SANS_SERIF := NotoSans-Regular.ttf
override NETSURF_FB_FONT_SANS_SERIF_BOLD := NotoSans-Bold.ttf
override NETSURF_FB_FONT_SANS_SERIF_ITALIC := NotoSans-Italic.ttf
override NETSURF_FB_FONT_SANS_SERIF_ITALIC_BOLD := NotoSans-BoldItalic.ttf
override NETSURF_FB_FONT_SERIF := NotoSerif-Regular.ttf
override NETSURF_FB_FONT_SERIF_BOLD := NotoSerif-Bold.ttf
override NETSURF_FB_FONT_MONOSPACE := NotoSansMono-Regular.ttf
override NETSURF_FB_FONT_MONOSPACE_BOLD := NotoSansMono-Bold.ttf
override NETSURF_FB_FONT_CURSIVE := NotoSans-Italic.ttf
override NETSURF_FB_FONT_FANTASY := NotoSans-Bold.ttf
MK
python3 - <<'PY'
import re
fb = 'netsurf/frontends/framebuffer/'

def patch(path, old, new, count=1, regex=False):
    s = open(path).read()
    if regex:
        s2, n = re.subn(old, new, s)
    else:
        n = s.count(old)
        s2 = s.replace(old, new)
    assert n >= count, (path, old)
    open(path, 'w').write(s2)

# colores (0xAABBGGRR): barra #1B1F2A, barras de desplazamiento #4A5468
patch(fb + 'fbtk.h', '#define FB_SCROLL_COLOUR 0xFFAAAAAA', '#define FB_SCROLL_COLOUR 0xFF68544A')
patch(fb + 'fbtk.h', '#define FB_FRAME_COLOUR 0xFFDDDDDD', '#define FB_FRAME_COLOUR 0xFF2A1F1B')
patch(fb + 'fbtk/scroll.c', '1, 0xFF999999, false, false);', '1, widget->fg, false, false);', 2)

# tamanos y orden de la barra: atras, adelante, recargar, detener, URL, carga
patch(fb + 'options.h', 'NSOPTION_INTEGER(fb_toolbar_size, 30)', 'NSOPTION_INTEGER(fb_toolbar_size, 52)')
patch(fb + 'options.h', 'NSOPTION_INTEGER(fb_furniture_size, 18)', 'NSOPTION_INTEGER(fb_furniture_size, 22)')
patch(fb + 'gui.c', '#define NSFB_TOOLBAR_DEFAULT_LAYOUT "blfsrutc"', '#define NSFB_TOOLBAR_DEFAULT_LAYOUT "bfrsut"')
patch(fb + 'gui.c', r'(create_toolbar\(gw,\s*toolbar_height,\s*)2,', r'\g<1>6,', regex=True)
patch(fb + 'gui.c', 'resize_toolbar(gw, toolbar_height, 2,', 'resize_toolbar(gw, toolbar_height, 6,')

# barra de direcciones gris azulada con texto claro; estado en gris suave
patch(fb + 'gui.c', r'FB_COLOUR_WHITE,(\s*)FB_COLOUR_BLACK,(\s*)true,(\s*)fb_url_enter',
      r'0xFF45332C,\g<1>0xFFF2ECE8,\g<2>true,\g<3>fb_url_enter', regex=True)
patch(fb + 'gui.c', r'(gw->status = fbtk_create_text\([^;]*?)FB_COLOUR_BLACK', r'\g<1>0xFFB8A59A', regex=True)

# controles de formularios de las paginas con colores actuales
for name, col in (('ButtonFace', '0x00f5f1ee'), ('ButtonHighlight', '0x00ffffff'),
                  ('ButtonShadow', '0x00ddd6d0'), ('ThreeDFace', '0x00f5f1ee'),
                  ('ThreeDHighlight', '0x00ffffff'), ('ThreeDLightShadow', '0x00ddd6d0'),
                  ('ThreeDShadow', '0x00c4bab3'), ('ThreeDDarkShadow', '0x00a0948c'),
                  ('Highlight', '0x00ffc800'), ('Window', '0x00ffffff')):
    patch(fb + 'gui.c', r'(nsoption_set_colour\(sys_colour_%s, )0x[0-9a-fA-F]{8}' % name,
          r'\g<1>%s' % col, regex=True)

# campos de texto con borde (la barra de direcciones) con esquinas redondeadas
patch(fb + 'fbtk/text.c', '''	/* clear background */
	if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
	}

	/* widget can have a single pixel outline border */
	if (widget->u.text.outline) {
		rect.x1--;
		rect.y1--;
		nsfb_plot_rectangle(root->u.root.fb, &rect, 1,
				0x00000000, false, false);
	}
''', '''	/* clear background */
	if (widget->u.text.outline) {
		/* EonSurf: rectangulo redondeado sobre el color de la barra */
		int r = (bbox.y1 - bbox.y0) / 4;
		int dy;
		nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, FB_FRAME_COLOUR);
		for (dy = 0; dy < bbox.y1 - bbox.y0; dy++) {
			int e = dy < r ? r - dy : dy >= bbox.y1 - bbox.y0 - r ? dy - (bbox.y1 - bbox.y0 - r) + 1 : 0;
			int in = r - (int)sqrt((double)(r * r - e * e));
			nsfb_bbox_t row = { bbox.x0 + in, bbox.y0 + dy, bbox.x1 - in, bbox.y0 + dy + 1 };
			nsfb_plot_rectangle_fill(root->u.root.fb, &row, widget->bg);
		}
	} else if ((widget->bg & 0xFF000000) != 0) {
		/* transparent polygon filling isnt working so fake it */
		nsfb_plot_rectangle_fill(root->u.root.fb, &bbox, widget->bg);
	}
''')
patch(fb + 'fbtk/text.c', '#include <libnsfb_plot.h>', '#include <math.h>\n#include <libnsfb_plot.h>')
# el texto de la barra no debe pegarse a la curva
patch(fb + 'fbtk/text.c', '\tif (widget->u.text.outline)\n\t\t*padding = 1;', '\tif (widget->u.text.outline)\n\t\t*padding = 4;')
# las imagenes compiladas (iconos, punteros) son palabras 0xAABBGGRR: en la PS3
# (big-endian) los bytes van al reves que en la PC donde corre convert_image
patch('netsurf/tools/convert_image.c', '\t\tmemcpy(row, new_row, rowbytes);\n',
      '\t\tmemcpy(row, new_row, rowbytes);\n'
      '\t\tfor (i = 0; i + 3 < rowbytes; i += 4) { /* EonSurf: RGBA -> ABGR */\n'
      '\t\t\tunsigned char t = row[i]; row[i] = row[i + 3]; row[i + 3] = t;\n'
      '\t\t\tt = row[i + 1]; row[i + 1] = row[i + 2]; row[i + 2] = t;\n'
      '\t\t}\n')
print('tema EonSurf aplicado')
PY

# JavaScript: Duktape ya se compila, pero NetSurf lo trae apagado por defecto
sed -i 's/^NSOPTION_BOOL(enable_javascript, false)/NSOPTION_BOOL(enable_javascript, true)/' netsurf/desktop/options.h
grep -n "enable_javascript" netsurf/desktop/options.h

# Mejoras de JavaScript (redibujo en vivo y APIs que faltan)
python3 ../../ps3/patch-js.py

# Motor JavaScript: QuickJS-ng (por defecto) o el Duktape de NetSurf (EONSURF_JS=duktape)
if [ "${EONSURF_JS:-quickjs}" = quickjs ]; then
  Q=netsurf/content/handlers/javascript/quickjs
  mkdir -p $Q
  cp ../../ps3/quickjs/eon_qjs.c ../../ps3/quickjs/dom.js ../../ps3/quickjs/intl.js $Q/
  cp ../../ps3/quickjs/Makefile.netsurf $Q/Makefile
  python3 ../../ps3/quickjs/patch-netsurf.py
  sed -i 's#^ifeq ($(NETSURF_USE_DUKTAPE),YES)#ifeq ($(NETSURF_USE_QUICKJS),YES)\ninclude content/handlers/javascript/quickjs/Makefile\nelse ifeq ($(NETSURF_USE_DUKTAPE),YES)#' netsurf/content/handlers/javascript/Makefile
  printf 'override NETSURF_USE_QUICKJS := YES\noverride NETSURF_USE_DUKTAPE := NO\n' >> netsurf/Makefile.config
  sed -i 's/-Wl,--start-group -lrsx /-Wl,--start-group -lqjs -lrsx /' netsurf/frontends/framebuffer/Makefile
  grep -n "QUICKJS" netsurf/content/handlers/javascript/Makefile
  grep -n "lqjs" netsurf/frontends/framebuffer/Makefile
fi

# HTML5 en el CSS base: NetSurf 3.11 no oculta [hidden], <template> ni los
# <dialog>/<details> cerrados (GitHub, YouTube y otros los usan para todo)
cat >> netsurf/resources/default.css <<'CSS'

/* EonSurf: reglas HTML5 que faltan */
[hidden], template, slot, dialog:not([open]), noscript { display: none; }
details:not([open]) > :not(summary) { display: none; }
main, article, aside, nav, section, header, footer, figure, figcaption, hgroup, search { display: block; }
dialog[open] { display: block; position: absolute; left: 0; right: 0; margin: auto; border: solid; padding: 1em; background: white; color: black; }
CSS

# Variables CSS (var(--x)): preprocesador antes de libcss
python3 ../../ps3/css/patch-css.py

# Red mas rapida: sesiones TLS compartidas y mas conexiones reutilizadas
python3 ../../ps3/net/patch-net.py
