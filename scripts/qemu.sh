#!/bin/bash

# runs the disk image created by image.sh using qemu

source ./scripts/common.sh
if [ $? -ne 0 ]; then
  echo "Please run this script from the repo's root directory"
  exit 1
fi

if ! command -v qemu-system-x86_64 2>&1 > /dev/null; then
  error "Failed to locate qemu (qemu-system-x86_64)"
  desc  "Is it installed?"
  exit 1
fi

if [ ! -f "${IMAGE}" ]; then
  error "Failed to access to the disk image (${IMAGE})"
  desc  "Did you build the image?"
  exit 1
fi

opt_serial_log=0 # logging serial output to a file is disabled, enable with --serial-log
opt_display=1    # video display is enabled, disable video display with --no-display
opt_int=1        # display CPU interrupts, hide them with the option --no-int
opt_gtk=1        # using GTK display, switch to nographic option with --no-gtk
opt_log=0        # logging stdout and stderr is disabled, enable with --log

qemu_args=(
  -s
  -no-shutdown
  -no-reboot
  -machine q35 -m 256
  -drive media=disk,index=0,format=raw,file="${IMAGE}"
)

for arg in "$@"; do
  case "${arg}" in
    "--serial-log")
      opt_serial_log=1
      ;;

    "--no-display")
      opt_display=0
      ;;

    "--no-gtk")
      opt_gtk=0
      ;;

    "--no-int")
      opt_int=0
      ;;

    "--log")
      opt_log=1
      ;;

    "--"*)
      error "Unknown option: ${arg}"
      exit 1
      ;;
  esac
done

if [ $opt_int -eq 1 ]; then
  qemu_args+=(-d int,cpu_reset)
fi

if [ $opt_serial_log -eq 0 ]; then
  qemu_args+=(
    #-serial mon:stdio
    -serial stdio
  )
else
  qemu_args+=(
    -chardev stdio,id=char0,mux=on,logfile="${SERIALLOG}",signal=off
    -serial chardev:char0
    #-mon chardev=char0
  )
fi

if [ $opt_display -eq 0 ]; then
  qemu_args+=(-display none)
elif [ $opt_gtk -eq 1 ]; then
  qemu_args+=(-display gtk)
else
  qemu_args+=(-nographic)
fi

if [ $opt_log -eq 1 ]; then
  qemu-system-x86_64 ${qemu_args[@]} &> qemu.log
else
  qemu-system-x86_64 ${qemu_args[@]}
fi
