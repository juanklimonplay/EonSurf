#!/usr/bin/env bash
# EonSurf - Compilación del navegador NetSurf para PlayStation 3 (PSL1GHT)
set -euo pipefail

export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin

PREFIX=/usr/local/ps3dev/portlibs/ppu
HOST=powerpc64-ps3-elf

echo "==> Preparando código de NetSurf..."
bash port/fetch.sh port/work

echo "==> Copiando driver PS3 a libnsfb..."
cp port/ps3/surface_ps3.c port/work/netsurf-all-3.11/libnsfb/src/surface/ps3.c
cp port/ps3/osk_ps3.c port/work/netsurf-all-3.11/netsurf/frontends/framebuffer/osk_ps3.c

echo "==> Todo listo para el paso de empaquetado."
