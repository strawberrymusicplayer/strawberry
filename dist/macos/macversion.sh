#!/bin/sh

macos_version=$(sw_vers -productVersion)
macos_version_major=$(echo $macos_version | awk -F '[.]' '{print $1}')
macos_version_minor=$(echo $macos_version | awk -F '[.]' '{print $2}')

if [ "${macos_version_major}" = "10" ]; then
  macos_codenames=(
    ["13"]="highsierra"
    ["14"]="mojave"
    ["15"]="catalina"
  )
  if [[ -n "${macos_codenames[$macos_version_minor]}" ]]; then
    echo "${macos_codenames[$macos_version_minor]}"
  else
    echo "unknown"
  fi
elif [ "${macos_version_major}" = "11" ]; then
  echo "bigsur"
else
  echo "unknown"
fi
