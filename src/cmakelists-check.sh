#!/bin/sh

for f in `find .`
do
  file=$(basename $f)
  grep -i $file CMakeLists.txt >/dev/null 2>&1
  #echo $?
  if [ $? -eq 0 ]; then
    continue
  fi
  echo "$file not in CMakeLists.txt"
done
