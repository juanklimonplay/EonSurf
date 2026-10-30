#!/usr/bin/env bash
# Compila NetSurf (frontend framebuffer, superficie ram) nativo en Linux, para probar sin PS3.
# Requiere: build-essential libcurl4-openssl-dev libssl-dev libpng-dev libjpeg-dev flex bison gperf libexpat1-dev perl
set -euo pipefail
SRC=$("$(dirname "$(readlink -f "$0")")/../fetch.sh" "${1:-/tmp/eonsurf-work}" | tail -1)
cd "$SRC"
make TARGET=framebuffer -j"$(nproc)"
echo "$SRC/netsurf/nsfb"
