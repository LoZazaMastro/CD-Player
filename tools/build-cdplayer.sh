#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
bash tools/prepare-build.sh
make --jobserver-style=pipe app > build/ps5-build.log 2>&1
if [[ ! -f build/helper/lapy.elf || ! -f build/helper/lapy-manifest.json ]]; then
    bash tools/helper-build.sh
fi
python3 tools/package-cdplayer.py
