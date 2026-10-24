#!/usr/bin/env bash
# EonSurf - Compilación del navegador NetSurf para PlayStation 3 (PSL1GHT)
set -euo pipefail

export PS3DEV=/usr/local/ps3dev
export PSL1GHT=/usr/local/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin

HOST=powerpc64-ps3-elf
PREFIX=/usr/local/ps3dev/portlibs/ppu
WORK_DIR="$(pwd)/port/work"

echo "==> 1. Descargando código de NetSurf..."
bash port/fetch.sh "$WORK_DIR"

echo "==> 2. Instalando driver PS3 en libnsfb y frontends..."
cp port/ps3/surface_ps3.c "$WORK_DIR/netsurf-all-3.11/libnsfb/src/surface/ps3.c"
cp port/ps3/osk_ps3.c "$WORK_DIR/netsurf-all-3.11/netsurf/frontends/framebuffer/osk_ps3.c"

echo "==> 3. Generando binario EonSurf para PS3..."
mkdir -p build-eonsurf/pkg
cp repo/pkg/ICON0.PNG build-eonsurf/pkg/ 2>/dev/null || cp hola/pkg/ICON0.PNG build-eonsurf/pkg/
cp repo/pkg/PIC1.PNG build-eonsurf/pkg/ 2>/dev/null || cp hola/pkg/PIC1.PNG build-eonsurf/pkg/ || true
cp hola/pkg/sfo.xml build-eonsurf/pkg/

INC_FLAGS="-Ihola/source -Iport/ps3 -I$PSL1GHT/include -I$PSL1GHT/ppu/include -I$PREFIX/include -I$WORK_DIR/netsurf-all-3.11/libnsfb/include -I$WORK_DIR/netsurf-all-3.11/libnsfb/src -I$WORK_DIR/netsurf-all-3.11/libnsfb/src/plot"
LIB_FLAGS="-L$PSL1GHT/lib -L$PSL1GHT/ppu/lib -L$PREFIX/lib"

NSFB_DIR="$WORK_DIR/netsurf-all-3.11/libnsfb"
NSFB_SOURCES="
    $NSFB_DIR/src/libnsfb.c
    $NSFB_DIR/src/cursor.c
    $NSFB_DIR/src/palette.c
    $NSFB_DIR/src/surface/surface.c
    $NSFB_DIR/src/surface/ram.c
    $NSFB_DIR/src/plot/api.c
    $NSFB_DIR/src/plot/util.c
    $NSFB_DIR/src/plot/generic.c
    $NSFB_DIR/src/plot/32bpp-xrgb8888.c
    $NSFB_DIR/src/plot/32bpp-xbgr8888.c
    $NSFB_DIR/src/plot/16bpp.c
    $NSFB_DIR/src/plot/8bpp.c
"

echo "Enlazando EonSurf con libnsfb y driver RSX nativo..."
powerpc64-ps3-elf-gcc -O2 -mcpu=cell -DNSFB_PS3_AVAILABLE \
    port/ps3/surface_ps3.c port/ps3/osk_ps3.c hola/source/main.c $NSFB_SOURCES \
    $INC_FLAGS $LIB_FLAGS \
    -lrsx -lgcm_sys -lio -lsysutil -lnet -lmbedtls -lcurl -lrt -llv2 -lm \
    -o build-eonsurf/EonSurf.elf

echo "==> 4. Firmando ejecutable y generando EonSurf-Navegador.pkg..."
# Usar make_fself directo para compatibilidad universal (RPCS3 + HEN/CFW sin bloqueo de clave DRM)
if command -v make_fself >/dev/null 2>&1; then
    make_fself build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN
elif command -v fself >/dev/null 2>&1; then
    fself build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN
elif command -v make_self >/dev/null 2>&1; then
    make_self build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN
else
    cp build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN
fi

sfo --title "EonSurf" --appid "EONSURF01" -f build-eonsurf/pkg/sfo.xml build-eonsurf/PARAM.SFO

mkdir -p build-eonsurf/pkg_dir/USRDIR
mkdir -p build-eonsurf/pkg_dir/USRDIR/startpage
cp build-eonsurf/EBOOT.BIN build-eonsurf/pkg_dir/USRDIR/
cp build-eonsurf/PARAM.SFO build-eonsurf/pkg_dir/
cp build-eonsurf/pkg/ICON0.PNG build-eonsurf/pkg_dir/
cp build-eonsurf/pkg/PIC1.PNG build-eonsurf/pkg_dir/ 2>/dev/null || true
cp repo/pkg/eonsurf.cfg build-eonsurf/pkg_dir/USRDIR/ 2>/dev/null || true
cp repo/pkg/lang_*.json build-eonsurf/pkg_dir/USRDIR/ 2>/dev/null || true
cp repo/startpage/index.html build-eonsurf/pkg_dir/USRDIR/startpage/ 2>/dev/null || true

if command -v pkg >/dev/null 2>&1; then
    pkg --contentid UP0001-EONSURF01_00-0000000000000000 build-eonsurf/pkg_dir/ EonSurf-Navegador.pkg
    if command -v package_finalize >/dev/null 2>&1; then
        package_finalize EonSurf-Navegador.pkg || true
    fi
elif command -v pkg.py >/dev/null 2>&1; then
    pkg.py --contentid UP0001-EONSURF01_00-0000000000000000 build-eonsurf/pkg_dir/ EonSurf-Navegador.pkg
else
    echo "Aviso: herramienta pkg no disponible"
fi

echo "==> Proceso de compilación de EonSurf finalizado con éxito."
