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
cmake_policy(SET CMP0054 NEW)
if(${CMAKE_VERSION} VERSION_GREATER "3.10.3") 
  cmake_policy(SET CMP0072 NEW)
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

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX 1)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  set(FREEBSD 1)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
  set(OPENBSD 1)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
#set(CMAKE_BUILD_TYPE Debug)

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
  add_definitions(-DNDEBUG)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
  #add_definitions(-DQT_NO_WARNING_OUTPUT)
endif(${CMAKE_BUILD_TYPE} MATCHES "Release")

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
  find_package(DBus REQUIRED)
else(LINUX)
  find_package(ALSA)
  find_package(DBus)
endif(LINUX)
if(ALSA_FOUND)
  set(HAVE_ALSA ON)
endif()
find_package(X11)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_APP gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_TAG gstreamer-tag-1.0)
pkg_check_modules(LIBXINE libxine)
pkg_check_modules(LIBVLC libvlc)
pkg_check_modules(PHONON phonon4qt5)
pkg_check_modules(SQLITE REQUIRED sqlite3>=3.7)
find_package(OpenGL REQUIRED)
pkg_check_modules(CHROMAPRINT REQUIRED libchromaprint)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(LIBPULSE libpulse)
pkg_check_modules(LIBXML libxml-2.0)
pkg_check_modules(IMOBILEDEVICE libimobiledevice-1.0)
pkg_check_modules(USBMUXD libusbmuxd)
pkg_check_modules(PLIST libplist)

if(WIN32)
  find_package(ZLIB REQUIRED)
endif(WIN32)

# QT
set(QT_MIN_VERSION 5.6.0)
find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core OpenGL Sql Network Xml Widgets Concurrent Test)
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

if(UNIX AND X11_FOUND AND DBUS_FOUND)
  set(QT_LIBRARIES Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network Qt5::Sql Qt5::OpenGL Qt5::Xml Qt5::X11Extras Qt5::DBus)
elseif(UNIX AND X11_FOUND)
  set(QT_LIBRARIES Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network Qt5::Sql Qt5::OpenGL Qt5::Xml Qt5::X11Extras)
elseif(APPLE)
  set(QT_LIBRARIES Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network Qt5::Sql Qt5::OpenGL Qt5::Xml Qt5::MacExtras)
else()
  set(QT_LIBRARIES Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network Qt5::Sql Qt5::OpenGL Qt5::Xml)
endif()

# Don't try to use webkit if their include directories couldn't be found.
if (NOT QT_QTWEBKIT_INCLUDE_DIR)
  set (QT_USE_QTWEBKIT 0)
endif (NOT QT_QTWEBKIT_INCLUDE_DIR)

# TAGLIB
pkg_check_modules(TAGLIB taglib)
# Only use system taglib if it's greater than 1.11.1 because of audio file detection by content.
# But let the user override as strawberry will still compile and work without.
if (TAGLIB_VERSION VERSION_GREATER 1.11.1 OR WIN32)
  option(USE_SYSTEM_TAGLIB "Use system taglib" ON)
else()
  option(USE_SYSTEM_TAGLIB "Use system taglib" OFF)
endif()
if (TAGLIB_FOUND AND USE_SYSTEM_TAGLIB)
  message(STATUS "Using system taglib library")
  set(CMAKE_REQUIRED_INCLUDES "${TAGLIB_INCLUDE_DIRS}")
  set(CMAKE_REQUIRED_LIBRARIES "${TAGLIB_LIBRARIES}")
  set(CMAKE_REQUIRED_INCLUDES)
  set(CMAKE_REQUIRED_LIBRARIES)
else()
  message(STATUS "Using builtin taglib library")
  set(TAGLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/taglib/;${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/")
  set(TAGLIB_LIBRARY_DIRS "")
  set(TAGLIB_LIBRARIES tag)
  add_subdirectory(3rdparty/utf8-cpp)
  add_subdirectory(3rdparty/taglib)
endif()

# LASTFM
find_library(LASTFM5_LIBRARIES lastfm5)
find_path(LASTFM5_INCLUDE_DIRS lastfm5/ws.h)
find_path(LASTFM51_INCLUDE_DIRS lastfm5/Track.h)
if(LASTFM5_INCLUDE_DIRS AND LASTFM51_INCLUDE_DIRS)
  set(HAVE_LIBLASTFM1 ON)
endif()

# CHECK INCLUDES
CHECK_INCLUDE_FILES(chromaprint.h CHROMAPRINT_H)

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

# Use system QtSingleApplication only if explicitly enabled.
option(USE_SYSTEM_QTSINGLEAPPLICATION "Use system QtSingleApplication library" OFF)
if(USE_SYSTEM_QTSINGLEAPPLICATION)
  message(STATUS "Using system QtSingleApplication library")
  find_path(QTSINGLEAPPLICATION_INCLUDE_DIRS qtsingleapplication.h PATH_SUFFIXES QtSolutions)
  find_library(QTSINGLEAPPLICATION_LIBRARIES QtSolutions_SingleApplication-2.6)
  find_library(QTSINGLECOREAPPLICATION_LIBRARIES QtSolutions_SingleCoreApplication-2.6)
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
  find_path(QXTCORE_INCLUDE_DIRS qxtglobal.h PATH_SUFFIXES QxtCore)
  find_path(QXTGUI_INCLUDE_DIRS qxtglobalshortcut.h PATH_SUFFIXES QxtGui)
  set(QXT_INCLUDE_DIRS ${QXTCORE_INCLUDE_DIRS} ${QXTGUI_INCLUDE_DIRS})
  # We only need its header. We don't need to link to QxtCore.
  find_library(QXT_LIBRARIES QxtGui)
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

# Windows
if (WIN32)
  add_subdirectory(3rdparty/qtwin)
endif (WIN32)

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

optional_component(PHONON OFF "Engine: Phonon backend"
  DEPENDS "phonon4qt5" PHONON_FOUND
)

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" CDIO_FOUND
)

optional_component(LIBGPOD ON "Devices: iPod classic support"
  DEPENDS "libgpod" LIBGPOD_FOUND
)

optional_component(GIO ON "Devices: GIO device backend"
  DEPENDS "libgio" GIO_FOUND
  DEPENDS "Unix or Windows" "NOT APPLE"
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

optional_component(LIBLASTFM ON "Last.fm album cover provider"
  DEPENDS "liblastfm" LASTFM5_LIBRARIES LASTFM5_INCLUDE_DIRS
)

optional_component(DEVICEKIT ON "Devices: DeviceKit backend"
  DEPENDS "D-Bus support" DBUS_FOUND
)

optional_component(UDISKS2 ON "Devices: UDisks2 backend"
  DEPENDS "D-Bus support" DBUS_FOUND
)

optional_component(LIBPULSE ON "Pulse audio integration"
  DEPENDS "libpulse" LIBPULSE_FOUND
)

optional_component(SPARKLE ON "Sparkle integration"
  DEPENDS "Mac OS X" APPLE
  DEPENDS "Sparkle" SPARKLE
)

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

if(ENABLE_IMOBILEDEVICE AND IMOBILEDEVICE_VERSION VERSION_GREATER 1.1.1)
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
if(NOT HAVE_GSTREAMER AND NOT HAVE_XINE AND NOT HAVE_VLC AND NOT HAVE_PHONON)
  message(FATAL_ERROR "You need to enable either GStreamer, Xine, VLC or Phonon to compile!")
endif()
