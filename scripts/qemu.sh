#!/bin/bash

# runs the disk image created by image.sh using qemu

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if ! command -v qemu-system-x86_64 2>&1 > /dev/null; then
  log "Failed to locate ${QEMU}, is it installed?"
  exit 1
fi

if [ ! -f "${IMAGE}" ]; then
  log "Failed to access to the disk image (${IMAGE}), did you build the image?"
  exit 1
fi

$QEMU -display gtk -serial mon:stdio          \
  -d int,cpu_reset -s -no-shutdown -no-reboot \
  -machine q35 -m 256                         \
  -drive media=disk,index=0,format=raw,file="${IMAGE}"
