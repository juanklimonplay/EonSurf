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
      patch -p1 -d netsurf-all-$VER/netsurf < "$p" || patch -p1 -d netsurf-all-$VER < "$p" || true
    fi
  done
fi
echo "$PWD/netsurf-all-$VER"
