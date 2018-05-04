# Strawberry Music Player
# Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
# This file was part of Clementine.
# Copyright 2010, David Sansome <me@davidsansome.com>
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

project(strawberry)
cmake_minimum_required(VERSION 2.8.11)
#cmake_policy(SET CMP0011 OLD)

#aux_source_directory(. SRC_LIST)

include(CheckCXXCompilerFlag)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(cmake/C++11Compat.cmake)
include(cmake/Summary.cmake)
include(cmake/Version.cmake)
include(cmake/Rpm.cmake)
include(cmake/OptionalSource.cmake)

#set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if (CMAKE_CXX_COMPILER MATCHES ".*clang")
  set(CMAKE_COMPILER_IS_CLANGXX 1)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized")
endif ()

if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --stdlib=libc++")
endif ()

find_program(CCACHE_EXECUTABLE NAMES ccache)
if (CCACHE_EXECUTABLE)
  message(STATUS "ccache found: will be used for compilation and linkage")
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
endif ()

if (UNIX AND NOT APPLE)
  set(LINUX 1)
endif (UNIX AND NOT APPLE)

set(QT_MIN_VERSION 5.6.0)

find_package(X11)
find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core OpenGL Sql Network Xml Widgets Concurrent Test)

if(X11_FOUND)
  find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS X11Extras)
endif()

if(NOT APPLE)
  find_package(Qt5 COMPONENTS WebKitWidgets)
endif(NOT APPLE)

find_program(QT_LCONVERT_EXECUTABLE NAMES lconvert lconvert-qt5 PATHS ${QT_BINARY_DIR} NO_DEFAULT_PATH)
find_program(QT_LCONVERT_EXECUTABLE NAMES lconvert lconvert-qt5)

if(APPLE)
  if(NOT QT_MAC_USE_COCOA)
    message(FATAL_ERROR "Cocoa support is required")
  endif(NOT QT_MAC_USE_COCOA)
endif(APPLE)

find_package(OpenGL REQUIRED)
find_package(Boost REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Threads)
if(LINUX)
  find_package(ALSA REQUIRED)
endif(LINUX)

pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(CDIO libcdio)
pkg_check_modules(CHROMAPRINT REQUIRED libchromaprint)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_APP gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_TAG gstreamer-tag-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)
pkg_check_modules(LIBXINE libxine)
pkg_check_modules(LIBVLC libvlc)
pkg_check_modules(PHONON phonon4qt5)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(LIBPULSE libpulse)
pkg_check_modules(LIBXML libxml-2.0)
pkg_check_modules(TAGLIB REQUIRED taglib>=1.8)
pkg_check_modules(SQLITE REQUIRED sqlite3>=3.7)
pkg_check_modules(LIBGLU REQUIRED glu)
pkg_check_modules(IMOBILEDEVICE libimobiledevice-1.0)
pkg_check_modules(USBMUXD libusbmuxd)
pkg_check_modules(PLIST libplist)

find_library(PROTOBUF_STATIC_LIBRARY libprotobuf.a libprotobuf)

if (WIN32)
  find_package(ZLIB REQUIRED)
endif (WIN32)

# LASTFM
find_library(LASTFM5_LIBRARIES lastfm5)
find_path(LASTFM5_INCLUDE_DIRS lastfm5/ws.h)
find_path(LASTFM51_INCLUDE_DIRS lastfm5/Track.h)
#CHECK_INCLUDE_FILES(lastfm/Track.h HAVE_LASTFM_TRACK_H)

# GSTREAMER
CHECK_INCLUDE_FILES(gst/audio/gstaudiocdsrc.h GST_AUDIO_GSTAUDIOCDSRC_H)

