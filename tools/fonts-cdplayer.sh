#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
${HOST_CXX:-clang++-18} -std=c++20 -O2 -w tools/font-baker/bake_font.cpp -o build/host/bake_font
build/host/bake_font third_party/fonts/Inter-Regular.ttf assets/fonts/inter-regular.huifont 56 8 2048 european
build/host/bake_font third_party/fonts/Inter-SemiBold.ttf assets/fonts/inter-semibold.huifont 56 8 2048 european
build/host/bake_font third_party/fonts/Doto-SemiBold.ttf assets/fonts/doto-semibold.huifont 80 6 2048 european
build/host/bake_font third_party/fonts/DejaVuSansMono.ttf assets/fonts/visualizer-mono.huifont 52 8 2048 visualizer
build/host/bake_font third_party/fonts/DottedSongtiSquareRegular.otf assets/fonts/dotted-songti.huifont 24 2 4096 chinese
cp third_party/fonts/DejaVu-LICENSE.txt assets/fonts/DejaVu-LICENSE.txt
cp third_party/fonts/DottedSongti-LICENSE.txt assets/fonts/DottedSongti-LICENSE.txt
