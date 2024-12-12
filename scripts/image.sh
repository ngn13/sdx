#!/bin/bash

# build the disk image

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if [ ! -d "${DESTDIR}" ]; then
  error "Failed to access to the dist directory"
  desc  "Did you run make?"
  exit 1
fi

image_check_ret(){
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      error "${1}"
    fi
    rm -f "${IMAGE}"
    exit 1
  fi
}

if [ -f "${IMAGE}" ]; then
  warn "Image is already available, not rebuilding it"

  info "Running the copy script"
  desc "Need root access to work with loop devices"

  $SUDO ./scripts/image/copy.sh
  image_check_ret "Copy script failed"

  info "Disk image is ready"
  exit 0
fi

info "Truncating the image file"
truncate --size 1G "${IMAGE}"

info "Partioning the disk"
(
echo o # create (P)MBR disklabel
echo y # confirm

echo n      # new partition for BIOS
echo        # default partition number (1)
echo        # default first sector
echo '+1M'  # last sector at the first MB
echo 'ef02' # change partition type to BIOS

#echo n      # new partition for EFI
#echo        # default partition number (2)
#echo        # default first sector
#echo '+32M' # min EFI format size (FAT32)
#echo 'ef00' # change type to EFI

echo n      # new partition for the OS
echo        # default partition number
echo        # default first sector
echo        # use the rest of the disk
echo '8300' # change type to linux file system

echo w # write the changes
echo y # confirm
) | gdisk "${IMAGE}" &> /dev/null
image_check_ret "Failed to partion the disk"

# setup the disk image
info "Running the disk image setup script"
desc "Need root access to work with loop devices"

$SUDO ./scripts/image/setup.sh
image_check_ret "Setup script failed"

info "Disk image is ready"
