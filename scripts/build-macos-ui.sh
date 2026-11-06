#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the macOS SwiftUI shell and assemble a double-clickable .app.
#
# Usage:
#   scripts/build-macos-ui.sh [--debug] [--no-open] [--skip-cli] [--no-sign]
#
# Environment:
#   SPEKTRA_VERSION        CFBundleShortVersionString (default 0.0.1-alpha.1)
#   SPEKTRA_BUILD_NUMBER   CFBundleVersion (default 1)
#   SPEKTRA_SIGN_IDENTITY  "-" for ad-hoc (default) or a Developer ID name
#
# The app bundle contains:
#   Contents/MacOS/SpektraRoll
#   Contents/Helpers/spektra-roll-cli (+ ffmpeg/ffprobe when found)
#   Contents/Resources/data/{profiles,ocio}
#   Contents/Resources/AppIcon.icns
#   Contents/Resources/Fonts (optional; run scripts/fetch-macos-fonts.sh)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT/build-macos"
APP_DIR="$ROOT/build-ui-macos/SpektraRoll.app"
CONFIG="release"
OPEN=1
SKIP_CLI=0
NO_SIGN=0
VERSION="${SPEKTRA_VERSION:-0.0.1-alpha.1}"
BUILD_NUMBER="${SPEKTRA_BUILD_NUMBER:-1}"
SIGN_IDENTITY="${SPEKTRA_SIGN_IDENTITY:--}"

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
    CMAKE_ARGS=(-S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DSR_BUILD_TESTS=OFF)
    # OCIO is optional. Prefer a self-contained static prefix (D080); fall back
    # to Homebrew's package if it is installed. `OCIO_PREFIX` overrides both.
    OCIO_PREFIX="${OCIO_PREFIX:-$HOME/Documents/ocio-2.6.0-static}"
    CMAKE_PREFIX=""
    if [[ -f "$OCIO_PREFIX/include/OpenColorIO/OpenColorIO.h" ]]; then
        CMAKE_PREFIX="$OCIO_PREFIX"
        echo "==> OpenColorIO: static prefix $OCIO_PREFIX"
    elif command -v brew >/dev/null 2>&1 && brew --prefix opencolorio >/dev/null 2>&1; then
        CMAKE_PREFIX="$(brew --prefix)"
        echo "==> OpenColorIO: Homebrew package"
    else
        echo "==> OpenColorIO: not found (stub build; --ocio-* will be unavailable)"
    fi
    if [[ -n "$CMAKE_PREFIX" ]]; then
        CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$CMAKE_PREFIX")
        CMAKE_ARGS+=("-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON")
    fi
    cmake "${CMAKE_ARGS[@]}"
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
cp "$ROOT/LICENSE" "$APP_DIR/Contents/Resources/LICENSE"
cp "$ROOT/THIRD_PARTY_NOTICES.md" "$APP_DIR/Contents/Resources/THIRD_PARTY_NOTICES.md"

# App icon: commit the .icns; regenerate from the .iconset when missing.
ICON_SRC="$ROOT/ui-macos/Resources/AppIcon.icns"
if [[ ! -f "$ICON_SRC" && -d "$ROOT/ui-macos/Resources/AppIcon.iconset" ]]; then
    echo "==> Generating AppIcon.icns"
    iconutil -c icns "$ROOT/ui-macos/Resources/AppIcon.iconset" -o "$ICON_SRC"
fi
if [[ -f "$ICON_SRC" ]]; then
    cp "$ICON_SRC" "$APP_DIR/Contents/Resources/AppIcon.icns"
else
    echo "warning: AppIcon.icns missing; restore it from the repository." >&2
fi

if [[ -d "$ROOT/ui-macos/Resources/Fonts" ]]; then
    mkdir -p "$APP_DIR/Contents/Resources/Fonts"
    find "$ROOT/ui-macos/Resources/Fonts" -maxdepth 1 -type f \
        \( -name '*.otf' -o -name '*.ttf' -o -name '*.ttc' \) \
        -exec cp {} "$APP_DIR/Contents/Resources/Fonts/" \;
    if [[ -f "$ROOT/ui-macos/Resources/Fonts/OFL.txt" ]]; then
        cp "$ROOT/ui-macos/Resources/Fonts/OFL.txt" "$APP_DIR/Contents/Resources/Fonts/OFL.txt"
    fi
fi
if ! find "$APP_DIR/Contents/Resources/Fonts" -type f \( -name '*.otf' -o -name '*.ttf' -o -name '*.ttc' \) -print -quit 2>/dev/null | grep -q .; then
    echo "warning: no bundled font. Run scripts/fetch-macos-fonts.sh for Source Han Sans (Noto Sans CJK)." >&2
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

cat > "$APP_DIR/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>SpektraRoll</string>
    <key>CFBundleDisplayName</key>
    <string>SpektraRoll</string>
    <key>CFBundleIdentifier</key>
    <string>com.spektra-roll.app</string>
    <key>CFBundleVersion</key>
    <string>$BUILD_NUMBER</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleExecutable</key>
    <string>SpektraRoll</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.0</string>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.video</string>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2026 SpektraRoll contributors. GPL-3.0-or-later.</string>
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
    if [[ "$SIGN_IDENTITY" == "-" ]]; then
        echo "==> Ad-hoc signing (local use only)"
        SIGN_ARGS=(--force --timestamp=none --sign -)
    else
        echo "==> Signing with: $SIGN_IDENTITY"
        SIGN_ARGS=(--force --timestamp --options runtime --sign "$SIGN_IDENTITY")
    fi
    # Inside out: helpers first, then the bundle.
    while IFS= read -r -d '' file; do
        codesign "${SIGN_ARGS[@]}" "$file" >/dev/null 2>&1 || true
    done < <(find "$APP_DIR/Contents/Helpers" -type f -perm -111 -print0)
    codesign "${SIGN_ARGS[@]}" "$APP_DIR"
fi

echo
echo "App: $APP_DIR"
echo "Run: open \"$APP_DIR\""
if [[ "$OPEN" -eq 1 ]]; then
    open "$APP_DIR"
fi
