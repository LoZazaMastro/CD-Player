#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
bash tools/prepare-build.sh
make_jobs_flag=""
if make --help 2>&1 | grep -q jobserver-style; then make_jobs_flag="--jobserver-style=pipe"; fi
make $make_jobs_flag app > build/ps5-build.log 2>&1
if [[ ! -f build/helper/lapy.elf || ! -f build/helper/lapy-manifest.json ]]; then
    bash tools/helper-build.sh
fi
python3 tools/package-cdplayer.py
