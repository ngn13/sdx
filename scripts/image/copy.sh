#!/bin/sh

# copies the sdx files to the disk image

source ./scripts/image/loop.sh
if [ $? -ne 0 ]; then
  echo "Failed to import the loop.sh"
  exit 1
fi

if ! check_parent_script "./scripts/image.sh"; then
  error "Invalid execution"
  desc "This script is supposed be run by the scripts/image.sh"
  exit 1
fi

if [ "${UID}" -ne 0 ]; then
  error "You should run this script as root"
  exit 1
fi

# setup and mount loop devices
loop_setup
loop_mount

# copy sdx files and grub config
loop_copy

# deattach the loop devices
info "Unmounting and deattaching loop devices"
loop_cleanup
check_ret "Failed to deattach loop devices"
