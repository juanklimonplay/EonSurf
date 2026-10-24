#!/usr/bin/env bash
# EonSurf - Compilación automatizada del Core NetSurf 3.11 para PS3
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
echo " [EonSurf] 2. Configurando soporte PS3 y dependencias..."
echo "=========================================================="

# 2.1 Proveer endian.h a portlibs si no existe en el toolchain
if [ ! -f /usr/local/ps3dev/portlibs/ppu/include/endian.h ]; then
    echo "Instalando endian.h en portlibs PPU..."
    sudo mkdir -p /usr/local/ps3dev/portlibs/ppu/include
    sudo cp ../../ps3/endian.h /usr/local/ps3dev/portlibs/ppu/include/endian.h
fi

# 2.2 Desactivar binding expat en libdom (NetSurf usa Hubbub para HTML5 completo)
echo "Configurando libdom (WITH_EXPAT_BINDING := no, WITH_HUBBUB_BINDING := yes)..."
cat << 'EOF' > libdom/Makefile.config.override
WITH_LIBXML_BINDING := no
WITH_EXPAT_BINDING := no
WITH_HUBBUB_BINDING := yes
EOF

# 2.3 Excluir libsvgtiny de Makefile principal ya que depende de expat/xml
sed -i 's/NSLIB_SVGTINY_TARG := libsvgtiny/NSLIB_SVGTINY_TARG :=/' Makefile

# 2.4 Copiar driver de superficie RSX nativo a libnsfb
echo "Inyectando driver de superficie PS3 RSX en libnsfb..."
cp ../../ps3/surface_ps3.c libnsfb/src/surface/ps3.c

# Agregar ps3 a los sources de superficie en libnsfb si no esta presente
if ! grep -q "ps3.c" libnsfb/src/surface/Makefile; then
    sed -i 's/SURFACE_HANDLER_yes := surface.c ram.c/SURFACE_HANDLER_yes := surface.c ram.c ps3.c/' libnsfb/src/surface/Makefile
fi

# 2.5 Configurar Makefile.config de NetSurf Framebuffer
echo "Configurando opciones de NetSurf Framebuffer..."
cat << 'EOF' > netsurf/Makefile.config
override NETSURF_FB_FRONTEND := ps3
override NETSURF_FB_FONTLIB := internal
override NETSURF_USE_HARU_PDF := NO
override NETSURF_USE_NSSVG := NO
override NETSURF_USE_ROSPRITE := NO
override NETSURF_USE_BMP := YES
override NETSURF_USE_GIF := YES
override NETSURF_USE_JPEG := NO
override NETSURF_USE_PNG := NO
override NETSURF_USE_WEBP := NO
override NETSURF_USE_OPENSSL := NO
override NETSURF_USE_CURL := YES
override CFLAGS += -mcpu=cell -DNSFB_PS3_AVAILABLE -I/usr/local/ps3dev/portlibs/ppu/include -I/usr/local/ps3dev/include
override LDFLAGS += -L/usr/local/ps3dev/portlibs/ppu/lib -L/usr/local/ps3dev/lib -lrsx -lgcm_sys -lio -lsysutil -lnet -lmbedtls -lcurl -lrt -llv2 -lm
EOF

echo "=========================================================="
echo " [EonSurf] 3. Compilando librerias del Core NetSurf..."
echo "=========================================================="
make TARGET=framebuffer HOST="$HOST" BUILD="$(gcc -dumpmachine)" V=1 -j"$(nproc)"

echo "=========================================================="
echo " [EonSurf] 4. Proceso de compilacion exitoso."
echo "=========================================================="
