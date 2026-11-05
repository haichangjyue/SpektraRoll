#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# D080: build a self-contained static OpenColorIO prefix for macOS.
#
# Why: Homebrew's opencolorio on macOS 14 tries to build python@3.15 for the
# Python bindings and fails with clang's tail-call error. The core library does
# not need Python. This script builds OCIO with Python, apps, tests and docs
# off, links everything statically, and installs into a local prefix that
# scripts/build-macos-ui.sh picks up automatically.
#
# Usage:
#   scripts/build-ocio-macos.sh [version]
#
# Env overrides:
#   OCIO_PREFIX  install prefix (default: ~/Documents/ocio-<version>-static)
#   OCIO_SRC     source dir     (default: ~/Documents/ocio-<version>-src)
#   OCIO_BUILD   build dir      (default: ~/Documents/ocio-<version>-build)

set -euo pipefail

VERSION="${1:-2.6.0}"
export PATH="/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin"
export CMAKE_POLICY_VERSION_MINIMUM=3.5

PREFIX="${OCIO_PREFIX:-$HOME/Documents/ocio-${VERSION}-static}"
SRC="${OCIO_SRC:-$HOME/Documents/ocio-${VERSION}-src}"
BUILD="${OCIO_BUILD:-$HOME/Documents/ocio-${VERSION}-build}"
CACHE_TARBALL="$HOME/Library/Caches/Homebrew/opencolorio--${VERSION}.tar.gz"
DOWNLOAD="$HOME/Library/Caches/SpektraRoll/OpenColorIO-${VERSION}.tar.gz"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake not found. Install it with: brew install cmake" >&2
    exit 1
fi

echo "OpenColorIO $VERSION"
echo "  source: $SRC"
echo "  build:  $BUILD"
echo "  prefix: $PREFIX"

rm -rf "$SRC" "$BUILD" "$PREFIX"
mkdir -p "$SRC" "$BUILD" "$PREFIX"

if [[ -f "$CACHE_TARBALL" ]]; then
    TARBALL="$CACHE_TARBALL"
elif [[ -f "$DOWNLOAD" ]]; then
    TARBALL="$DOWNLOAD"
else
    mkdir -p "$(dirname "$DOWNLOAD")"
    echo "==> Downloading OpenColorIO $VERSION"
    curl -fL --retry 3 -o "$DOWNLOAD" \
        "https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v${VERSION}.tar.gz"
    TARBALL="$DOWNLOAD"
fi

echo "==> Extracting"
tar -xzf "$TARBALL" -C "$SRC" --strip-components=1

echo "==> Configuring (static, no Python/apps/tests)"
cmake -S "$SRC" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DOCIO_INSTALL_EXT_PACKAGES=ALL \
    -DOCIO_BUILD_PYTHON=OFF \
    -DOCIO_BUILD_APPS=OFF \
    -DOCIO_BUILD_TESTS=OFF \
    -DOCIO_BUILD_GPU_TESTS=OFF \
    -DOCIO_BUILD_DOCS=OFF \
    -DBUILD_SHARED_LIBS=OFF

echo "==> Building (several minutes)"
cmake --build "$BUILD" --parallel "$(sysctl -n hw.ncpu)"
cmake --install "$BUILD"

# OCIO's install target writes libOpenColorIO.a but not the external static
# libraries it was configured with. Copy them into the prefix so a plain
# `CMAKE_PREFIX_PATH=$PREFIX` is enough.
if [[ -d "$BUILD/ext/dist/lib" ]]; then
    cp "$BUILD/ext/dist/lib"/*.a "$PREFIX/lib/" 2>/dev/null || true
fi
if [[ -d "$BUILD/ext/dist/include" ]]; then
    mkdir -p "$PREFIX/include"
    cp -R "$BUILD/ext/dist/include/." "$PREFIX/include/" 2>/dev/null || true
fi
if [[ -d "$BUILD/ext/dist/lib/cmake" ]]; then
    mkdir -p "$PREFIX/lib/cmake"
    cp -R "$BUILD/ext/dist/lib/cmake/." "$PREFIX/lib/cmake/"
    # OCIO's bundled zlib config points at dylibs we do not copy; let CMake's
    # FindZLIB use the system libz instead.
    rm -rf "$PREFIX/lib/cmake/zlib"
fi

# OCIO's ext build does not install a pystring package config (only the static
# archive), so find_dependency(pystring) would otherwise pick Homebrew's dylib.
PYSTRING_CMAKE="$PREFIX/lib/cmake/pystring"
mkdir -p "$PYSTRING_CMAKE"
cat > "$PYSTRING_CMAKE/pystringConfig.cmake" <<'PYSTRING_EOF'
if(NOT TARGET pystring::pystring)
    add_library(pystring::pystring STATIC IMPORTED)
    set_target_properties(pystring::pystring PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_LIST_DIR}/../../../lib/libpystring.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/../../../include")
endif()
PYSTRING_EOF
cat > "$PYSTRING_CMAKE/pystringConfigVersion.cmake" <<'PYSTRING_EOF'
set(PACKAGE_VERSION "1.2.0")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
PYSTRING_EOF


echo "==> Done"
ls -lh "$PREFIX/lib"
echo
echo "Build SpektraRoll with:"
echo "  OCIO_PREFIX=\"$PREFIX\" scripts/build-macos-ui.sh"
