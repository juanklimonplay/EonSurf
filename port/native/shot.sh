#!/usr/bin/env bash
# Uso: shot.sh <url> <salida.png> [segundos] [ancho] [alto]
set -euo pipefail
SRC=${EONSURF_SRC:-/tmp/eonsurf-work/netsurf-all-3.11}
export NETSURFRES="$SRC/netsurf/frontends/framebuffer/res"
EONSURF_SHOT="$2.ppm" EONSURF_SHOT_SECS="${3:-20}" timeout $(( ${3:-20} + 60 )) \
  "$SRC/netsurf/nsfb" -f ram -w "${4:-1280}" -h "${5:-720}" "$1" >/dev/null 2>&1 || true
convert "$2.ppm" "$2" && rm -f "$2.ppm"
