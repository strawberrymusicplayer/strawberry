#!/bin/sh
#
# Strip stale build-machine rpaths (e.g. /opt/strawberry_macos_arm64_release/lib) from Mach-O binaries already deployed into the app bundle.
#
# Third-party libraries built against the strawberry_macos_* prefix keep that absolute path as one of their LC_RPATH entries, and it is never needed at runtime: the relative rpath macdeployqt adds to the main executable and Qt frameworks is enough,
# since dyld searches the rpaths of every loaded image.
# Left in place, this absolute path is still present on the machine doing the build, so macdeployqt's own @rpath dependency resolver can resolve a library back to that external copy instead of the one already inside the bundle,
# and then skips signing it because it thinks it is an external system dependency.
#
# Run this after deploying with "macdeployqt ... -no-codesign" and before the final "macdeployqt ... -codesign=<identity>" pass.

if [ "$1" = "" ]; then
  echo "Usage: $0 <bundledir>"
  exit 1
fi
bundledir=$1

find "${bundledir}/Contents/Frameworks" "${bundledir}/Contents/PlugIns" -type f 2>/dev/null | while read -r f; do
  file "$f" | grep -q "Mach-O" || continue
  otool -l "$f" 2>/dev/null | grep -A2 "cmd LC_RPATH" | sed -n 's/^ *path \(.*\) (offset.*/\1/p' | while IFS= read -r rpath; do
    case "$rpath" in
      /opt/strawberry_macos_*)
        echo "Deleting stale rpath ${rpath} from ${f}"
        install_name_tool -delete_rpath "$rpath" "$f" || exit 1
        ;;
    esac
  done
done
