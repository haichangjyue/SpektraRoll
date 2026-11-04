#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the macOS SwiftUI shell and assemble a double-clickable .app.
#
# Usage:
#   scripts/build-macos-ui.sh [--debug] [--no-open] [--skip-cli] [--no-sign]
#
# The app bundle contains:
#   Contents/MacOS/SpektraRoll
#   Contents/Helpers/spektra-roll-cli (+ ffmpeg/ffprobe when found)
#   Contents/Resources/data/{profiles,ocio}
#   Contents/Resources/Fonts (optional; run scripts/fetch-macos-fonts.sh)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-macos"
APP_DIR="$ROOT/build-ui-macos/SpektraRoll.app"
CONFIG="release"
OPEN=1
SKIP_CLI=0
NO_SIGN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug) CONFIG="debug"; shift ;;
        --no-open) OPEN=0; shift ;;
        --skip-cli) SKIP_CLI=1; shift ;;
        --no-sign) NO_SIGN=1; shift ;;
        *)
            echo "unknown option: $1" >&2
            exit 2
            ;;
    esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi
if ! command -v swift >/dev/null 2>&1; then
    echo "swift not found. Install Xcode 16 + command line tools." >&2
    exit 1
fi

if [[ "$SKIP_CLI" -eq 0 ]]; then
    echo "==> Building spektra-roll-cli"
    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake not found. Install it with: brew install cmake" >&2
        exit 1
    fi
    cmake -S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DSR_BUILD_TESTS=OFF
    cmake --build "$BUILD_DIR" -j "$(sysctl -n hw.ncpu)"
fi

echo "==> Building SwiftUI app"
swift build --package-path "$ROOT/ui-macos" -c "$CONFIG"
BIN_DIR="$(swift build --package-path "$ROOT/ui-macos" -c "$CONFIG" --show-bin-path)"
APP_BIN="$BIN_DIR/SpektraRoll"
if [[ ! -x "$APP_BIN" ]]; then
    echo "SwiftUI binary missing: $APP_BIN" >&2
    exit 1
fi

echo "==> Assembling $APP_DIR"
rm -rf "$APP_DIR"
mkdir -p "$APP_DIR/Contents/MacOS"
mkdir -p "$APP_DIR/Contents/Helpers"
mkdir -p "$APP_DIR/Contents/Resources/data"
cp "$APP_BIN" "$APP_DIR/Contents/MacOS/SpektraRoll"

CLI_SOURCE=""
if [[ -x "$BUILD_DIR/spektra-roll-cli" ]]; then
    CLI_SOURCE="$BUILD_DIR/spektra-roll-cli"
elif [[ -x "$ROOT/build/spektra-roll-cli" ]]; then
    CLI_SOURCE="$ROOT/build/spektra-roll-cli"
fi
if [[ -n "$CLI_SOURCE" ]]; then
    cp "$CLI_SOURCE" "$APP_DIR/Contents/Helpers/spektra-roll-cli"
else
    echo "warning: no spektra-roll-cli found; the app will use SPEKTRA_CLI or PATH." >&2
fi

cp -R "$ROOT/data/profiles" "$APP_DIR/Contents/Resources/data/"
cp -R "$ROOT/data/ocio" "$APP_DIR/Contents/Resources/data/"

if [[ -d "$ROOT/ui-macos/Resources/Fonts" ]]; then
    mkdir -p "$APP_DIR/Contents/Resources/Fonts"
    find "$ROOT/ui-macos/Resources/Fonts" -maxdepth 1 -type f \
        \( -name '*.otf' -o -name '*.ttf' -o -name '*.ttc' \) \
        -exec cp {} "$APP_DIR/Contents/Resources/Fonts/" \;
fi

FFMPEG="${SPEKTRA_FFMPEG:-}"
if [[ -z "$FFMPEG" ]]; then
    FFMPEG="$(command -v ffmpeg || true)"
fi
if [[ -n "$FFMPEG" && -x "$FFMPEG" ]]; then
    cp "$FFMPEG" "$APP_DIR/Contents/Helpers/ffmpeg"
    FFPROBE="$(dirname "$FFMPEG")/ffprobe"
    if [[ -x "$FFPROBE" ]]; then
        cp "$FFPROBE" "$APP_DIR/Contents/Helpers/ffprobe"
    fi
else
    echo "warning: ffmpeg not found. Install with 'brew install ffmpeg' or set SPEKTRA_FFMPEG." >&2
fi

cat > "$APP_DIR/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>SpektraRoll</string>
    <key>CFBundleDisplayName</key>
    <string>SpektraRoll</string>
    <key>CFBundleIdentifier</key>
    <string>com.spektra-roll.app</string>
    <key>CFBundleVersion</key>
    <string>0.1.0</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>SpektraRoll</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
</dict>
</plist>
PLIST
printf 'APPL????' > "$APP_DIR/Contents/PkgInfo"

if [[ "$NO_SIGN" -eq 0 ]]; then
    echo "==> Ad-hoc signing (local use only)"
    while IFS= read -r -d '' file; do
        codesign --force --timestamp=none --sign - "$file" >/dev/null 2>&1 || true
    done < <(find "$APP_DIR/Contents/Helpers" -type f -perm -111 -print0)
    codesign --force --timestamp=none --sign - "$APP_DIR" >/dev/null 2>&1 || true
fi

echo
echo "App: $APP_DIR"
echo "Run: open \"$APP_DIR\""
if [[ "$OPEN" -eq 1 ]]; then
    open "$APP_DIR"
fi
