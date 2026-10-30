#!/bin/bash
# Compila el nucleo de QuickJS-ng para la PS3 (sin quickjs-libc) y una prueba minima.
export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev
export PATH=$PS3DEV/bin:$PS3DEV/ppu/bin:$PATH
Q=${QJS_DIR:-/root/qjs}
mkdir -p $Q && cd $Q
[ -d src ] || { curl -sL https://github.com/quickjs-ng/quickjs/archive/refs/tags/v0.17.0.tar.gz | tar xz && mv quickjs-* src; }
cd src
# PS3: sin hilos ni atomicos para JS, sin tm_gmtoff ni reloj monotono en newlib
sed -i "s/^#if defined(EMSCRIPTEN) || defined(__wasi__) || defined(__DJGPP)$/#if defined(EMSCRIPTEN) || defined(__wasi__) || defined(__DJGPP) || defined(__PPU__)/" cutils.h
CC=powerpc64-ps3-elf-gcc
CF="-O2 -mcpu=cell -std=gnu11 -D_GNU_SOURCE -DNO_TM_GMTOFF -D__STDC_NO_ATOMICS__=1 -DCLOCK_MONOTONIC=CLOCK_REALTIME -I$PS3DEV/ppu/include -I$PS3DEV/portlibs/ppu/include -w"
mkdir -p ../obj
for f in quickjs libregexp libunicode dtoa; do
  $CC $CF -c $f.c -o ../obj/$f.o || { echo "FALLO $f"; exit 1; }
done
powerpc64-ps3-elf-ar rcs ../obj/libqjs.a ../obj/*.o
ls -la ../obj/libqjs.a

# programa de prueba
T=$(cd "$(dirname "$0")" && pwd)
$CC $CF -I/root/qjs/src -c $T/prueba.c -o ../obj/main.o || exit 1
powerpc64-ps3-elf-gcc -mcpu=cell -o ../obj/qjstest.elf ../obj/main.o ../obj/libqjs.a -L$PS3DEV/ppu/lib -L$PS3DEV/portlibs/ppu/lib -lrt -llv2 -lm || exit 1
sprxlinker ../obj/qjstest.elf
make_self ../obj/qjstest.elf ../obj/qjstest.self 2>/dev/null || make_self_npdrm ../obj/qjstest.elf ../obj/qjstest.self UP0001-EONS00002_00-0000000000000000
ls -la ../obj/qjstest.self && cp ../obj/qjstest.self $T/EBOOT.self
