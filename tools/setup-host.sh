#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$root/build/host-debs" "$root/build/host-sysroot"
cd "$root/build/host-debs"
apt-get download clang-18 libclang-cpp18 libclang-common-18-dev libclang1-18 libllvm18 \
  libclang-rt-18-dev llvm-18-linker-tools llvm-18-tools ninja-build libegl-dev libgl-dev \
  libegl1 libegl-mesa0 libgbm1 mesa-libgallium libglx-dev libopengl-dev libgles-dev \
  libglvnd-dev libgles1 libgles2 libglx0 libopengl0 > "$root/build/host-dependencies.log" 2>&1
for pkg in *.deb; do dpkg-deb -x "$pkg" "$root/build/host-sysroot"; done
