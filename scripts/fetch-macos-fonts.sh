#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Downloads Noto Sans CJK SC (the Google OFL release of the Source Han Sans
# design) into ui-macos/Resources/Fonts so the app can bundle it.
# Fonts are NOT committed to the repository; the build script copies them when
# present. Licence: SIL Open Font License 1.1 (see Resources/Fonts/README.md).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/ui-macos/Resources/Fonts"
mkdir -p "$DEST"

BASE="https://github.com/notofonts/noto-cjk/raw/main/Sans/OTF/SimplifiedChinese"
FILES=(
  "NotoSansCJKsc-Regular.otf"
  "NotoSansCJKsc-Bold.otf"
)

for file in "${FILES[@]}"; do
    if [[ -f "$DEST/$file" ]]; then
        echo "already present: $file"
        continue
    fi
    echo "downloading $file"
    curl -L --fail --retry 3 -o "$DEST/$file" "$BASE/$file"
done

echo "Fonts: $DEST"
echo "Now rebuild the app: scripts/build-macos-ui.sh"
