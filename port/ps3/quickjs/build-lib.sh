#!/bin/bash
# Compila QuickJS-ng para la PS3 e instala libqjs.a y quickjs.h en portlibs.
# Uso: bash port/ps3/quickjs/build-lib.sh <carpeta de trabajo>
set -euo pipefail
VER=v0.17.0
WORK=${1:-port/work}
PS3DEV=${PS3DEV:-/usr/local/ps3dev}
PREFIX=$PS3DEV/portlibs/ppu
STAMP=$PREFIX/lib/libqjs.$VER.stamp
[ -f "$STAMP" ] && exit 0

mkdir -p "$WORK" && cd "$WORK"
rm -rf quickjs && mkdir quickjs && cd quickjs
curl -sfL "https://github.com/quickjs-ng/quickjs/archive/refs/tags/$VER.tar.gz" | tar xz --strip-components=1
# PS3: sin hilos ni atomicos para JS, sin tm_gmtoff ni reloj monotono en newlib
sed -i 's/^#if defined(EMSCRIPTEN) || defined(__wasi__) || defined(__DJGPP)$/& || defined(__PPU__)/' cutils.h
CF="-O2 -mcpu=cell -std=gnu11 -D_GNU_SOURCE -DNO_TM_GMTOFF -D__STDC_NO_ATOMICS__=1 -DCLOCK_MONOTONIC=CLOCK_REALTIME -I$PS3DEV/ppu/include -w"
for f in quickjs libregexp libunicode dtoa; do
  "$PS3DEV/ppu/bin/powerpc64-ps3-elf-gcc" $CF -c $f.c -o $f.o
done
"$PS3DEV/ppu/bin/powerpc64-ps3-elf-ar" rcs libqjs.a quickjs.o libregexp.o libunicode.o dtoa.o
cp libqjs.a "$PREFIX/lib/" && cp quickjs.h "$PREFIX/include/"
rm -f "$PREFIX"/lib/libqjs.*.stamp && touch "$STAMP"
echo "[OK] QuickJS-ng $VER instalado en $PREFIX"
