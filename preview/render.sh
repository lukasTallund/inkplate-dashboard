#!/bin/sh
# Builds the dashboard code natively, runs the tests and renders every
# scenario to preview/out/*.png. No flashing needed.
#
#   render.sh            render
#   render.sh --check    render and compare with preview/reference/; exit 2
#                        and write out/<name>.diff.png for each change
#   render.sh --accept   render and make the result the new reference
#
# Needs g++, FreeType and libpng headers, and one prior
# `esphome compile dashboard.yaml` (for the cached fonts and qrcodegen).
set -eu
mode=${1:-}
case "$mode" in "" | --check | --accept) ;; *) echo "usage: $0 [--check | --accept]" >&2; exit 64 ;; esac

here=$(cd "$(dirname "$0")" && pwd)
root=$(dirname "$here")
build="$here/build"
out="$here/out"
ref="$here/reference"
mkdir -p "$build" "$out"
rm -f "$out"/*.diff.png

# Python with PyYAML: system python if it has it, else ESPHome's own.
py=python3
if ! $py -c 'import yaml' 2>/dev/null; then
  py="$(dirname "$(readlink -f "$(command -v esphome)")")/python"
fi
"$py" "$here/gen_fonts.py" "$root/dashboard.yaml" "$root/.esphome/font" "$build/fonts.inc"

qr=$(find "$root/.esphome" -name qrcodegen.c -print -quit)
[ -n "$qr" ] || { echo "qrcodegen.c not found; run esphome compile once" >&2; exit 1; }

cxx() {
  g++ -std=c++17 -O1 -Wall -Wno-unused-function \
    -I"$root" -I"$build" -I"$(dirname "$qr")" $(pkg-config --cflags freetype2 libpng) \
    "$@" "$build/qrcodegen.o" $(pkg-config --libs freetype2 libpng)
}
gcc -O1 -c "$qr" -o "$build/qrcodegen.o"

# ESPHome #includes every header in dash/ in no particular order, so each
# one must compile on its own.
for h in "$root"/dash/*.h; do
  printf '#include "esphome_stub.h"\n#include "dash/%s"\n' "$(basename "$h")" > "$build/standalone.cpp"
  g++ -std=c++17 -fsyntax-only -Wall -Wno-unused-function -I"$here" -I"$root" -I"$(dirname "$qr")" \
    $(pkg-config --cflags freetype2) "$build/standalone.cpp" ||
    { echo "dash/$(basename "$h") doesn't compile on its own" >&2; exit 1; }
done
cxx "$here/tests.cpp" -o "$build/tests"
cxx "$here/main.cpp" -o "$build/preview"

"$build/tests"
case "$mode" in
  --check) "$build/preview" "$out" "$ref" ;;
  --accept)
    "$build/preview" "$out"
    mkdir -p "$ref"
    cp "$out"/*.png "$ref"/
    echo "reference updated: $ref"
    ;;
  *) "$build/preview" "$out" ;;
esac
