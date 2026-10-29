#!/usr/bin/env bash
# Builds a bare-metal mipsel-none-elf binutils + GCC (C only) and armips into $PREFIX.
set -euo pipefail
PREFIX=${PREFIX:-$HOME/.local/opt/psx}
WORK=${WORK:-$HOME/.cache/psx-toolchain-build}
BINUTILS=2.45
GCC=15.2.0
JOBS=$(nproc)
# host GCC 16 defaults to C23/C++20, which older GNU sources do not build under
export CFLAGS="-O2 -std=gnu17" CXXFLAGS="-O2 -fno-char8_t"
mkdir -p "$PREFIX" "$WORK"
cd "$WORK"
fetch() { [ -f "$(basename "$1")" ] || curl -fL --retry 3 -O "$1"; }
fetch https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS.tar.xz
fetch https://ftp.gnu.org/gnu/gcc/gcc-$GCC/gcc-$GCC.tar.xz
[ -d binutils-$BINUTILS ] || tar xf binutils-$BINUTILS.tar.xz
[ -d gcc-$GCC ] || tar xf gcc-$GCC.tar.xz

if [ ! -x "$PREFIX/bin/mipsel-none-elf-as" ]; then
  rm -rf build-binutils && mkdir build-binutils && cd build-binutils
  ../binutils-$BINUTILS/configure --prefix="$PREFIX" --target=mipsel-none-elf \
    --disable-docs --disable-nls --disable-werror --with-float=soft
  make -j"$JOBS"
  make install-strip
  cd ..
fi
export PATH="$PREFIX/bin:$PATH"
if [ ! -x "$PREFIX/bin/mipsel-none-elf-gcc" ]; then
  rm -rf build-gcc && mkdir build-gcc && cd build-gcc
  ../gcc-$GCC/configure --prefix="$PREFIX" --target=mipsel-none-elf \
    --disable-docs --disable-nls --disable-libada --disable-libssp --disable-libquadmath \
    --disable-libstdc++-v3 --disable-threads --disable-shared --disable-multilib \
    --with-float=soft --with-arch=r3000 --enable-languages=c --without-headers --with-gnu-as --with-gnu-ld
  make -j"$JOBS" all-gcc all-target-libgcc
  make install-strip-gcc install-strip-target-libgcc
  cd ..
fi
if [ ! -x "$PREFIX/bin/armips" ]; then
  rm -rf armips && git clone --depth 1 --recursive https://github.com/Kingcom/armips.git
  cmake -S armips -B armips/build -DCMAKE_BUILD_TYPE=Release && cmake --build armips/build -j"$JOBS"
  mkdir -p "$PREFIX/bin"
  cp armips/build/armips "$PREFIX/bin/"
fi
echo "Toolchain ready in $PREFIX/bin"
