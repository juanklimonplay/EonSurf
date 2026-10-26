#!/usr/bin/env bash
# Baja el NetSurf original (GPL v2, tarball oficial netsurf-all) y aplica nuestros parches.
set -euo pipefail
VER=3.11
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="${1:-$SCRIPT_DIR/work}"
mkdir -p "$DEST"; cd "$DEST"

if [ ! -d netsurf-all-$VER ]; then
  curl -fsSL -O https://download.netsurf-browser.org/netsurf/releases/source-full/netsurf-all-$VER.tar.gz
  tar xzf netsurf-all-$VER.tar.gz
  for p in "$SCRIPT_DIR"/patches/*.patch; do
    if [ -f "$p" ]; then
      echo "Aplicando parche: $p"
      if patch -p1 -d netsurf-all-$VER --dry-run < "$p" > /dev/null 2>&1; then
        patch -p1 -d netsurf-all-$VER -N < "$p"
      elif patch -p1 -d netsurf-all-$VER/netsurf --dry-run < "$p" > /dev/null 2>&1; then
        patch -p1 -d netsurf-all-$VER/netsurf -N < "$p"
      elif patch -p1 -d netsurf-all-$VER/libnsfb --dry-run < "$p" > /dev/null 2>&1; then
        patch -p1 -d netsurf-all-$VER/libnsfb -N < "$p"
      else
        echo "Aviso: no se pudo aplicar parche $p limpiamente"
      fi
    fi
  done
fi
echo "$PWD/netsurf-all-$VER"
