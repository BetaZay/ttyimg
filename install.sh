#!/bin/sh

set -e

APP_NAME="ttyimg"
REPO_URL="https://github.com/BetaZay/ttyimg.git"
BIN_DIR="/usr/local/bin"
SHARE_DIR="/usr/local/share/${APP_NAME}"

# Check if script is run as root
if [ "$(id -u)" -ne 0 ]; then
  echo "❌ This script must be run as root. Try:"
  echo ""
  echo "  curl -sSL https://github.com/BetaZay/ttyimg/raw/main/install.sh | sudo sh"
  echo ""
  exit 1
fi

# Detect package manager and install deps
echo "[*] Installing build dependencies..."

if command -v apt >/dev/null 2>&1; then
  apt update
  apt install -y build-essential cmake pkg-config libdrm-dev
elif command -v apk >/dev/null 2>&1; then
  apk update
  apk add build-base cmake pkgconf libdrm-dev git
else
  echo "❌ Unsupported package manager. Please install dependencies manually:"
  echo "   - cmake, pkg-config, libdrm-dev, build tools"
  exit 1
fi

# Build from source
echo "[*] Cloning latest source..."
TMP_DIR=$(mktemp -d)
cd "$TMP_DIR"
git clone --depth=1 "$REPO_URL" "${APP_NAME}"
cd "${APP_NAME}"

echo "[*] Building ${APP_NAME}..."
mkdir -p build
cd build
cmake ..
make

echo "[*] Installing binary to ${BIN_DIR}..."
install -Dm755 "${APP_NAME}" "${BIN_DIR}/${APP_NAME}"

echo "[*] Installing assets to ${SHARE_DIR}..."
mkdir -p "${SHARE_DIR}"
install -Dm644 ../txt.png "${SHARE_DIR}/txt.png"

echo "[*] Cleaning up..."
cd /
rm -rf "$TMP_DIR"

echo "[✓] ${APP_NAME} installed successfully."
