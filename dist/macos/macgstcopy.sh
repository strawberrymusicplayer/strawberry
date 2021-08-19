#!/bin/sh

# Script to copy gstreamer plugins before macdeployqt is run.

if [ "$1" = "" ]; then
  echo "Usage: $0 <bundledir>"
  exit 1
fi
bundledir=$1

if [ "$GIO_EXTRA_MODULES" = "" ]; then
  echo "Error: Set the GIO_EXTRA_MODULES environment variable to the path containing libgiognutls.so."
  exit 1
fi

if [ "$GST_PLUGIN_SCANNER" = "" ]; then
  echo "Error: Set the GST_PLUGIN_SCANNER environment variable to the gst-plugin-scanner."
  exit 1
fi

if [ "$GST_PLUGIN_PATH" = "" ]; then
  echo "Error: Set the GST_PLUGIN_PATH environment variable to the path containing gstreamer plugins."
  exit 1
fi

mkdir -p "${bundledir}/Contents/PlugIns/gio-modules" || exit 1
mkdir -p "${bundledir}/Contents/PlugIns/gstreamer" || exit 1

if ! [ -f "${GIO_EXTRA_MODULES}/libgiognutls.so" ]; then
  echo "Error: Missing ${GIO_EXTRA_MODULES}/libgiognutls.so."
  exit 1
fi
cp -v -f "${GIO_EXTRA_MODULES}/libgiognutls.so" "${bundledir}/Contents/PlugIns/gio-modules/" || exit 1

if ! [ -f "${GST_PLUGIN_SCANNER}" ]; then
  echo "Error: Missing ${GST_PLUGIN_SCANNER}"
  exit 1
fi
cp -v -f "${GST_PLUGIN_SCANNER}" "${bundledir}/Contents/PlugIns/" || exit 1

gst_plugins="
libgstapetag.dylib
libgstapp.dylib
libgstaudioconvert.dylib
libgstaudiofx.dylib
libgstaudiomixer.dylib
libgstaudioparsers.dylib
libgstaudiorate.dylib
libgstaudioresample.dylib
libgstaudiotestsrc.dylib
libgstaudiovisualizers.dylib
libgstauparse.dylib
libgstautoconvert.dylib
libgstautodetect.dylib
libgstcoreelements.dylib
libgstequalizer.dylib
libgstgio.dylib
libgsticydemux.dylib
libgstid3demux.dylib
libgstlevel.dylib
libgstosxaudio.dylib
libgstplayback.dylib
libgstrawparse.dylib
libgstreplaygain.dylib
libgstsoup.dylib
libgstspectrum.dylib
libgsttypefindfunctions.dylib
libgstvolume.dylib
libgstxingmux.dylib
libgsttcp.dylib
libgstudp.dylib
libgstpbtypes.dylib
libgstrtp.dylib
libgstrtsp.dylib
libgstflac.dylib
libgstwavparse.dylib
libgstfaad.dylib
libgstogg.dylib
libgstopus.dylib
libgstasf.dylib
libgstspeex.dylib
libgsttaglib.dylib
libgstvorbis.dylib
libgstisomp4.dylib
libgstlibav.dylib
libgstaiff.dylib
libgstlame.dylib
libgstopusparse.dylib
libgstfaac.dylib
libgstmusepack.dylib
"

gst_plugins=$(echo "$gst_plugins" | tr '\n' ' ' | sed -e 's/^ //g' | sed -e 's/  / /g')

for gst_plugin in $gst_plugins
do
  if [ -f "${GST_PLUGIN_PATH}/${gst_plugin}" ]; then
    cp -v -f "${GST_PLUGIN_PATH}/${gst_plugin}" "${bundledir}/Contents/PlugIns/gstreamer/" || exit 1
  else
    echo "Warning: Missing gstreamer plugin ${GST_PLUGIN_PATH}/${gst_plugin}"
  fi
done

if [ -f "/usr/local/lib/libbrotlicommon.1.dylib" ]; then
  mkdir -p ${bundledir}/Contents/Frameworks
  cp -v -f "/usr/local/lib/libbrotlicommon.1.dylib" "${bundledir}/Contents/Frameworks/"
fi
