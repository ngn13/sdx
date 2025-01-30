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

loop_copy(){
  [ ! -z "${DESTDIR}" ]
  loop_check_ret "Install destination directory not specified"

  info "Copying root filesystem directories and files"
  find "${DESTDIR}/"* -maxdepth 0 -mindepth 0 -not -wholename "${IMAGE}" -exec cp -rv {} "${LOOP_DIR}" \;
  loop_check_ret "Failed to copy root filesystem directories and files"

  info "Copying the GRUB configuration"
  cp "config/grub.cfg" "${LOOP_DIR}/boot/grub"
  loop_check_ret "Failed to copy the GRUB configuration"

  info "Replacing version placeholder in the GRUB configuration"
  sed "s/VERSION_HERE/$(./scripts/version.sh)/g" -i "${LOOP_DIR}/boot/grub/grub.cfg"
  loop_check_ret "Failed to replace the version placeholder in the GRUB configuration"
}
