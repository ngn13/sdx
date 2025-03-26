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
  'Serial: enumerated \d+ ports'                                         # test 1 : serial
  'PMM: bitmapping 0[xX][0-9a-fA-F]+ - 0[xX][0-9a-fA-F]+ with \d+ bytes' # test 2 : PMM
  'Sched: created the main task: 0[xX][0-9a-fA-F]+'                      # test 3 : scheduler
  'ACPI: loaded version \d+.\d+'                                         # test 4 : ACPI
  'PCI: enumerated \d+ devices'                                          # test 5 : PCI
  'AHCI: HBA at 0[xX][0-9a-fA-F]+ supports version'                      # test 6 : AHCI
  'Disk: loaded \d+ GPT partitions'                                      # test 7 : GPT
  'PS/2: successfully tested \d+ ports'                                  # test 8 : PS/2
  'VFS: mounted node 0[xX][0-9a-fA-F]+ to /'                             # test 9 : mount
  'Sys: \(1:sys_exec\) executing the new binary'                         # test 10: init
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
