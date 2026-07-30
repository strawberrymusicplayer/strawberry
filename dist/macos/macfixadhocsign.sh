#!/bin/sh
#
# Sign any Mach-O binary left with only an ad-hoc/linker signature after macdeployqt has run.
#
# macdeployqt's codesign step only walks the Mach-O dependency graph starting from Contents/MacOS and Contents/PlugIns.
# Libraries that are never declared as a linked dependency of anything on that walk - such as libsoup, which GStreamer dlopens by name rather than links against, and in turn its own dependencies libnghttp2/libpsl - are never queued for signing and are left with the ad-hoc signature applied at build time.
# Apple notarization requires every executable in the bundle to carry the same Developer ID signature, so find and fix any stragglers here.

if [ "$1" = "" ] || [ "$2" = "" ]; then
  echo "Usage: $0 <bundledir> <identity>"
  exit 1
fi
bundledir=$1
identity=$2

find "${bundledir}/Contents/Frameworks" "${bundledir}/Contents/PlugIns" -type f 2>/dev/null | while read -r f; do
  file "$f" | grep -q "Mach-O" || continue
  codesign -dvvv "$f" 2>&1 | grep -q "TeamIdentifier=${identity}" && continue
  echo "Signing ${f} (was only ad-hoc signed)"
  codesign --force --preserve-metadata=identifier,entitlements -s "${identity}" "$f" || exit 1
done
