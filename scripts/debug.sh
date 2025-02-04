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

gdb_args=(
  -q                                    # quiet
  -ex "'target remote localhost:1234'"  # connect to QEMU debug server
  #-ex "'set disassembly-flavor intel'" # for virgin intel syntax fanboys
)

case "${1}" in
  "boot")
    gdb_args+=(-ex "'symbol-file kernel/dist/kernel -o 0x0'")
    ;;

  "kernel")
    gdb_args+=(-ex "'symbol-file kernel/dist/kernel -o 0xfffffff800000000'")
    ;;

  *)
    error "Please specify a valid debug target"
    desc  "Available targets are \"boot\" and \"kernel\""
    exit 1
    ;;
esac

exec bash -c "gdb ${gdb_args[*]}"
