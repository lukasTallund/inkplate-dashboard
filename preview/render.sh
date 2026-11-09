#!/bin/sh
# Render dashboard.h to preview/out/*.png without flashing.
# Needs g++, FreeType and libpng headers, and one prior
# `esphome compile dashboard.yaml` (for the cached fonts and qrcodegen).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
build="$here/build"
mkdir -p "$build" "$here/out"

# Python with PyYAML: system python if it has it, else ESPHome's own.
py=python3
if ! $py -c 'import yaml' 2>/dev/null; then
  py="$(dirname "$(readlink -f "$(command -v esphome)")")/python"
fi
"$py" "$here/gen_fonts.py" "$root/dashboard.yaml" "$root/.esphome/font" "$build/fonts.inc"

qr=$(find "$root/.esphome" -name qrcodegen.c -print -quit)
[ -n "$qr" ] || { echo "qrcodegen.c not found; run esphome compile once" >&2; exit 1; }

gcc -O1 -c "$qr" -o "$build/qrcodegen.o"
g++ -std=c++17 -O1 -Wall -Wno-unused-function \
  -I"$build" -I"$(dirname "$qr")" $(pkg-config --cflags freetype2 libpng) \
  "$here/main.cpp" "$build/qrcodegen.o" $(pkg-config --libs freetype2 libpng) \
  -o "$build/preview"
"$build/preview" "$here/out"
