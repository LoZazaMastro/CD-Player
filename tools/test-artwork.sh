#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
clang++-18 -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined \
  -Isrc tests/artwork_test.cpp src/cd/artwork.cpp src/third_party/stb/image.cpp -o build/artwork-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/artwork-test | tee build/artwork-test.log
