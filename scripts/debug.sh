#!/bin/bash

# attachs debugger to the running qemu instance

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if [ -z "kernel/dist/kernel" ]; then
  error "Failed to access kernel binary"
  desc  "Did you build the kernel?"
  exit 1
fi

gdb -q -ex "target remote localhost:1234"   \
       -ex "symbol-file kernel/dist/kernel" \
       -ex "set disassembly-flavor intel"
