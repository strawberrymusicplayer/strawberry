#!/bin/sh

macos_version=$(sw_vers -productVersion| awk -F '[.]' '{print $2}')
macos_codenames=(
["13"]="highsierra"
["14"]="mojave"
["15"]="catalina"
)

if [[ -n "${macos_codenames[$macos_version]}" ]]; then 
  echo "${macos_codenames[$macos_version]}"
else
  echo "unknown"
fi
