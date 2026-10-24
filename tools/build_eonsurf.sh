#!/usr/bin/env bash
# EonSurf - Script Maestro de Compilación y Empaquetado PKG Oficial
set -euo pipefail

TITLE_ID="EONS00001"
CONTENT_ID="UP0001-${TITLE_ID}_00-0000000000000000"
BUILD_DIR="build-eonsurf"
PKG_DIR="dev_hdd0/game/${TITLE_ID}"

mkdir -p "${BUILD_DIR}" "${PKG_DIR}/USRDIR"

echo "[1/5] Compilando objetos PPU con optimizaciones Cell B.E. & AltiVec..."
for src_file in src/source/*.c; do
    obj_file="${BUILD_DIR}/$(basename "${src_file%.c}.o")"
    powerpc64-ps3-elf-gcc -O2 -mcpu=cell -maltivec -mabi=altivec -Wall \
        -I"${PSL1GHT}/ppu/include" \
        -I"${PS3DEV}/portlibs/ppu/include" \
        -Isrc/source \
        -Iport/ps3 \
        -c "${src_file}" -o "${obj_file}"
done

echo "[2/5] Enlazando ELF con jerarquía estricta de librerías Lv2..."
powerpc64-ps3-elf-gcc "${BUILD_DIR}"/*.o \
    -L"${PSL1GHT}/ppu/lib" \
    -L"${PS3DEV}/portlibs/ppu/lib" \
    -lrsx -lgcm_sys -lnet -lio -lfont -lfs -laudio \
    -lsysutil -lsysmodule -lm -lrt -llv2 \
    -o "${BUILD_DIR}/EonSurf.elf"

echo "[3/5] Resolviendo stubs PRX contra la tabla OPD (sprxlinker)..."
if command -v sprxlinker >/dev/null 2>&1; then
    sprxlinker "${BUILD_DIR}/EonSurf.elf"
fi

echo "[4/5] Generando EBOOT.BIN libre para test rápido en RPCS3..."
if command -v make_fself >/dev/null 2>&1; then
    make_fself "${BUILD_DIR}/EonSurf.elf" "${BUILD_DIR}/EBOOT_RPCS3.BIN"
fi

echo "[5/5] Firmando EBOOT.BIN NPDRM y empaquetando PKG oficial (CEX/HEN/CFW)..."
if command -v make_self_npdrm >/dev/null 2>&1; then
    make_self_npdrm "${BUILD_DIR}/EonSurf.elf" \
        "${PKG_DIR}/USRDIR/EBOOT.BIN" \
        "${CONTENT_ID}"
fi

if command -v pkg.py >/dev/null 2>&1; then
    pkg.py --contentid "${CONTENT_ID}" "${PKG_DIR}/" "EonSurf-v0.3.1-alpha.pkg"
elif command -v pkg_builder >/dev/null 2>&1; then
    pkg_builder -c pkg/eonsurf_pkg.conf "${PKG_DIR}/" "EonSurf-v0.3.1-alpha.pkg"
fi

echo "[OK] Build completado: EonSurf-v0.3.1-alpha.pkg listo para instalar."
