#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
objects=()
for object in build/preview-obj/*.o;do
  case "$object" in *host_preview.cpp.o)continue;;esac
  objects+=("$object")
done
clang++-18 -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror -DGL_GLEXT_PROTOTYPES=1 \
  -Isrc -I"$sysroot/usr/include" tests/screen_test.cpp "${objects[@]}" \
  -Wl,--gc-sections -L"$sysroot/usr/lib/x86_64-linux-gnu" -lEGL -l:libGL.so.1 -lpthread -lm -o build/screen-test
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES="$sysroot/usr/share/glvnd/egl_vendor.d/50_mesa.json" \
 LIBGL_DRIVERS_PATH="$sysroot/usr/lib/x86_64-linux-gnu/dri" build/screen-test | tee build/screen-test.log
