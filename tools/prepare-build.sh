#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/host-env.sh"
cd "$root"
bash tools/setup-native-dependencies.sh
bash tools/prepare-opengl.sh
