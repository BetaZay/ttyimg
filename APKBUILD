# Maintainer: Your Name <zay@betazay.com>
pkgname=ttyimg
pkgver=1.0
pkgrel=0
pkgdesc="DRM-based live webcam overlay for TTY"
url="https://example.com/ttyimg"
arch="x86_64"
license="MIT"
depends=""
makedepends="cmake pkgconf libdrm-dev"
source="https://example.com/ttyimg-${pkgver}.tar.gz"
sha512sums="SKIP"  # Replace SKIP with the actual checksum if available

build() {
  cd "$srcdir/ttyimg-${pkgver}"
  mkdir -p build
  cd build
  cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXE_LINKER_FLAGS="-static"
  make
}

package() {
  cd "$srcdir/ttyimg-${pkgver}/build"
  # Install the binary to /usr/bin
  install -Dm755 ttyimg "$pkgdir/usr/bin/ttyimg"
  # Also install the text image if your program expects it in the same directory
  install -Dm644 ../txt.png "$pkgdir/usr/share/ttyimg/txt.png"
}
