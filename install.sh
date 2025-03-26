#!/bin/sh

set -e

APP_NAME="ttyimg"
BIN_DIR="/usr/local/bin"
SHARE_DIR="/usr/local/share/${APP_NAME}"
REPO_URL="https://github.com/BetaZay/ttyimg"
ARCHIVE_URL="https://github.com/BetaZay/ttyimg/releases/latest/download/${APP_NAME}.tar.gz"

echo "[*] Installing ${APP_NAME}..."

# 1. Create temp directory
TMP_DIR=$(mktemp -d)
cd "$TMP_DIR"

# 2. Download release tarball
echo "[*] Downloading latest release..."
wget -q --show-progress "$ARCHIVE_URL" -O "${APP_NAME}.tar.gz"

# 3. Extract
tar -xzf "${APP_NAME}.tar.gz"

# 4. Install binary
echo "[*] Installing binary to ${BIN_DIR}..."
install -Dm755 ${APP_NAME} "${BIN_DIR}/${APP_NAME}"

# 5. Install assets (like txt.png)
echo "[*] Installing assets to ${SHARE_DIR}..."
mkdir -p "${SHARE_DIR}"
install -Dm644 txt.png "${SHARE_DIR}/txt.png"

# 6. Cleanup
cd /
rm -rf "$TMP_DIR"

echo "[✓] ${APP_NAME} installed successfully."
