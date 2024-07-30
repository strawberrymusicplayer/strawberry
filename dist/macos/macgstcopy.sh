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

if ! [ -e "${GIO_EXTRA_MODULES}/libgiognutls.so" ] && ! [ -e "${GIO_EXTRA_MODULES}/libgioopenssl.so" ]; then
  echo "Error: Missing ${GIO_EXTRA_MODULES}/libgiognutls.so or ${GIO_EXTRA_MODULES}/libgioopenssl.so."
  exit 1
fi

if ! [ -e "${GST_PLUGIN_SCANNER}" ]; then
  echo "Error: Missing ${GST_PLUGIN_SCANNER}"
  exit 1
fi

if ! [ -d "${GST_PLUGIN_PATH}" ]; then
  echo "Error: GStreamer plugins path ${GST_PLUGIN_PATH} does not exist."
  exit 1
fi

if ! [ -e "${GST_PLUGIN_PATH}/libgstcoreelements.so" ] && ! [ -e "${GST_PLUGIN_PATH}/libgstcoreelements.dylib" ]; then
  echo "Error: Missing libgstcoreelements.{so,dylib} in GStreamer plugins path ${GST_PLUGIN_PATH}."
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

cp -v -f "${GST_PLUGIN_SCANNER}" "${bundledir}/Contents/PlugIns/" || exit 1
install_name_tool -add_rpath "@loader_path/../Frameworks" "${bundledir}/Contents/PlugIns/$(basename ${GST_PLUGIN_SCANNER})" || exit 1

gst_plugins="
libgstadaptivedemux2
libgstaes
libgstaiff
libgstapetag
libgstapp
libgstasf
libgstasfmux
libgstaudioconvert
libgstaudiofx
libgstaudioparsers
libgstaudioresample
libgstautodetect
libgstbs2b
libgstcdio
libgstcoreelements
libgstdash
libgstdsd
libgstequalizer
libgstfaac
libgstfaad
libgstfdkaac
libgstflac
libgstgio
libgsthls
libgsticydemux
libgstid3demux
libgstid3tag
libgstisomp4
libgstlame
libgstmpegpsdemux
libgstmpegpsmux
libgstmpegtsdemux
libgstmpegtsmux
libgstlibav
libgstmpg123
libgstmusepack
libgstogg
libgstopenmpt
libgstopus
libgstopusparse
libgstosxaudio
libgstpbtypes
libgstplayback
libgstreplaygain
libgstrtp
libgstrtsp
libgstsoup
libgstspectrum
libgstspeex
libgstspotify
libgsttaglib
libgsttcp
libgsttwolame
libgsttypefindfunctions
libgstudp
libgstvolume
libgstvorbis
libgstwavenc
libgstwavpack
libgstwavparse
libgstxingmux
"

gst_plugins=$(echo "$gst_plugins" | tr '\n' ' ' | sed -e 's/^ //g' | sed -e 's/  / /g')
for gst_plugin in $gst_plugins; do
  if [ -e "${GST_PLUGIN_PATH}/${gst_plugin}.dylib" ]; then
    cp -v -f "${GST_PLUGIN_PATH}/${gst_plugin}.dylib" "${bundledir}/Contents/PlugIns/gstreamer/" || exit 1
    install_name_tool -id "@rpath/${gst_plugin}.dylib" "${bundledir}/Contents/PlugIns/gstreamer/${gst_plugin}.dylib"
  elif [ -e "${GST_PLUGIN_PATH}/${gst_plugin}.so" ]; then
    cp -v -f "${GST_PLUGIN_PATH}/${gst_plugin}.so" "${bundledir}/Contents/PlugIns/gstreamer/" || exit 1
    install_name_tool -id "@rpath/${gst_plugin}.so" "${bundledir}/Contents/PlugIns/gstreamer/${gst_plugin}.so"
  else
    echo "Warning: Missing gstreamer plugin ${gst_plugin}."
  fi
done

# libsoup is dynamically loaded by gstreamer, so it needs to be copied.
if [ "${LIBSOUP_LIBRARY_PATH}" = "" ]; then
  echo "Warning: Set the LIBSOUP_LIBRARY_PATH environment variable for copying libsoup."
else
  if [ -e "${LIBSOUP_LIBRARY_PATH}" ]; then
    mkdir -p "${bundledir}/Contents/Frameworks" || exit 1
    cp -v -f "${LIBSOUP_LIBRARY_PATH}" "${bundledir}/Contents/Frameworks/" || exit 1
    install_name_tool -id "@rpath/$(basename ${LIBSOUP_LIBRARY_PATH})" "${bundledir}/Contents/Frameworks/$(basename ${LIBSOUP_LIBRARY_PATH})"
  else
    echo "Warning: Missing libsoup ${LIBSOUP_LIBRARY_PATH}."
  fi
fi
