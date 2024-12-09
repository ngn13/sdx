#!/bin/bash

# compiles necessary tools for the sdx build process

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

prefix="/opt/cross"
target="x86_64-elf"

binutils_data=("2.43" "025c436d15049076ebe511d29651cc4785ee502965a8839936a65518582bdd64")
gcc_data=("14.2.0" "7d376d445f93126dc545e2c0086d0f647c3094aae081cdb78f42ce2bc25e7293")

PATH="$prefix/bin:$PATH"

info "Creating ${prefix}"
$SUDO mkdir -p "${prefix}"

mkdir -p /tmp/binutils-build && pushd /tmp/binutils-build
  source_dir="binutils-${binutils_data[0]}"
  source_archive="${source_dir}.tar.gz"

  info "Downloading ${source_archive}"
  download "https://ftp.gnu.org/gnu/binutils/${source_archive}"
  check_ret "Failed to download ${source_archive}"

  info "Verifying ${source_archive}"
  check_sha256 "${source_archive}" "${binutils_data[1]}"
  check_ret

  info "Extracting ${source_archive}"
  tar xf "${source_archive}"
  check_ret "Failed to extract ${source_archive}"

  mkdir -p "${source_dir}/bld" && cd "${source_dir}/bld"

  info "Running ./configure and make"
  ../configure --target="${target}"     \
              --prefix="${prefix}"      \
              --with-sysroot            \
              --disable-nls             \
              --disable-werror
  check_ret "Configure failed for binutils"

  make -j$(nproc)
  check_ret "Binutils build failed"

  info "Running installs"

  $SUDO make install
  check_ret "Binutils install failed"
popd && rm -rf /tmp/binutils-build

mkdir -p /tmp/gcc-build && pushd /tmp/gcc-build
  source_dir="gcc-${gcc_data[0]}"
  source_archive="${source_dir}.tar.gz"

  info "Downloading ${source_archive}"
  download "https://ftp.gnu.org/gnu/gcc/gcc-${gcc_data[0]}/${source_archive}"
  check_ret "Failed to download ${source_archive}"

  info "Verifying ${source_archive}"
  check_sha256 "${source_archive}" "${gcc_data[1]}"
  check_ret

  info "Extracting ${source_archive}"
  tar xf "${source_archive}"
  check_ret "Failed to extract ${source_archive}"

  mkdir -p "${source_dir}/bld" && cd "${source_dir}/bld"

  info "Running ./configure and make"
  ../configure --target="${target}"     \
               --prefix="${prefix}"     \
               --disable-nls            \
               --enable-languages=c,c++ \
               --without-headers
  check_ret "Configure failed for GCC"

  make -j$(nproc) all-gcc
  check_ret "GCC build failed"

  make -j$(nproc) all-target-libgcc
  check_ret "LibGCC build failed"

  info "Running installs"

  $SUDO make install-gcc
  check_ret "GGC install failed"

  $SUDO make install-target-libgcc
  check_ret "LibGCC install failed"
popd && rm -rf /tmp/gcc-build
