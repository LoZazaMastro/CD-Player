#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
objects=()
for object in build/preview-obj/*.o;do
  case "$object" in *host_preview.cpp.o|*src_app_archive_studies.cpp.o|*src_gfx_draw_list.cpp.o)continue;;esac
  objects+=("$object")
done
clang++-18 -std=c++20 -O1 -g -fsanitize=address,undefined -ffunction-sections -fdata-sections -Isrc \
  tests/archive_test.cpp src/app/archive_studies.cpp src/gfx/draw_list.cpp "${objects[@]}" \
  -Wl,--gc-sections -L"$sysroot/usr/lib/x86_64-linux-gnu" -lEGL -l:libGL.so.1 -lpthread -lm -o build/archive-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/archive-test 2>&1 | tee build/archive-test.log
