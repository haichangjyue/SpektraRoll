#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build, sign and package the macOS app as a DMG (D083).
#
# Usage:
#   scripts/package-macos.sh [options]
#
# Options:
#   --version <ver>         marketing version        (default: 0.0.1-alpha.1)
#   --build <n>             CFBundleVersion          (default: 1)
#   --preview <tag>         optional filename tag    (default: empty)
#   --identity <name>       Developer ID identity; "-" = ad-hoc (default: -)
#   --notary-profile <p>    notarytool keychain profile (implies --identity is real)
#   --no-build              reuse build-ui-macos/SpektraRoll.app
#   --no-sign               skip signing (debug only)
#   -h, --help
#
# Outputs (in build-pkg/):
#   SpektraRoll-<version>[-<preview>]-<arch>.dmg
#   SHA256.txt
#
# Route A (today):  scripts/package-macos.sh
#   Ad-hoc signature. Users open the DMG, drag to Applications, then
#   right-click -> Open (or run xattr on first launch).
#
# Route B (later):  scripts/package-macos.sh \
#     --identity "Developer ID Application: NAME (TEAMID)" \
#     --notary-profile spektra-notary
#   Signs inside-out with hardened runtime, submits to Apple, staples the
#   ticket and verifies with spctl.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_DIR="$ROOT/build-ui-macos/SpektraRoll.app"
OUT_DIR="$ROOT/build-pkg"

VERSION="${SPEKTRA_VERSION:-0.0.1-alpha.1}"
BUILD_NUMBER="${SPEKTRA_BUILD_NUMBER:-1}"
PREVIEW_TAG="${SPEKTRA_PREVIEW_TAG:-}"
IDENTITY="${SPEKTRA_SIGN_IDENTITY:--}"
NOTARY_PROFILE="${SPEKTRA_NOTARY_PROFILE:-}"
DO_BUILD=1
DO_SIGN=1

usage() {
    sed -n '2,30p' "$0" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --build) BUILD_NUMBER="$2"; shift 2 ;;
        --preview) PREVIEW_TAG="$2"; shift 2 ;;
        --identity) IDENTITY="$2"; shift 2 ;;
        --notary-profile) NOTARY_PROFILE="$2"; shift 2 ;;
        --no-build) DO_BUILD=0; shift ;;
        --no-sign) DO_SIGN=0; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "This script must run on macOS." >&2
    exit 1
fi

ARCH="$(uname -m)"
if [[ -n "$PREVIEW_TAG" ]]; then
    DMG_NAME="SpektraRoll-${VERSION}-${PREVIEW_TAG}-${ARCH}.dmg"
else
    DMG_NAME="SpektraRoll-${VERSION}-${ARCH}.dmg"
fi
DMG_PATH="$OUT_DIR/$DMG_NAME"

# --- Build -------------------------------------------------------------------
if [[ "$DO_BUILD" -eq 1 ]]; then
    BUILD_ARGS=(--no-open)
    [[ "$DO_SIGN" -eq 0 ]] && BUILD_ARGS+=(--no-sign)
    SPEKTRA_VERSION="$VERSION" \
    SPEKTRA_BUILD_NUMBER="$BUILD_NUMBER" \
    SPEKTRA_SIGN_IDENTITY="$IDENTITY" \
        "$ROOT/scripts/build-macos-ui.sh" "${BUILD_ARGS[@]}"
fi

if [[ ! -d "$APP_DIR" ]]; then
    echo "App not found: $APP_DIR" >&2
    exit 1
fi

# --- Verify signature --------------------------------------------------------
if [[ "$DO_SIGN" -eq 1 ]]; then
    echo "==> Verifying code signature"
    codesign --verify --deep --strict --verbose=2 "$APP_DIR"
    codesign --display --verbose=4 "$APP_DIR" 2>&1 | sed -n '1,12p'
fi

# --- Stage -------------------------------------------------------------------
echo "==> Staging DMG contents"
STAGE="$ROOT/build-pkg/dmg-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"
ditto "$APP_DIR" "$STAGE/SpektraRoll.app"
ln -s /Applications "$STAGE/Applications"
if [[ -f "$ROOT/docs/release-macos.md" ]]; then
    cp "$ROOT/docs/release-macos.md" "$STAGE/安装说明.md"
fi

# --- Create DMG --------------------------------------------------------------
echo "==> Creating $DMG_NAME"
mkdir -p "$OUT_DIR"
rm -f "$DMG_PATH"
hdiutil create \
    -volname "SpektraRoll $VERSION" \
    -srcfolder "$STAGE" \
    -fs HFS+ \
    -format UDZO \
    -ov \
    "$DMG_PATH"

# --- Notarize (route B) ------------------------------------------------------
if [[ -n "$NOTARY_PROFILE" ]]; then
    if [[ "$IDENTITY" == "-" ]]; then
        echo "error: --notary-profile requires a real --identity" >&2
        exit 1
    fi
    echo "==> Submitting to Apple notary service"
    xcrun notarytool submit "$DMG_PATH" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait
    echo "==> Stapling ticket"
    xcrun stapler staple "$DMG_PATH"
    xcrun stapler validate "$DMG_PATH"
    echo "==> Gatekeeper assessment"
    spctl -a -vvv -t install "$DMG_PATH" || true
fi

# --- Checksum ----------------------------------------------------------------
echo "==> Writing SHA256.txt"
(
    cd "$OUT_DIR"
    shasum -a 256 "$DMG_NAME" > SHA256.txt
    cat SHA256.txt
)

echo
echo "DMG:  $DMG_PATH"
echo "SHA:  $OUT_DIR/SHA256.txt"
if [[ "$IDENTITY" == "-" ]]; then
    echo
    echo "This build is ad-hoc signed (route A). First launch:"
    echo "  1. Open the DMG and drag SpektraRoll to Applications."
    echo "  2. Right-click SpektraRoll -> Open -> Open."
    echo "  Or: xattr -dr com.apple.quarantine /Applications/SpektraRoll.app"
fi
