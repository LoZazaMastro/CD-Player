#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
python3 tools/build-lapy-helper.py PPSA98427 build/helper > build/helper-build.log 2>&1
