#!/bin/bash

# attaches GNU debugger (gdb) to the running qemu instance

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
  -q                                     # quiet
  -ex "'target remote localhost:1234'"   # connect to QEMU debug server
  #-ex "'set disassembly-flavor intel'"  # for virgin intel syntax fanboys
  -ex "'symbol-file kernel/dist/kernel'" # load the symbols from the kernel binary
  -ex "'source config/gdb/blower.py'"    # blower command
  -ex "'source config/gdb/hooks'"        # user defined comamnd hooks
)

exec bash -c "gdb ${gdb_args[*]}"
