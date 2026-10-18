#!/usr/bin/env bash
# Baja el NetSurf original (GPL v2, tarball oficial netsurf-all) y aplica nuestros parches.
set -euo pipefail
VER=3.11
DEST="${1:-$(dirname "$0")/work}"
mkdir -p "$DEST"; cd "$DEST"
if [ ! -d netsurf-all-$VER ]; then
  curl -fsSL -O https://download.netsurf-browser.org/netsurf/releases/source-full/netsurf-all-$VER.tar.gz
  tar xzf netsurf-all-$VER.tar.gz
  for p in "$(dirname "$(readlink -f "$0")")"/patches/*.patch; do
    patch -p1 -d netsurf-all-$VER/netsurf < "$p"
  done
fi
echo "$PWD/netsurf-all-$VER"
