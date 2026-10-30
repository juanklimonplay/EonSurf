#!/bin/bash
# Ajustes de fuentes de netsurf-all-3.11 para PS3 que no conviene mantener como .patch.
# Se corre desde la raiz de netsurf-all-3.11, despues de port/fetch.sh.
set -e

# Superficie PS3 (RSX): compilarla, darle su propio tipo y usarla por defecto
cp ../../ps3/surface_ps3.c libnsfb/src/surface/ps3.c
cp ../../ps3/osk_ps3.c libnsfb/src/surface/ps3_osk.c
cp ../../ps3/osk_ps3.h libnsfb/src/surface/osk_ps3.h
sed -i 's/^SURFACE_HANDLER_yes := surface.c ram.c$/SURFACE_HANDLER_yes := surface.c ram.c ps3.c ps3_osk.c/' libnsfb/src/surface/Makefile
sed -i '1i CFLAGS += -I$(PS3DEV)/ppu/include -I$(PS3DEV)/portlibs/ppu/include' libnsfb/src/surface/Makefile
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
