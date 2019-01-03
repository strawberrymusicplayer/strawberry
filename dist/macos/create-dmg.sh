#!/bin/sh

if [ -z "$1" ]; then
    echo "Usage: $0 <bundle.app>"
    exit 1
fi

NAME=$(basename "$1" | perl -pe 's/(.*).app/\1/')
IN="$1"
TMP="dmg/$NAME"
OUT="$NAME.dmg"

rm -rf "$TMP"
rm -f "$OUT"

mkdir -p "$TMP"
#mkdir -p "$TMP/.background"
#cp ../dist/macos/dmg_background.png "$TMP/.background/background.png"
#cp ../dist/macos/DS_Store.in "$TMP/.DS_Store"
#chmod go-rwx "$TMP/.DS_Store"
ln -s /Applications "$TMP/Applications"
# Copies the prepared bundle into the dir that will become the DMG
cp -R "$IN" "$TMP"

# Create dmg
hdiutil makehybrid -hfs -hfs-volume-name "$NAME" -hfs-openfolder "$TMP" "$TMP" -o tmp.dmg
hdiutil convert -format UDZO -imagekey zlib-level=9 tmp.dmg -o "$OUT"
