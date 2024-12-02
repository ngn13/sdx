#!/bin/bash

# set ups the filesystem for the disk image

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

setup_check_ret(){
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      error "${1}"
    fi
    loop_cleanup
    rm "${IMAGE}"
    exit 1
  fi
}

# loop device setup
loop_setup

# file system setup
info "Formatting the root partition"
mkfs.fat -F32 "${LOOP_PART}"
setup_check_ret "Failed to format the root partition (${LOOP_PART})"

# filesystem is now ready, we can mount the partitions
loop_mount

if [ -z "${ROOTDIR}" ] || [ -z "${LOOP_DIR}" ]; then
  error "Directory variables are not defined"
  loop_cleanup
  exit 1
fi

info "Installing GRUB for BIOS"
mkdir -p "${LOOP_DIR}/boot"
grub-install --target=i386-pc --recheck          \
             --no-floppy --bootloader-id=GRUB    \
             --boot-directory="${LOOP_DIR}/boot" \
             --modules="normal part_msdos part_gpt multiboot2" "${LOOP_DEV}"
setup_check_ret "GRUB installation failed"

# copy sdx files and grub config
info "Copying the sdx files and directories"
cp -r "${ROOTDIR}/"* "${LOOP_DIR}"
setup_check_ret "Failed to copy sdx files and directories"

info "Copying the GRUB configuration"
cp "config/grub.cfg" "${LOOP_DIR}/boot/grub"
sed "s/VERSION_HERE/$(./scripts/version.sh)/g" -i "${LOOP_DIR}/boot/grub/grub.cfg"

# deattach the loop devices
info "Unmounting and deattaching loop devices"
loop_cleanup
setup_check_ret "Failed to deattach loop devices"
