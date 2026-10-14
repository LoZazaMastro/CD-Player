#!/usr/bin/env bash
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
sysroot="$root/build/host-sysroot"
export PATH="$sysroot/usr/bin:$sysroot/usr/lib/llvm-18/bin:$PATH"
export LD_LIBRARY_PATH="$sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export HOST_CXX=clang++-18 HOST_CC=clang-18 PS5_CLANG=clang-18 USE_CCACHE=0
export TMPDIR="$root/build/tmp"
mkdir -p "$TMPDIR"
