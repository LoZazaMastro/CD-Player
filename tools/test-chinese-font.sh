#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
${HOST_CXX:-clang++-18} -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror -Isrc tests/chinese_font_test.cpp src/gfx/font.cpp -o build/chinese-font-test
build/chinese-font-test | tee build/chinese-font-test.log