# TAGLIB
set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDE_DIRS}")
set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")
check_cxx_source_compiles("#include <opusfile.h>
      int main() { char *s; TagLib::Ogg::Opus::File opusfile(s); return 0;}" TAGLIB_HAS_OPUS)
set(CMAKE_REQUIRED_INCLUDES)
set(CMAKE_REQUIRED_LIBRARIES)
CHECK_INCLUDE_FILES(taglib/xiphcomment.h HAVE_XIPCOMMENT_H)

# LASTFM
if(LASTFM5_INCLUDE_DIRS AND LASTFM51_INCLUDE_DIRS)
  set(HAVE_LIBLASTFM1 ON)
endif()

# CHROMAPRINT
CHECK_INCLUDE_FILES(chromaprint.h CHROMAPRINT_H)

if (APPLE)
  find_library(SPARKLE Sparkle)
  add_subdirectory(3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_LIBRARIES SPMediaKeyTap)
endif (APPLE)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
  add_definitions(-DNDEBUG)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
  #add_definitions(-DQT_NO_WARNING_OUTPUT)
endif(${CMAKE_BUILD_TYPE} MATCHES "Release")

# Set up definitions and paths
add_definitions(${QT_DEFINITIONS})
link_directories(${TAGLIB_LIBRARY_DIRS})
link_directories(${GSTREAMER_LIBRARY_DIRS})

# Don't try to use webkit if their include directories couldn't be found.
if (NOT QT_QTWEBKIT_INCLUDE_DIR)
  set (QT_USE_QTWEBKIT 0)
endif (NOT QT_QTWEBKIT_INCLUDE_DIR)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GLIBCONFIG_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${LIBXML_INCLUDE_DIRS})
include_directories(${TAGLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_APP_INCLUDE_DIRS})
include_directories(${GSTREAMER_AUDIO_INCLUDE_DIRS})
include_directories(${GSTREAMER_BASE_INCLUDE_DIRS})
include_directories(${GSTREAMER_TAG_INCLUDE_DIRS})
include_directories(${GSTREAMER_PBUTILS_INCLUDE_DIRS})

if (WIN32)
  # RC compiler
  string(REPLACE "gcc" "windres" CMAKE_RC_COMPILER_INIT ${CMAKE_C_COMPILER})
  enable_language(RC)
  SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -o <OBJECT> <SOURCE> -I ${CMAKE_SOURCE_DIR}/dist/windows")
endif(WIN32)

add_definitions(-DQT_NO_CAST_TO_ASCII -DQT_STRICT_ITERATORS)

# Optional bits
if(WIN32)
  option(ENABLE_WIN32_CONSOLE "Show the windows console even outside Debug mode" OFF)
endif(WIN32)

optional_component(GSTREAMER ON "Engine: GStreamer backend"
  DEPENDS "gstreamer-1.0" GSTREAMER_FOUND
  DEPENDS "gstreamer-base-1.0" GSTREAMER_BASE_FOUND
  DEPENDS "gstreamer-app-1.0" GSTREAMER_APP_FOUND
  DEPENDS "gstreamer-audio-1.0" GSTREAMER_AUDIO_FOUND
  DEPENDS "gstreamer-tag-1.0" GSTREAMER_TAG_FOUND
  DEPENDS "gstreamer-pbutils-1.0" GSTREAMER_PBUTILS_FOUND
)

optional_component(XINE ON "Engine: Xine backend"
  DEPENDS "libxine" LIBXINE_FOUND
)

optional_component(VLC ON "Engine: VLC backend"
  DEPENDS "libvlc" LIBVLC_FOUND
)

optional_component(PHONON OFF "Engine: Phonon backend"
  DEPENDS "phonon4qt5" PHONON4QT5_FOUND
)

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" CDIO_FOUND
)

optional_component(LIBGPOD ON "Devices: iPod classic support"
  DEPENDS "libgpod" LIBGPOD_FOUND
)

optional_component(GIO ON "Devices: GIO device backend"
  DEPENDS "libgio" GIO_FOUND
  DEPENDS "Linux or Windows" "NOT APPLE"
)

optional_component(IMOBILEDEVICE ON "Devices: iPod Touch, iPhone, iPad support"
  DEPENDS "libimobiledevice" IMOBILEDEVICE_FOUND
  DEPENDS "libplist" PLIST_FOUND
  DEPENDS "libusbmuxd" USBMUXD_FOUND
  DEPENDS "iPod classic support" HAVE_LIBGPOD
)

optional_component(LIBMTP ON "Devices: MTP support"
  DEPENDS "libmtp" LIBMTP_FOUND
)

optional_component(LIBLASTFM ON "Last.fm support"
  DEPENDS "liblastfm" LASTFM5_LIBRARIES LASTFM5_INCLUDE_DIRS
)

optional_component(DBUS ON "D-Bus support"
  DEPENDS "Linux" LINUX
)

optional_component(DEVICEKIT ON "Devices: DeviceKit backend"
  DEPENDS "D-Bus support" HAVE_DBUS
)

optional_component(UDISKS2 ON "Devices: UDisks2 backend"
  DEPENDS "D-Bus support" HAVE_DBUS
)

optional_component(SPARKLE ON "Sparkle integration"
  DEPENDS "Mac OS X" APPLE
  DEPENDS "Sparkle" SPARKLE
)

optional_component(LIBPULSE ON "Pulse audio integration"
  DEPENDS "libpulse" LIBPULSE_FOUND
)


