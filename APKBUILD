# Maintainer: Isaiah Clark <zay@betazay.com>
pkgname=ttyimg
pkgver=1.0
pkgrel=0
pkgdesc="DRM-based live webcam overlay for TTY"
url="https://github.com/BetaZay/ttyimg"
arch="x86_64"
license="MIT"
options="!check"
depends=""
makedepends="cmake pkgconf libdrm-dev"
source="ttyimg-1.0.tar.gz"
sha512sums="SKIP"

build() {
  cd "$srcdir/ttyimg-${pkgver}"
  mkdir -p build
  cd build
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make
}

package() {
    cd "$srcdir/ttyimg-${pkgver}/build"
    ls -la  # Debug: List contents of build folder
    install -Dm755 ./ttyimg "$pkgdir/usr/bin/ttyimg"
    install -Dm644 ../txt.png "$pkgdir/usr/share/ttyimg/txt.png"
}
