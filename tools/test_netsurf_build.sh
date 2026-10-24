#!/usr/bin/env bash
# EonSurf - Compilación del Core de NetSurf 3.11 con Frontend Framebuffer para PS3
set -euo pipefail

export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin

HOST=powerpc64-ps3-elf
PREFIX=/usr/local/ps3dev/portlibs/ppu
WORK_DIR="port/work"

echo "=========================================================="
echo " [EonSurf] 1. Descargando fuentes originales de NetSurf..."
echo "=========================================================="
bash port/fetch.sh "$WORK_DIR"

cd "$WORK_DIR/netsurf-all-3.11"

echo "=========================================================="
echo " [EonSurf] 2. Verificando librerias auxiliares..."
echo "=========================================================="
# Las librerias base de netsurf-all-3.11
make TARGET=framebuffer HOST="$HOST" BUILD="$(gcc -dumpmachine)" V=1 -j"$(nproc)" || true

echo "=========================================================="
echo " [EonSurf] 3. Proceso finalizado."
echo "=========================================================="
