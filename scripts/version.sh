#!/bin/bash

# obtains the sdx OS version from the version header

if [ ! -f config/version.h ]; then
  exit 1
fi

version_sdx=$(grep 'VERSION_SDX' config/version.h)
version_sdx=$(echo "${version_sdx}" | awk '{print $3}')
version_sdx=$(echo "${version_sdx}" | sed 's/"//g')

echo "${version_sdx}"
