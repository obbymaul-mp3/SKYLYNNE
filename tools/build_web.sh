#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# SKYLYNNE in the browser: the instrument's own sources as a freestanding WebAssembly module (clang and wasm-ld, no
# Emscripten, no C library), and the page that runs it.
#   sh tools/build_web.sh   ->  build/web/{index.html, play.js, worklet.js, skylynne.wasm}
# Needs clang with the wasm32 target and wasm-ld (Ubuntu: clang, lld), and the generated headers (build/gen: the generate
# step of tools/build.py runs when sk_fonts.h is missing).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
[ -f build/gen/sk_fonts.h ] || python3 tools/gen_pixfont.py build/gen/sk_fonts.h
OUT=build/web
mkdir -p "$OUT"
CC=${CC:-clang}
$CC --target=wasm32 -O2 -fno-builtin -nostdlib -Wall -Wno-unused-function \
    -Wl,--no-entry -Wl,--export-dynamic -Wl,-z,stack-size=262144 -Wl,--initial-memory=2097152 \
    -Ifirmware/src -Ibuild/gen -o "$OUT/skylynne.wasm" tools/web/sk_web.c
cp web/play/index.html web/play/play.js web/play/worklet.js "$OUT/"
# a new build is a new address: a browser cannot keep an old wasm or script after a rebuild
STAMP=$(cat "$OUT/skylynne.wasm" web/play/play.js web/play/worklet.js | shasum | cut -c1-10)
sed -i.bak "s|src=\"play.js\"|src=\"play.js?v=$STAMP\"|" "$OUT/index.html"
sed -i.bak -e "s|fetch(\"skylynne.wasm\")|fetch(\"skylynne.wasm?v=$STAMP\")|" -e "s|addModule(\"worklet.js\")|addModule(\"worklet.js?v=$STAMP\")|" "$OUT/play.js"
rm -f "$OUT/index.html.bak" "$OUT/play.js.bak"
echo "web: $OUT"
ls -l "$OUT" | tail -n +2
