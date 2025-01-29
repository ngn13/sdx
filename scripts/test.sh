#!/bin/bash

# inspect the serial log file to make sure that everything went correctly

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if [ ! -f "${SERIALLOG}" ]; then
  error "Failed to access to the serial log file (${SERIALLOG})"
  desc  "Did you run the QEMU script?"
  exit 1
fi

matches=(
  'Serial: enumerated \d+ ports'                                      # test 0: serial
  'PCI: enumerated \d+ devices'                                       # test 1: PCI
  'PM: extended paged memory to 0[xX][0-9a-fA-F]+'                    # test 2: PM
  'AHCI: \(0[xX][0-9a-fA-F]+\) HBA supports version'                  # test 3: AHCI
  'Disk: \(0[xX][0-9a-fA-F]+\) loaded \d+ GPT partitions'             # test 4: GPT
  'VFS: \(0[xX][0-9a-fA-F]+:0[xX][0-9a-fA-F]+\) mounted node to root' # test 5: mount
  'User: \(/init:1:user_exec\) new executable is ready'               # test 6: init
)

for i in "${!matches[@]}"; do
  match="${matches[$i]}"

  if ! pcre2grep "${match}" "${SERIALLOG}" &> /dev/null; then
    error "Test #${i} failed (${match})"
    exit 1
  fi

  info "Test #${i} was successful"
done
