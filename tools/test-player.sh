#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
clang++-18 -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined \
  -Isrc tests/player_test.cpp src/cd/disc.cpp src/cd/album_info.cpp src/cd/sound.cpp src/cd/dsp.cpp src/cd/player.cpp \
  src/core/save_file.cpp src/third_party/stb/image.cpp host/platform_host.cpp -lpthread -o build/player-test
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 build/player-test | tee build/player-test.log
