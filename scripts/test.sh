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
  'Serial: enumerated \d+ ports'                                      # test 1: serial
  'PCI: enumerated \d+ devices'                                       # test 2: PCI
  'PM: extended paged memory to 0[xX][0-9a-fA-F]+'                    # test 3: PM
  'AHCI: \(0[xX][0-9a-fA-F]+\) HBA supports version'                  # test 4: AHCI
  'Disk: \(0[xX][0-9a-fA-F]+\) loaded \d+ GPT partitions'             # test 5: GPT
  'VFS: \(0[xX][0-9a-fA-F]+:0[xX][0-9a-fA-F]+\) mounted node to root' # test 6: mount
  'User: \(/init:1:user_exec\) new executable is ready'               # test 7: init
)

_test_match() {
  pcre2grep "${matches[$1]}" "${SERIALLOG}" &> /dev/null && return 0
  return 1
}

if [ ! -z "${1}" ]; then
  if [ "${1}" -gt "${#matches[@]}" ] || [ "${1}" -lt 1 ]; then
    error "Please specify a valid test number"
    desc  "${#matches[@]} tests available"
    exit 1
  fi

  index=$(($1-1))

  if _test_match $index; then
    info "Test #${1} was successful"
    exit 0
  fi

  error "Test #${1} failed"
  exit 1
fi

for i in "${!matches[@]}"; do
  if ! _test_match $i; then
    error "Test #$(($i+1)) failed"
    exit 1
  fi

  info "Test #$(($i+1)) was successful"
done
