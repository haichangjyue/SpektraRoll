#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click wrapper for scripts/package-macos.sh (D083 route A).
set -euo pipefail
export PATH=/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin
cd "$(dirname "$0")/.."
./scripts/package-macos.sh "$@"
echo
read -n 1 -s -r -p "按任意键关闭窗口…"
