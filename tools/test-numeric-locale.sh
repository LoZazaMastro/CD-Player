#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
clang-18 -std=c11 -O1 -g -fsanitize=address,undefined -c src/runtime/c_locale.c -o build/c-locale-test.o
clang++-18 -std=c++20 -O1 -g -fsanitize=address,undefined -Isrc tests/numeric_locale_test.cpp build/c-locale-test.o -o build/numeric-locale-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/numeric-locale-test | tee build/numeric-locale-test.log
