# Strawberry Music Player
# Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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
if(${CMAKE_VERSION} VERSION_GREATER "3.0")
  cmake_policy(SET CMP0054 NEW)
endif()

include(CheckCXXCompilerFlag)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(cmake/C++11Compat.cmake)
include(cmake/Version.cmake)
include(cmake/Summary.cmake)
include(cmake/OptionalSource.cmake)
include(cmake/ParseArguments.cmake)
include(cmake/Rpm.cmake)
include(cmake/Deb.cmake)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX ON)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  set(FREEBSD ON)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
  set(OPENBSD ON)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
  add_definitions(-DNDEBUG)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
  #add_definitions(-DQT_NO_WARNING_OUTPUT)
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  set(DEBUG ON)
endif()

if (CMAKE_CXX_COMPILER MATCHES ".*clang")
  set(CMAKE_COMPILER_IS_CLANGXX 1)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-uninitialized")
endif()

if (APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --stdlib=libc++")
endif(APPLE)

find_program(CCACHE_EXECUTABLE NAMES ccache)
if (CCACHE_EXECUTABLE)
  message(STATUS "ccache found: will be used for compilation and linkage")
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
endif ()
find_program(QT_LCONVERT_EXECUTABLE NAMES lconvert lconvert-qt5 PATHS ${QT_BINARY_DIR} NO_DEFAULT_PATH)
find_program(QT_LCONVERT_EXECUTABLE NAMES lconvert lconvert-qt5)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(CDIO libcdio)
find_package(Threads)
find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)
find_library(PROTOBUF_STATIC_LIBRARY libprotobuf.a libprotobuf)
if(LINUX)
  find_package(ALSA REQUIRED)
  pkg_check_modules(DBUS REQUIRED dbus-1)
else(LINUX)
  find_package(ALSA)
  pkg_check_modules(DBUS dbus-1)
endif(LINUX)
if (NOT APPLE)
  find_package(X11)
endif()
if(X11_FOUND)
  set(HAVE_X11 ON)
endif()
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_APP gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_TAG gstreamer-tag-1.0)
pkg_check_modules(LIBXINE libxine)
pkg_check_modules(LIBVLC libvlc)
pkg_check_modules(PHONON phonon4qt5)
pkg_check_modules(SQLITE REQUIRED sqlite3>=3.7)
pkg_check_modules(LIBPULSE libpulse)
pkg_check_modules(CHROMAPRINT libchromaprint)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(LIBIMOBILEDEVICE libimobiledevice-1.0)
pkg_check_modules(LIBUSBMUXD libusbmuxd)
pkg_check_modules(LIBPLIST libplist)
pkg_check_modules(LIBDEEZER libdeezer)
pkg_check_modules(LIBDZMEDIA libdzmedia)

if(WIN32)
  find_package(ZLIB REQUIRED)
endif(WIN32)

# QT
set(QT_MIN_VERSION 5.5.1)
find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Concurrent Widgets Network Sql Xml)
if(X11_FOUND)
  find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS X11Extras)
endif()
if(DBUS_FOUND)
  find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS DBus)
  get_target_property(QT_DBUSXML2CPP_EXECUTABLE Qt5::qdbusxml2cpp LOCATION)
endif()
if(APPLE)
  find_package(Qt5 REQUIRED COMPONENTS MacExtras)
endif()
if(WIN32)
  find_package(Qt5 REQUIRED COMPONENTS WinExtras)
endif()

set(QT_LIBRARIES Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network Qt5::Sql Qt5::Xml)

if(DBUS_FOUND)
  set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::DBus)
endif()
if(X11_FOUND)
  set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::X11Extras)
endif()
if(APPLE)
  set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::MacExtras)
endif()
if(WIN32)
  set(QT_LIBRARIES ${QT_LIBRARIES} Qt5::WinExtras)
endif()

# TAGLIB
pkg_check_modules(TAGLIB taglib)
# Only use system taglib if it's greater than 1.11.1
# There is a bug in version 1.11.1 corrupting Ogg files, see: https://github.com/taglib/taglib/issues/864
# If you decide to use the systems taglib, make sure it has been patched with the following commit:
# https://github.com/taglib/taglib/commit/9336c82da3a04552168f208cd7a5fa4646701ea4
# The current taglib in 3rdparty also has the following features used by strawberry:
# - Audio file detection by content.
# - DSF and DSDIFF support
#
if (TAGLIB_VERSION VERSION_GREATER 1.11.1 OR WIN32)
  option(USE_SYSTEM_TAGLIB "Use system taglib" ON)
else()
  option(USE_SYSTEM_TAGLIB "Use system taglib" OFF)
