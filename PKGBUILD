pkgname=toggle-git
pkgver=0.0.0
pkgrel=1
pkgdesc="Minimal drawing app built with raylib"
arch=("x86_64")
url="https://github.com/Protyasha-Roy/toggle"
license=("MIT")
depends=("raylib")
makedepends=("cmake" "ninja" "git")
provides=("toggle")
conflicts=("toggle")
source=("git+$url.git")
sha256sums=("SKIP")

pkgver() {
  cd "$srcdir/toggle"
  git describe --tags --long --always | sed 's/^v//;s/-/./g'
}

build() {
  cmake -S "$srcdir/toggle" -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build
}

package() {
  cmake --install build --prefix "$pkgdir/usr"
}
