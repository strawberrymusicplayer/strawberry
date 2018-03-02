#!/bin/bash

name=strawberry
version="0.1.1"
deb_dist=""
root=$(cd "${0%/*}/.." && echo $PWD/${0##*/})
root=`dirname "$root"`
rootnoslash=`echo $root | sed "s/^\///"`

echo "Creating $name-$version.tar.xz..."

tar -cJf $name-$version.tar.xz \
    --transform "s,^$rootnoslash,$name-$version," \
    --exclude-vcs \
    --exclude "$root/dist/*.tar" \
    --exclude "$root/dist/*.tar.*" \
    --exclude "$root/CMakeLists.txt.user" \
    "$root"

echo "Also creating ${name}_${version}~${deb_dist}.orig.tar.xz..."
cp "$name-$version.tar.xz" "${name}_${version}~${deb_dist}.orig.tar.xz"
