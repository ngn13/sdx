#!/bin/bash

# common functions and vars for the sdx build/helper scripts

# check for sudo or doas
if [ "${UID}" -eq 0 ]; then
  SUDO=""
elif command -v doas 2>&1 > /dev/null; then
  SUDO="doas"
elif command -v sudo 2>&1 > /dev/null; then
  SUDO="sudo"
else
  error "Run the script as root, install doas or install sudo"
  exit 1
fi

# static paths
SERIALLOG="serial.log"
DESTDIR="dist"
IMAGE="${DESTDIR}/sdx.img"

# colors
FG_YELLOW="\e[0;33m"
FG_BLUE="\e[0;34m"
FG_RED="\e[0;31m"
FG_RESET="\e[0m"
FG_BOLD="\e[1m"

# logging
info(){
  echo -e "${FG_BOLD}${FG_BLUE}INFO ${FG_RESET}${FG_BOLD}${1}${FG_RESET}"
}

warn(){
  echo -e "${FG_BOLD}${FG_YELLOW}WARN ${FG_RESET}${FG_BOLD}${1}${FG_RESET}"
}

error(){
  echo -e "${FG_BOLD}${FG_RED}FAIL ${FG_RESET}${FG_BOLD}${1}${FG_RESET}"
}

desc() {
  echo -e "     ${FG_BOLD}${1}${FG_RESET}"
}

# checks if the previous command failed
check_ret() {
  if [ $? -ne 0 ]; then
    if [ ! -z "${1}" ]; then
      error "${1}"
    fi
    exit 1
  fi
}

# downloads a file from the given URL
download(){
  wget --show-progress --quiet "${1}"
  return ${?}
}

# compares SHA256 hash of a given file with a provided hash
check_sha256() {
  local hash=$(sha256sum "${1}" | cut -d " " -f 1)
  if [[ "${hash}" == "${2}" ]]; then
    info "Good hash for $1"
    return 0
  else
    error "Bad hash for $1"
    return 1
  fi
}
