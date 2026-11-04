#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click me on macOS to build and open the SwiftUI app.
set -e
cd "$(dirname "$0")/.."
exec ./scripts/build-macos-ui.sh "$@"
