#!/bin/sh
#
# Strawberry Music Player
# Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
#
# Strawberry is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Strawberry is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.

sizes="full 128 64 48 32 22"

for s in $sizes
do
  if [ "$s" = "full" ]; then
    dir=$s
  else
    dir=${s}x${s}
  fi
  if ! [ -d "$dir" ]; then
    echo "Missing $dir directory."
    continue
  fi
  for f in ${dir}/*
  do
    file=`basename $f`

    for y in $sizes
    do

      if [ "$s" = "$y" ]; then
        continue
      fi

     if [ "$y" = "full" ]; then
        dir2=$y
      else
        dir2=${y}x${y}
      fi

      if [ "$dir2" = "full" ]; then
        continue
      fi

      if ! [ "$s" = "full" ] && [ $y -gt $s ]; then
        continue
      fi

      if ! [ -f "${dir2}/$file" ]; then
        echo "Warning: ${dir2}/$file does not exist, but ${dir}/${file} exists."
      fi
    done

    if [ "$dir" = "full" ]; then
      continue
    fi

    id=`identify "$f"` || exit 1
    if [ "$id" = "" ] ; then
      echo "ERROR: Cannot determine format and geometry for image: \"$f\"."
      continue
    fi
    g=`echo $id | awk '{print $3}'` || exit 1
    if [ "$g" = "" ] ; then
      echo "ERROR: Cannot determine geometry for image: \"$f\"."
      continue
    fi

    # Geometry can be 563x144+0+0 or 75x98
    # we need to get rid of the plus (+) and the x characters:
    w=`echo $g | sed 's/[^0-9]/ /g' | awk '{print $1}'` || exit 1
    if [ "$w" = "" ] ; then
      echo "ERROR: Cannot determine width for image: \"$f\"."
      continue
    fi
    h=`echo $g | sed 's/[^0-9]/ /g' | awk '{print $2}'` || exit 1
    if [ "$h" = "" ] ; then
      echo "ERROR: Cannot determine height for image: \"$f\"."
      continue
    fi

    if ! [ "${h}x${w}" = "$dir" ]; then
      echo "Warning: $f is not $dir, but ${h}x${w}!"
    fi

  done
done

#file="../icons.qrc"
#rm -rf "$file"
#echo "<RCC>" >>$file
#echo "<qresource prefix=\"/\">" >>$file

#for i in full $sizes
#do
#  for x in $i/*
#  do
#    f=`basename $x`
#    echo "	<file>icons/$i/$f</file>" >>$file
#  done
#done

#echo "</qresource>" >>$file
#echo "</RCC>" >>$file

