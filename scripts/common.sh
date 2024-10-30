#!/bin/bash

# common functions and vars for the sdx build/helper scripts
QEMU="qemu-system-x86_64"
SUDO="doas"

DISTDIR="dist"
ROOTDIR="${DISTDIR}/root"
IMAGE="${DISTDIR}/sdx.img"

# colors
FG_BOLD="\e[1m"
FG_RESET="\e[0m"

log(){
  echo -e "${FG_BOLD}*** ${1}${FG_RESET}"
}

sub_log() {
  echo -e "${FG_BOLD}    ${1}${FG_RESET}"
}

if [ "${UID}" -eq 0 ]; then
  SUDO=""
elif command -v doas 2>&1 > /dev/null; then
  SUDO="doas"
elif command -v sudo 2>&1 > /dev/null; then
  SUDO="sudo"
else
  log "Run the script as root, install doas or install sudo"
  exit 1
fi

check_ret() {
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      log "${1}"
    fi
    exit 1
  fi
}

download(){
  wget --show-progress --quiet "${1}"
  return ${?}
}

check_sha256() {
  local hash=$(sha256sum "${1}" | cut -d " " -f 1)
  if [[ "${hash}" == "${2}" ]]; then
    log "Good hash for $1"
    return 0
  else
    log "Bad hash for $1"
    return 1
  fi
}

if [ "$EUID" -eq 0 ]; then
  SUDO=""
elif type "doas" > /dev/null; then
  SUDO="doas"
elif type "sudo" > /dev/null; then
  SUDO="sudo"
else
  log "Install sudo, doas or run the script as root"
  exit 1
fi
