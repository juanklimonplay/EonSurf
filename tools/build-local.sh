#!/bin/bash
# Compila NetSurf para PS3 en local (WSL Ubuntu 24.04 con el kit de tools/export-kit) y firma el EBOOT.
# Uso:  bash tools/build-local.sh          -> recompila solo lo que cambio
#       bash tools/build-local.sh --fresh  -> vuelve a bajar y parchar netsurf-all-3.11
# Si RPCS3_USRDIR esta definido, copia ahi el EBOOT.BIN y los recursos res/.
# Si PKG_OUT esta definido, arma ademas el .pkg instalable en esa ruta.
# EONSURF_VERBOSE=1 prende el log detallado de NetSurf por la TTY.
set -euo pipefail
cd "$(dirname "$0")/.."
REPO=$PWD
export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
PREFIX=$PS3DEV/portlibs/ppu
TITLE_ID=EONS00001
CONTENT_ID=UP0001-EONS00001_00-0000000000000000
WORK_DIR=port/work
NS=$WORK_DIR/netsurf-all-3.11

# libiconv estatica con alias POSIX (newlib de PS3 no trae iconv)
if [ ! -f "$PREFIX/lib/libiconv.a" ]; then
  T=$(mktemp -d); cd "$T"
  wget -q https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.17.tar.gz
  tar xzf libiconv-1.17.tar.gz && cd libiconv-1.17
  CFLAGS="-O2 -mcpu=cell" ./configure --host=powerpc64-ps3-elf --prefix="$PREFIX" \
    --enable-static --disable-shared --disable-nls > /dev/null
  make -s -C libcharset && make -s -C libcharset install-lib libdir="$PWD/lib" includedir="$PWD/lib"
  make -s -C lib
  mkdir -p "$PREFIX/lib" && cp lib/.libs/libiconv.a "$PREFIX/lib/libiconv.a"
  cat > posix_alias.c <<'EOF'
#include <stddef.h>
typedef void *iconv_t;
iconv_t libiconv_open(const char *, const char *);
size_t libiconv(iconv_t, char **, size_t *, char **, size_t *);
int libiconv_close(iconv_t);
iconv_t iconv_open(const char *t, const char *f) { return libiconv_open(t, f); }
size_t iconv(iconv_t c, char **i, size_t *il, char **o, size_t *ol) { return libiconv(c, i, il, o, ol); }
int iconv_close(iconv_t c) { return libiconv_close(c); }
EOF
  powerpc64-ps3-elf-gcc -O2 -c posix_alias.c -o posix_alias.o
  powerpc64-ps3-elf-ar rcs "$PREFIX/lib/libiconv.a" posix_alias.o
  cd "$REPO"; rm -rf "$T"
fi

# Fuentes de NetSurf: se bajan y parchean una vez; despues solo se refresca el driver PS3
if [ "${1:-}" = "--fresh" ] || [ ! -d "$NS" ]; then
  rm -rf "$NS"
  bash port/fetch.sh "$WORK_DIR"
  (cd "$NS" && TITLE_ID=$TITLE_ID bash ../../ps3/tweak-sources.sh > /dev/null)
else
  cmp -s port/ps3/surface_ps3.c "$NS/libnsfb/src/surface/ps3.c" || cp port/ps3/surface_ps3.c "$NS/libnsfb/src/surface/ps3.c"
  cmp -s port/ps3/osk_ps3.c "$NS/libnsfb/src/surface/ps3_osk.c" || cp port/ps3/osk_ps3.c "$NS/libnsfb/src/surface/ps3_osk.c"
  cmp -s port/ps3/osk_ps3.h "$NS/libnsfb/src/surface/osk_ps3.h" || cp port/ps3/osk_ps3.h "$NS/libnsfb/src/surface/osk_ps3.h"
fi

# EONSURF_VERBOSE=1: log detallado de NetSurf por stderr (TTY de la PS3 / RPCS3), como si se pasara -v
GUI="$NS/netsurf/frontends/framebuffer/gui.c"
if [ "${EONSURF_VERBOSE:-0}" = 1 ] && ! grep -q EONSURF_VERBOSE "$GUI"; then
  sed -i 's/^\([ \t]*\)nslog_init(nslog_stream_configure, &argc, argv);/\1{ \/* EONSURF_VERBOSE *\/ static char *vargv[] = { "nsfb", "-v", NULL }; int vargc = 2; nslog_init(nslog_stream_configure, \&vargc, vargv); }/' "$GUI"
elif [ "${EONSURF_VERBOSE:-0}" != 1 ] && grep -q EONSURF_VERBOSE "$GUI"; then
  sed -i 's/^\([ \t]*\){ \/\* EONSURF_VERBOSE .*$/\1nslog_init(nslog_stream_configure, \&argc, argv);/' "$GUI"
fi

# El make de netsurf-all salta todo si existe build-stamp; se borra siempre y cada
# subproyecto recompila solo lo que cambio
rm -f "$NS/inst-framebuffer/build-stamp"

time make -C "$NS" TARGET=framebuffer HOST=powerpc64-ps3-elf NETSURF_FB_FONTLIB=internal -j"$(nproc)" > "$WORK_DIR/build.log" 2>&1 || {
  grep -E "error|undefined reference" "$WORK_DIR/build.log" | head -20; exit 1; }

mkdir -p build-eonsurf
cp "$NS/netsurf/nsfb" build-eonsurf/EonSurf.elf
sprxlinker build-eonsurf/EonSurf.elf > /dev/null
make_self_npdrm build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN "$CONTENT_ID" > /dev/null
ls -lh build-eonsurf/EBOOT.BIN

if [ -n "${RPCS3_USRDIR:-}" ]; then
  cp build-eonsurf/EBOOT.BIN "$RPCS3_USRDIR/EBOOT.BIN"
  mkdir -p "$RPCS3_USRDIR/res" && cp -rL "$NS/netsurf/frontends/framebuffer/res/." "$RPCS3_USRDIR/res/"
  echo "[OK] EBOOT y res copiados a $RPCS3_USRDIR"
fi

# PKG_OUT=<archivo.pkg>: arma el instalable igual que el CI (para probar en la PS3 por pendrive)
if [ -n "${PKG_OUT:-}" ]; then
  P=$(mktemp -d)
  mkdir -p "$P/USRDIR"
  cp build-eonsurf/EBOOT.BIN "$P/USRDIR/EBOOT.BIN"
  cp -rL "$NS/netsurf/frontends/framebuffer/res" "$P/USRDIR/res"
  [ -d startpage ] && cp -r startpage "$P/USRDIR/"
  [ -f pkg/eonsurf.cfg ] && cp pkg/eonsurf.cfg "$P/USRDIR/"
  cp pkg/ICON0.PNG "$P/ICON0.PNG"
  [ -f pkg/PIC1.PNG ] && cp pkg/PIC1.PNG "$P/PIC1.PNG"
  sfo --title "EonSurf Web Browser" --appid "$TITLE_ID" -f src/pkg/sfo.xml "$P/PARAM.SFO"
  pkg --contentid "$CONTENT_ID" "$P/" "$PKG_OUT" > /dev/null
  rm -rf "$P"
  ls -lh "$PKG_OUT"
fi
