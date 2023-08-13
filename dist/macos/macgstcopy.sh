#!/bin/sh

# Script to copy gio modules and gstreamer plugins before macdeployqt is run.

if [ "$1" = "" ]; then
  echo "Usage: $0 <bundledir>"
  exit 1
fi
bundledir=$1

if [ "${GIO_EXTRA_MODULES}" = "" ]; then
  echo "Error: Set the GIO_EXTRA_MODULES environment variable to the path containing gio modules."
  exit 1
fi

if [ "${GST_PLUGIN_SCANNER}" = "" ]; then
  echo "Error: Set the GST_PLUGIN_SCANNER environment variable to the gst-plugin-scanner."
  exit 1
fi

if [ "${GST_PLUGIN_PATH}" = "" ]; then
  echo "Error: Set the GST_PLUGIN_PATH environment variable to the path containing gstreamer plugins."
  exit 1
fi

mkdir -p "${bundledir}/Contents/PlugIns/gio-modules" || exit 1
mkdir -p "${bundledir}/Contents/PlugIns/gstreamer" || exit 1

if [ -e "${GIO_EXTRA_MODULES}/libgiognutls.so" ]; then
  cp -v -f "${GIO_EXTRA_MODULES}/libgiognutls.so" "${bundledir}/Contents/PlugIns/gio-modules/" || exit 1
else
  echo "Warning: Missing ${GIO_EXTRA_MODULES}/libgiognutls.so."
fi

if [ -e "${GIO_EXTRA_MODULES}/libgioopenssl.so" ]; then
  cp -v -f "${GIO_EXTRA_MODULES}/libgioopenssl.so" "${bundledir}/Contents/PlugIns/gio-modules/" || exit 1
else
  echo "Warning: Missing ${GIO_EXTRA_MODULES}/libgioopenssl.so"
fi

if ! [ -e "${GST_PLUGIN_SCANNER}" ]; then
  echo "Error: Missing ${GST_PLUGIN_SCANNER}"
  exit 1
fi
cp -v -f "${GST_PLUGIN_SCANNER}" "${bundledir}/Contents/PlugIns/" || exit 1

if ! [ -d "${GST_PLUGIN_PATH}" ]; then
  echo "Error: GStreamer plugins path ${GST_PLUGIN_PATH} does not exist."
  exit 1
fi

gst_plugins="
libgstaes.dylib
libgstaiff.dylib
libgstapetag.dylib
libgstapp.dylib
libgstasf.dylib
libgstasfmux.dylib
libgstaudioconvert.dylib
libgstaudiofx.dylib
libgstaudiomixer.dylib
libgstaudioparsers.dylib
libgstaudiorate.dylib
libgstaudioresample.dylib
libgstaudiotestsrc.dylib
libgstautodetect.dylib
libgstbs2b.dylib
libgstcdio.dylib
libgstcoreelements.dylib
libgstdash.dylib
libgstequalizer.dylib
libgstfaac.dylib
libgstfaad.dylib
libgstfdkaac.dylib
libgstflac.dylib
libgstgio.dylib
libgsthls.dylib
libgsticydemux.dylib
libgstid3demux.dylib
libgstid3tag.dylib
libgstisomp4.dylib
libgstlame.dylib
libgstlibav.dylib
libgstmpg123.dylib
libgstmusepack.dylib
libgstogg.dylib
libgstopenmpt.dylib
libgstopus.dylib
libgstopusparse.dylib
libgstosxaudio.dylib
libgstpbtypes.dylib
libgstplayback.dylib
libgstreplaygain.dylib
libgstrtp.dylib
libgstrtsp.dylib
libgstsoup.dylib
libgstspectrum.dylib
libgstspeex.dylib
libgsttaglib.dylib
libgsttcp.dylib
libgsttwolame.dylib
libgsttypefindfunctions.dylib
libgstudp.dylib
libgstvolume.dylib
libgstvorbis.dylib
libgstwavenc.dylib
libgstwavpack.dylib
libgstwavparse.dylib
libgstxingmux.dylib
"

gst_plugins=$(echo "$gst_plugins" | tr '\n' ' ' | sed -e 's/^ //g' | sed -e 's/  / /g')
for gst_plugin in $gst_plugins; do
  if [ -e "${GST_PLUGIN_PATH}/${gst_plugin}" ]; then
    cp -v -f "${GST_PLUGIN_PATH}/${gst_plugin}" "${bundledir}/Contents/PlugIns/gstreamer/" || exit 1
  else
    echo "Warning: Missing gstreamer plugin ${gst_plugin}"
  fi
done
