#!/bin/bash

# set ups the filesystem for the disk image

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

parent="$(ps -o ppid= "$$")"
parent="$(ps -o args= $parent | awk '{print $2}')"

if [[ "${parent}" != "./scripts/setup.sh" ]]; then
  log     "Invalid execution"
  sub_log "This script is supposed be run by the scripts/image.sh"
  exit 1
fi

if [ "${UID}" -ne 0 ]; then
  log "You should run this script as root"
  exit 1
fi

LOOP_DEV="$(losetup -f)"
LOOP_PART="${LOOP_DEV}p2"
LOOP_DIR="/tmp/sdx-image"

if [[ "${LOOP_DEV}" != "/dev/loop"* ]]; then
  log "Invalid loop device from losetup"
  exit 1
fi

cleanup() {
  umount "${LOOP_PART}"
  losetup -d "${LOOP_DEV}"
}

check_ret_clean(){
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      log "${1}"
    fi
    cleanup
    exit 1
  fi
}

# make sure everything is cleaned up before starting
cleanup &> /dev/null

# setup the loop devices
log "Creating the loop devices for the disk and the partitions"
losetup -P "${LOOP_DEV}" "${IMAGE}"
check_ret "Failed to setup the loop for the image (${LOOP_DEV})"

# file system setup
log "Formatting the partitions"

mkfs.fat -F32 "${LOOP_PART}"
check_ret_clean "Failed to format the partition 1 (${LOOP_PART})"

# mount the loop devices
log "Mounting the partitions"
mkdir -p "${LOOP_DIR}"

mount "${LOOP_PART}" "${LOOP_DIR}"
check_ret_clean "Failed to mount loop device (${LOOP_PART})"

if [ -z "${ROOTDIR}" ] || [ -z "${LOOP_DIR}" ]; then
  log "Directory variables are not defined"
  cleanup
  exit 1
fi

log "Installing GRUB for BIOS"
mkdir -p "${LOOP_DIR}/boot"
grub-install --target=i386-pc --recheck          \
             --no-floppy --bootloader-id=GRUB    \
             --boot-directory="${LOOP_DIR}/boot" \
             --modules="normal part_msdos part_gpt multiboot2" "${LOOP_DEV}"

# copy sdx files and grub config
log "Copying the sdx files and directories"
cp -r "${ROOTDIR}/"* "${LOOP_DIR}"

log "Copying the GRUB configuration"
cp "config/grub.cfg" "${LOOP_DIR}/boot/grub"
sed "s/VERSION_HERE/$(./scripts/version.sh)/g" -i "${LOOP_DIR}/boot/grub/grub.cfg"

# sync the image file
sync "${IMAGE}"

# deattach the loop devices
log "Unmounting and deattaching loop devices"
cleanup
check_ret "Failed to deattach loop devices"
