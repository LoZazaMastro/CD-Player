#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
source tools/ninja-build.sh
ninja_begin "$root/build/preview.ninja"
objects=()
while IFS= read -r -d '' source;do
  object="build/preview-obj/${source//\//_}.o"
  mkdir -p "$(dirname "$object")"
  ninja_inputs=("$source")
  ninja_edge CXX "$object" clang++-18 -std=c++20 -O2 -Wall -Wextra -Wpedantic -ffunction-sections -fdata-sections \
    -DGL_GLEXT_PROTOTYPES=1 -Isrc -I"$sysroot/usr/include" -MD -MF "$object.d" -c "$source" -o "$object"
  objects+=("$object")
done < <(find src host -name '*.cpp' ! -path 'src/platform/*' ! -path 'src/runtime/*' ! -name main.cpp -print0)
for source in src/third_party/quickjs/*.c;do
  object="build/preview-obj/${source//\//_}.o"
  ninja_inputs=("$source")
  ninja_edge CC "$object" clang-18 -std=gnu11 -O2 -w -ffunction-sections -fdata-sections -D_GNU_SOURCE -Isrc/third_party/quickjs -MD -MF "$object.d" -c "$source" -o "$object"
  objects+=("$object")
done
ninja_inputs=("${objects[@]}")
ninja_edge LINK build/cdplayer-preview clang++-18 "${objects[@]}" -Wl,--gc-sections -L"$sysroot/usr/lib/x86_64-linux-gnu" -lEGL -l:libGL.so.1 -lpthread -lm -o build/cdplayer-preview
ninja_run > build/preview-build.log 2>&1
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
 __EGL_VENDOR_LIBRARY_FILENAMES="$sysroot/usr/share/glvnd/egl_vendor.d/50_mesa.json" \
 LIBGL_DRIVERS_PATH="$sysroot/usr/lib/x86_64-linux-gnu/dri" build/cdplayer-preview
