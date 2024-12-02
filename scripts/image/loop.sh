#!/bin/bash

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Failed to import the common.sh"
  exit 1
fi

export LOOP_DEV="$(losetup -f)"
export LOOP_PART="${LOOP_DEV}p2"
export LOOP_DIR="/tmp/sdx-image"

if [[ "${LOOP_DEV}" != "/dev/loop"* ]]; then
  error "Invalid loop device from losetup"
  exit 1
fi

# unmounts and removes loop devices
loop_cleanup() {
  sync "${IMAGE}"
  # unmounts the loop device's root partition
  umount "${LOOP_PART}"
  # removes the loop device
  losetup -d "${LOOP_DEV}"
}

# check if the previous command failed
loop_check_ret(){
  # modified version of common.sh's cleanup
  # used to make sure that everything is cleaned up before exit
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      error "${1}"
    fi
    loop_cleanup
    exit 1
  fi
}

# set ups the loop device for the disk image
loop_setup(){
  # make sure everything is cleaned up before starting
  loop_cleanup &> /dev/null

  # setup the loop devices
  info "Creating the loop devices for the disk and the partitions"
  losetup -P "${LOOP_DEV}" "${IMAGE}"
  loop_check_ret "Failed to setup the loop devices for the image (${LOOP_DEV})"
}

# mounts the loop device's partitions
loop_mount(){
  # mount the loop devices
  info "Mounting the root partition"
  mkdir -p "${LOOP_DIR}"

  mount "${LOOP_PART}" "${LOOP_DIR}"
  loop_check_ret "Failed to mount the root partition (${LOOP_PART})"
}
