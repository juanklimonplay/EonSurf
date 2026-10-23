#!/usr/bin/env bash
# Compila ps3toolchain + PSL1GHT + ps3libraries en /usr/local/ps3dev (tarda 1-2 horas).
set -euo pipefail
export PS3DEV=/usr/local/ps3dev PSL1GHT=/usr/local/ps3dev
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
mkdir -p "$PS3DEV"
W=${W:-/tmp/ps3kit}; mkdir -p "$W"; cd "$W"
[ -d ps3toolchain ] || git clone https://github.com/ps3dev/ps3toolchain
cd ps3toolchain
git checkout "${PS3TOOLCHAIN_REV:-HEAD}"
# El FTP esta bloqueado en algunos entornos: gcc 9.5 baja gmp/mpfr/mpc/isl por ftp://.
# Se cambia a https en el script del paso SPU antes de compilar.
sed -i 's|^  ./contrib/download_prerequisites|  sed -i "s#ftp://gcc.gnu.org#https://gcc.gnu.org#g" contrib/download_prerequisites; ./contrib/download_prerequisites|' scripts/006-gcc-newlib-SPU.sh
./toolchain.sh
