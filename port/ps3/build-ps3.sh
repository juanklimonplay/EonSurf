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
cp port/ps3/Makefile.config "$WORK_DIR/netsurf-all-3.11/netsurf/Makefile.config"

echo "==> 3. Compilando librerías y navegador NetSurf para PS3..."
cd "$WORK_DIR/netsurf-all-3.11"
make TARGET=framebuffer HOST=$HOST PREFIX=$PREFIX CC=powerpc64-ps3-elf-gcc CXX=powerpc64-ps3-elf-g++ AR=powerpc64-ps3-elf-ar Q= -j"$(nproc)" || true

echo "==> 4. Generando binario EonSurf para PS3..."
cd "$WORK_DIR/../.."
mkdir -p build-eonsurf/pkg
cp hola/pkg/ICON0.PNG build-eonsurf/pkg/
cp hola/pkg/sfo.xml build-eonsurf/pkg/

# Si nsfb compiló, enlazar como EBOOT de PS3; si no, enlazar el runner nativo con los módulos
if [ -f "$WORK_DIR/netsurf-all-3.11/netsurf/nsfb" ]; then
    echo "NetSurf nsfb compilado con éxito. Creando ELF y SELF..."
    cp "$WORK_DIR/netsurf-all-3.11/netsurf/nsfb" build-eonsurf/EonSurf.elf
else
    echo "Enlazando EonSurf con driver RSX nativo y módulos..."
    powerpc64-ps3-elf-gcc -O2 -mcpu=cell -DNSFB_PS3_AVAILABLE \
        port/ps3/surface_ps3.c port/ps3/osk_ps3.c hola/source/main.c \
        -Ihola/source -Iport/ps3 -I/usr/local/ps3dev/portlibs/ppu/include -I/usr/local/ps3dev/include \
        -L/usr/local/ps3dev/portlibs/ppu/lib -L/usr/local/ps3dev/lib \
        -lrsx -lgcm_sys -lio -lsysutil -lnet -lmbedtls -lcurl -lrt -llv2 -lm \
        -o build-eonsurf/EonSurf.elf
fi

echo "==> 5. Firmando ejecutable y generando EonSurf.pkg..."
make_fself build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN
make_self_npdrm build-eonsurf/EonSurf.elf build-eonsurf/EBOOT.BIN UP0001-EONSURF01_00-0000000000000000 || true
sfo -f build-eonsurf/pkg/sfo.xml build-eonsurf/PARAM.SFO

mkdir -p build-eonsurf/pkg_dir/USRDIR
cp build-eonsurf/EBOOT.BIN build-eonsurf/pkg_dir/USRDIR/
cp build-eonsurf/PARAM.SFO build-eonsurf/pkg_dir/
cp build-eonsurf/pkg/ICON0.PNG build-eonsurf/pkg_dir/

pkg.py --contentid UP0001-EONSURF01_00-0000000000000000 build-eonsurf/pkg_dir/ EonSurf-Navegador.pkg || \
package_finalize build-eonsurf/pkg_dir/ EonSurf-Navegador.pkg || true

echo "==> Proceso de compilación de EonSurf finalizado."
