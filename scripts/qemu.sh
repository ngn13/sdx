#!/bin/bash

# runs the disk image created by image.sh using qemu

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if ! command -v qemu-system-x86_64 2>&1 > /dev/null; then
  error "Failed to locate qemu (qemu-system-x86_64)"
  desc  "Is it installed?"
  exit 1
fi

if [ ! -f "${IMAGE}" ]; then
  error "Failed to access to the disk image (${IMAGE})"
  desc  "Did you build the image?"
  exit 1
fi

qemu-system-x86_64 -display gtk -serial mon:stdio \
  -d int,cpu_reset -s -no-shutdown -no-reboot     \
  -machine q35 -m 256                             \
  -drive media=disk,index=0,format=raw,file="${IMAGE}"
