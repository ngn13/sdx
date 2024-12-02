#!/bin/bash

# compiles necessary tools for the sdx build process

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

PATH="$prefix/bin:$PATH"
prefix="/opt/cross"
target="x86_64-elf"

info "Creating ${prefix}"
$SUDO mkdir -p "${prefix}"

mkdir -p /tmp/binutils-build && pushd /tmp/binutils-build
  info "Downloading binutils 2.32.2"

  download https://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.gz
  check_ret "Failed to download binutils"

  check_sha256 binutils-2.23.2.tar.gz d42cb1d82ead40ef55edd978d31c22f2d105712fc3a280dbba42cf7a316d0d56
  check_ret

  info "Extracting binutils"
  tar xf binutils-2.23.2.tar.gz
  mkdir -p binutils-2.23.2/bld && cd binutils-2.23.2/bld

  info "Running ./configure and make"
  ../configure --target="${target}"     \
              --prefix="${prefix}"      \
              --with-sysroot            \
              --disable-nls             \
              --disable-werror

  make -j$(nproc) && info "Running install scripts"
  $SUDO make install
popd && rm -rf /tmp/binutils-build

mkdir -p /tmp/gcc-build && pushd /tmp/gcc-build
  info "Downloading gcc 13.1.0"

  download https://ftp.gnu.org/gnu/gcc/gcc-13.1.0/gcc-13.1.0.tar.gz
  check_ret "Failed to download binutils"

  check_sha256 gcc-13.1.0.tar.gz bacd4c614d8bd5983404585e53478d467a254249e0f1bb747c8bc6d787bd4fa2
  check_ret

  info "Extracting gcc"
  tar xf gcc-13.1.0.tar.gz
  mkdir -p gcc-13.1.0/bld && cd gcc-13.1.0/bld

  info "Running ./configure and make"
  ../configure --target="${target}"     \
               --prefix="${prefix}"     \
               --disable-nls            \
               --enable-languages=c,c++ \
               --without-headers

  make -j$(nproc) all-gcc
  make -j$(nproc) all-target-libgcc

  info "Running install scripts"
  $SUDO make install-gcc
  $SUDO make install-target-libgcc
popd && rm -rf /tmp/gcc-build