# Find DBus if it's enabled
if (HAVE_DBUS)
  find_package(Qt5 COMPONENTS DBus)
  get_target_property(QT_DBUSXML2CPP_EXECUTABLE Qt5::qdbusxml2cpp LOCATION)
endif ()

# We can include the Qt definitions now
#include(${QT_USE_FILE})
if(WIN32)
  set(QT_LIBRARIES Qt5::Core Qt5::OpenGL Qt5::Sql Qt5::Network Qt5::Xml Qt5::Widgets Qt5::Concurrent)
else(WIN32)
  set(QT_LIBRARIES Qt5::Core Qt5::OpenGL Qt5::Sql Qt5::Network Qt5::Xml Qt5::Widgets Qt5::Concurrent Qt5::X11Extras Qt5::DBus)
endif(WIN32)

# Remove GLU and GL from the link line - they're not really required and don't exist on my mingw toolchain
list(REMOVE_ITEM QT_LIBRARIES "-lGLU -lGL")

# When/if upstream accepts our patches then these options can be used to link to system installed qtsingleapplication instead.
option(USE_SYSTEM_QTSINGLEAPPLICATION "Don't set this option unless your system QtSingleApplication library has been compiled with the Strawberry patches in 3rdparty" OFF)
if(USE_SYSTEM_QTSINGLEAPPLICATION)
  find_path(QTSINGLEAPPLICATION_INCLUDE_DIRS qtsingleapplication.h PATH_SUFFIXES QtSolutions)
  find_library(QTSINGLEAPPLICATION_LIBRARIES QtSolutions_SingleApplication-2.6)
  find_library(QTSINGLECOREAPPLICATION_LIBRARIES QtSolutions_SingleCoreApplication-2.6)
else(USE_SYSTEM_QTSINGLEAPPLICATION)
  add_subdirectory(3rdparty/qtsingleapplication)
  set(QTSINGLEAPPLICATION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtsingleapplication)
  set(QTSINGLEAPPLICATION_LIBRARIES qtsingleapplication)
endif(USE_SYSTEM_QTSINGLEAPPLICATION)

# When/if upstream accepts our or reimplement our patches then these options can be used to link to system installed qxt instead.
option(USE_SYSTEM_QXT "Don't set this option unless your system Qxt library has been compiled with the Strawberry patches in 3rdparty" OFF)
if (USE_SYSTEM_QXT)
  find_path(QXTCORE_INCLUDE_DIRS qxtglobal.h PATH_SUFFIXES QxtCore)
  find_path(QXTGUI_INCLUDE_DIRS qxtglobalshortcut.h PATH_SUFFIXES QxtGui)
  set(QXT_INCLUDE_DIRS ${QXTCORE_INCLUDE_DIRS} ${QXTGUI_INCLUDE_DIRS})
  # We only need its header. We don't need to link to QxtCore.
  find_library(QXT_LIBRARIES QxtGui)
else (USE_SYSTEM_QXT)
  add_definitions(-DQXT_STATIC -DBUILD_QXT_GUI -DBUILD_QXT_CORE)
  set(QXT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qxt)
  set(QXT_LIBRARIES qxt)
  if (NOT APPLE)
    add_subdirectory(3rdparty/qxt)
  endif (NOT APPLE)
endif (USE_SYSTEM_QXT)

# Use system sha2 if it's available
find_path(SHA2_INCLUDE_DIRS sha2.h)
find_library(SHA2_LIBRARIES sha2)
if (SHA2_LIBRARIES AND SHA2_INCLUDE_DIRS)
  message(STATUS "Using system sha2 library")
  set(USE_SYSTEM_SHA2 ON)
else()
  message(STATUS "Using builtin sha2 library")
  set(USE_SYSTEM_SHA2 OFF)
  add_subdirectory(3rdparty/sha2)
  set(SHA2_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/sha2)
  set(SHA2_LIBRARIES sha2)
endif()

# Qocoa
add_subdirectory(3rdparty/qocoa)

#if(IMOBILEDEVICE_FOUND AND PLIST_FOUND)
  #add_subdirectory(ext/gstafc)
#endif(IMOBILEDEVICE_FOUND AND PLIST_FOUND)

# Subdirectories
add_subdirectory(src)
if (WIN32)
  add_subdirectory(3rdparty/qtwin)
endif (WIN32)
add_subdirectory(dist)
add_subdirectory(ext/libstrawberry-common)
add_subdirectory(ext/libstrawberry-tagreader)
add_subdirectory(ext/strawberry-tagreader)

# Uninstall support
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# Show a summary of what we have enabled
summary_show()
