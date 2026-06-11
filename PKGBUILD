# Maintainer: Arnaud <arnauda-gh>
pkgname=alwide-git
pkgver=r0.g1
pkgrel=1
pkgdesc="A LightWeight IDE for the terminal (Sublime Text vibe in TUI)"
arch=('x86_64')
url="https://github.com/arnauda-gh/Alwide"
license=('MIT')
depends=('ncurses')
makedepends=('git' 'clang' 'rust' 'cargo')
provides=('alwide')
conflicts=('alwide')
source=("git+https://github.com/arnauda-gh/Alwide.git")
md5sums=('SKIP')

pkgver() {
  cd "$srcdir/Alwide"
  printf "r%s.g%s" "$(git rev-list --count HEAD)" "$(git rev-parse --short HEAD)"
}

build() {
  cd "$srcdir/Alwide"
  # Fetch submodules if not already done by makepkg
  git submodule update --init --recursive
  make release
}

package() {
  cd "$srcdir/Alwide"
  make DESTDIR="$pkgdir" PREFIX="/usr" install
}
