#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Double-click helper for scripts/build-ocio-macos.sh (D080).
cd "$(dirname "$0")" || exit 1
./build-ocio-macos.sh "$@"
status=$?
echo
if [[ $status -eq 0 ]]; then
    echo "OpenColorIO is ready. Now run build-macos-ui.command or build-macos-ui.sh."
else
    echo "OpenColorIO build failed (exit $status). See the log above." >&2
fi
read -r -p "Press Return to close…" _
exit "$status"
