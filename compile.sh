#!/bin/sh

set -e

APP_NAME="ttyimg"
BUILD_DIR="build"
DIST_DIR="dist"
TARBALL="${APP_NAME}.tar.gz"

echo "[*] Building ${APP_NAME}..."

# 1. Create build directory and build
mkdir -p $BUILD_DIR
cd $BUILD_DIR
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ..

# 2. Create dist directory
mkdir -p $DIST_DIR

# 3. Copy binary and assets to dist
cp "$BUILD_DIR/$APP_NAME" "$DIST_DIR/"
cp txt.png "$DIST_DIR/"

# 4. Create tarball
tar -czf $TARBALL -C $DIST_DIR .

echo "[✓] Build complete: $TARBALL"
