#!/bin/bash

# build the disk image

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

log "Allocating the image file"
rm -f "${IMAGE}"
truncate --size 1G "${IMAGE}"

log "Partioning the disk"
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
) | gdisk "${IMAGE}"
check_ret "Failed to partion the disk"

# setup the disk image
log "Setting up the disk image, need root access to work with loop devices"
$SUDO ./scripts/setup.sh
