#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root/build/host-debs"
apt-get download lld-18 llvm-18 > "$root/build/linker-dependencies.log" 2>&1
for p in lld-18*.deb llvm-18_*.deb;do dpkg-deb -x "$p" "$sysroot";done