endif()
if (TAGLIB_FOUND AND USE_SYSTEM_TAGLIB)
  if (TAGLIB_VERSION VERSION_GREATER 1.11.1 OR WIN32)
    message(STATUS "Using system taglib library")
  else()
    message(WARNING "Using system taglib library. Version 1.11.1 or less has a bug corrupting Ogg files, make sure your systems version has been patched!")
  endif()
  set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDE_DIRS}")
  set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")
  set(CMAKE_REQUIRED_INCLUDES)
  set(CMAKE_REQUIRED_LIBRARIES)
  find_path(HAVE_TAGLIB_DSFFILE_H taglib/dsffile.h)
  if(HAVE_TAGLIB_DSFFILE_H)
    set(HAVE_TAGLIB_DSFFILE ON)
  endif(HAVE_TAGLIB_DSFFILE_H)
else()
  message(STATUS "Using builtin taglib library")
  set(TAGLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/taglib/;${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/")
  set(TAGLIB_LIBRARY_DIRS "")
  set(TAGLIB_LIBRARIES tag)
  add_subdirectory(3rdparty/utf8-cpp)
  add_subdirectory(3rdparty/taglib)
  set(HAVE_TAGLIB_DSFFILE ON)
endif()

# Use system QtSingleApplication only if explicitly enabled.
option(USE_SYSTEM_QTSINGLEAPPLICATION "Use system QtSingleApplication library" OFF)
if(USE_SYSTEM_QTSINGLEAPPLICATION)
  message(STATUS "Using system QtSingleApplication library")
  find_path(QTSINGLEAPPLICATION_INCLUDE_DIRS qtsingleapplication.h PATH_SUFFIXES qt5/QtSolutions)
  find_library(QTSINGLEAPPLICATION_LIBRARIES Qt5Solutions_SingleApplication-2.6)
  find_library(QTSINGLECOREAPPLICATION_LIBRARIES Qt5Solutions_SingleCoreApplication-2.6)
else(USE_SYSTEM_QTSINGLEAPPLICATION)
  message(STATUS "Using builtin QtSingleApplication library")
  add_subdirectory(3rdparty/qtsingleapplication)
  set(QTSINGLEAPPLICATION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qtsingleapplication)
  set(QTSINGLEAPPLICATION_LIBRARIES qtsingleapplication)
endif(USE_SYSTEM_QTSINGLEAPPLICATION)

# Use system Qxt only if explicitly enabled.
option(USE_SYSTEM_QXT "Use system Qxt library" OFF)
if (USE_SYSTEM_QXT)
  message(STATUS "Using system Qxt library")
  find_path(QXTCORE_INCLUDE_DIRS qxtglobal.h PATH_SUFFIXES qt5/QxtCore)
  find_path(QXTGUI_INCLUDE_DIRS qxtglobalshortcut.h PATH_SUFFIXES qt5/QxtWidgets)
  set(QXT_INCLUDE_DIRS ${QXTCORE_INCLUDE_DIRS} ${QXTGUI_INCLUDE_DIRS})
  # We only need its header. We don't need to link to QxtCore.
  find_library(QXT_LIBRARIES QxtWidgets-qt5)
else (USE_SYSTEM_QXT)
  message(STATUS "Using builtin Qxt library")
  add_definitions(-DQXT_STATIC -DBUILD_QXT_GUI -DBUILD_QXT_CORE)
  set(QXT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/qxt)
  set(QXT_LIBRARIES qxt)
  if (NOT APPLE)
    add_subdirectory(3rdparty/qxt)
  endif (NOT APPLE)
endif (USE_SYSTEM_QXT)

# Qocoa
set(QOCOA_LIBRARIES Qocoa)
add_subdirectory(3rdparty/qocoa)

if (APPLE)
  find_library(SPARKLE Sparkle)
  add_subdirectory(3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_LIBRARIES SPMediaKeyTap)
endif (APPLE)

if (WIN32)
  # RC compiler
  string(REPLACE "gcc" "windres" CMAKE_RC_COMPILER_INIT ${CMAKE_C_COMPILER})
  enable_language(RC)
  SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -o <OBJECT> <SOURCE> -I ${CMAKE_SOURCE_DIR}/dist/windows")
endif(WIN32)

# Optional bits
if(WIN32)
  option(ENABLE_WIN32_CONSOLE "Show the windows console even outside Debug mode" OFF)
endif(WIN32)

optional_component(ALSA ON "ALSA integration"
  DEPENDS "alsa" ALSA_FOUND
)

optional_component(LIBPULSE ON "Pulse audio integration"
  DEPENDS "libpulse" LIBPULSE_FOUND
)

optional_component(DBUS ON "D-Bus support"
  DEPENDS "D-Bus" DBUS_FOUND
)

optional_component(GSTREAMER ON "Engine: GStreamer backend"
  DEPENDS "gstreamer-1.0" GSTREAMER_FOUND
  DEPENDS "gstreamer-base-1.0" GSTREAMER_BASE_FOUND
  DEPENDS "gstreamer-app-1.0" GSTREAMER_APP_FOUND
  DEPENDS "gstreamer-audio-1.0" GSTREAMER_AUDIO_FOUND
  DEPENDS "gstreamer-tag-1.0" GSTREAMER_TAG_FOUND
)

optional_component(XINE ON "Engine: Xine backend"
  DEPENDS "libxine" LIBXINE_FOUND
)

optional_component(VLC ON "Engine: VLC backend"
  DEPENDS "libvlc" LIBVLC_FOUND
)

optional_component(PHONON OFF "Engine: Phonon backend (UNSTABLE)"
  DEPENDS "phonon4qt5" PHONON_FOUND
)

if (WIN32)
  optional_component(DEEZER ON "Engine: Deezer backend"
    DEPENDS "libdeezer" LIBDEEZER_FOUND
  )
else ()
  optional_component(DEEZER ON "Engine: Deezer backend"
    DEPENDS "Linux" LINUX
    DEPENDS "libdeezer" LIBDEEZER_FOUND
    DEPENDS "libpulse" LIBPULSE_FOUND
  )
endif()

optional_component(CHROMAPRINT ON "Chromaprint support / Tag fetching from Musicbrainz"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
)

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" CDIO_FOUND
)

optional_component(DEVICEKIT ON "Devices: DeviceKit backend"
  DEPENDS "D-Bus support" DBUS_FOUND
)

optional_component(UDISKS2 ON "Devices: UDisks2 backend"
  DEPENDS "D-Bus support" DBUS_FOUND
)

optional_component(GIO ON "Devices: GIO device backend"
  DEPENDS "libgio" GIO_FOUND
  DEPENDS "Unix or Windows" "NOT APPLE"
)

optional_component(LIBGPOD ON "Devices: iPod classic support"
  DEPENDS "libgpod" LIBGPOD_FOUND
)

optional_component(LIBMTP ON "Devices: MTP support"
  DEPENDS "libmtp" LIBMTP_FOUND
)

optional_component(IMOBILEDEVICE ON "Devices: iPod Touch, iPhone, iPad support"
  DEPENDS "libimobiledevice" LIBIMOBILEDEVICE_FOUND
  DEPENDS "libplist" LIBPLIST_FOUND
  DEPENDS "libusbmuxd" LIBUSBMUXD_FOUND
  DEPENDS "iPod classic support" LIBGPOD_FOUND
)

optional_component(SPARKLE ON "Sparkle integration"
  DEPENDS "Mac OS X" APPLE
  DEPENDS "Sparkle" SPARKLE
)

optional_component(STREAM_TIDAL ON "Streaming: Tidal support")

if (LIBDZMEDIA_FOUND OR LIBDEEZER_FOUND)
  optional_component(STREAM_DEEZER ON "Streaming: Deezer support")
else()
  optional_component(STREAM_DEEZER OFF "Streaming: Deezer support")
endif()

optional_component(DZMEDIA ON "DZMedia"
  DEPENDS "libdzmedia" LIBDZMEDIA_FOUND
  DEPENDS "Deezer support" HAVE_STREAM_DEEZER
)

if (HAVE_STREAM_DEEZER AND NOT HAVE_DZMEDIA AND NOT HAVE_DEEZER)
  message(STATUS "Deezer is enabled, but not DZMedia or Deezer engine, only preview streams will be available.")
endif()

#if(IMOBILEDEVICE_FOUND AND PLIST_FOUND)
  #add_subdirectory(ext/gstafc)
#endif(IMOBILEDEVICE_FOUND AND PLIST_FOUND)

# Set up definitions and paths

add_definitions(-DBOOST_BIND_NO_PLACEHOLDERS)
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_USE_QSTRINGBUILDER)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_NO_CAST_TO_ASCII -DQT_STRICT_ITERATORS)

include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GLIBCONFIG_INCLUDE_DIRS})
include_directories(${TAGLIB_INCLUDE_DIRS})

if(HAVE_IMOBILEDEVICE AND LIBIMOBILEDEVICE_VERSION VERSION_GREATER 1.1.1)
  set(IMOBILEDEVICE_USES_UDIDS ON)
endif()

# Subdirectories
add_subdirectory(src)
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
if(NOT HAVE_GSTREAMER AND NOT HAVE_XINE AND NOT HAVE_VLC AND NOT HAVE_PHONON AND NOT HAVE_DEEZER)
  message(FATAL_ERROR "You need to have either GStreamer, Xine, VLC, Phonon or Deezer to compile!")
elseif(NOT HAVE_GSTREAMER)
  message(WARNING "GStreamer is the only engine that is fully implemented. Using other engines is possible but not recommended.")
endif()
