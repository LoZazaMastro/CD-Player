#!/usr/bin/env bash
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
sysroot="$root/build/host-sysroot"
for gnupath in /opt/homebrew/opt/coreutils/libexec/gnubin /opt/homebrew/opt/gnu-tar/libexec/gnubin /opt/homebrew/opt/gnu-sed/libexec/gnubin /opt/homebrew/opt/llvm@18/bin; do
    if [[ -d $gnupath ]]; then
        PATH="$gnupath:$PATH"
    fi
done
export PATH="$sysroot/usr/bin:$sysroot/usr/lib/llvm-18/bin:$PATH"
export LD_LIBRARY_PATH="$sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export HOST_CXX=clang++-18 HOST_CC=clang-18 PS5_CLANG=clang-18 USE_CCACHE=0
if ! command -v clang++-18 >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
    export HOST_CXX=clang++
fi
if ! command -v clang-18 >/dev/null 2>&1 && command -v clang >/dev/null 2>&1; then
    export HOST_CC=clang
    export PS5_CLANG=clang
fi
export TMPDIR="$root/build/tmp"
mkdir -p "$TMPDIR"
