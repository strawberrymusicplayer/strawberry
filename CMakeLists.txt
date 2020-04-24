project(strawberry)

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0054 NEW)

include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(cmake/Version.cmake)
include(cmake/Summary.cmake)
include(cmake/OptionalSource.cmake)
include(cmake/ParseArguments.cmake)
include(cmake/Rpm.cmake)
include(cmake/Deb.cmake)
include(cmake/Dmg.cmake)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  set(LINUX ON)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  set(FREEBSD ON)
endif()
if (${CMAKE_SYSTEM_NAME} STREQUAL "OpenBSD")
  set(OPENBSD ON)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND COMPILE_OPTIONS
  $<$<COMPILE_LANGUAGE:C>:--std=c99>
  $<$<COMPILE_LANGUAGE:CXX>:--std=c++11>
  -U__STRICT_ANSI__
  -Wall
  -Wextra
  -Wpedantic
  -Wunused
  -Wshadow
  -Wundef
  -Wuninitialized
  -Wredundant-decls
  -Wcast-align
  -Winit-self
  -Wmissing-include-dirs
  -Wmissing-declarations
  -Wstrict-overflow=2
  -Wunused-parameter
  -Wformat=2
  -Wdisabled-optimization
  -Wno-sign-conversion
  $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-old-style-cast>
  $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
)

if(APPLE)
  list(APPEND COMPILE_OPTIONS -Wno-unused-parameter)
endif()

option(BUILD_WERROR "Build with -Werror" OFF)
if(BUILD_WERROR)
  list(APPEND COMPILE_OPTIONS -Werror)
endif(BUILD_WERROR)

add_compile_options(${COMPILE_OPTIONS})

if(${CMAKE_BUILD_TYPE} MATCHES "Release")
  add_definitions(-DNDEBUG)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
  #add_definitions(-DQT_NO_WARNING_OUTPUT)
endif()

if(${CMAKE_BUILD_TYPE} MATCHES "Debug")
  set(DEBUG ON)
endif()

find_program(CCACHE_EXECUTABLE NAMES ccache)
if (CCACHE_EXECUTABLE)
  message(STATUS "ccache found: will be used for compilation and linkage")
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
endif ()

find_package(PkgConfig REQUIRED)
find_package(Boost REQUIRED)
find_package(Threads)
find_package(Backtrace QUIET)
if(Backtrace_FOUND)
  set(HAVE_BACKTRACE ON)
endif()
find_package(GnuTLS)
find_package(Protobuf REQUIRED)
find_library(PROTOBUF_STATIC_LIBRARY libprotobuf.a libprotobuf)
if(LINUX)
  find_package(ALSA REQUIRED)
  pkg_check_modules(DBUS REQUIRED dbus-1)
else(LINUX)
  find_package(ALSA)
  pkg_check_modules(DBUS dbus-1)
endif(LINUX)
if (UNIX AND NOT APPLE)
  find_package(X11)
  pkg_check_modules(XCB xcb)
endif()
if(X11_FOUND)
  set(HAVE_X11 ON)
endif()
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(LIBCDIO libcdio)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_APP gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_TAG gstreamer-tag-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)
pkg_check_modules(LIBXINE libxine)
pkg_check_modules(LIBVLC libvlc)
pkg_check_modules(SQLITE REQUIRED sqlite3>=3.9)
pkg_check_modules(LIBPULSE libpulse)
pkg_check_modules(CHROMAPRINT libchromaprint)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(LIBIMOBILEDEVICE libimobiledevice-1.0)
pkg_check_modules(LIBUSBMUXD libusbmuxd)
pkg_check_modules(LIBPLIST libplist)
find_package(Gettext)
find_package(FFTW3)

# QT
set(QT_MIN_VERSION 5.6)
set(QT_COMPONENTS Core Concurrent Widgets Network Sql)

if(X11_FOUND)
  list(APPEND QT_COMPONENTS X11Extras)
endif()
if(DBUS_FOUND)
  list(APPEND QT_COMPONENTS DBus)
endif()
if(APPLE)
  list(APPEND QT_COMPONENTS MacExtras)
endif()
if(WIN32)
  list(APPEND QT_COMPONENTS WinExtras)
endif()

find_package(Qt5 ${QT_MIN_VERSION} REQUIRED COMPONENTS ${QT_COMPONENTS})

set(QT_LIBRARIES ${Qt5Core_LIBRARIES} ${Qt5Concurrent_LIBRARIES} ${Qt5Widgets_LIBRARIES} ${Qt5Network_LIBRARIES} ${Qt5Sql_LIBRARIES})

if(Qt5DBus_FOUND)
  list(APPEND QT_LIBRARIES ${Qt5DBus_LIBRARIES})
  get_target_property(QT_DBUSXML2CPP_EXECUTABLE Qt5::qdbusxml2cpp LOCATION)
endif()
if(Qt5X11Extras_FOUND)
  list(APPEND QT_LIBRARIES ${Qt5X11Extras_LIBRARIES})
endif()
if(Qt5MacExtras_FOUND)
  list(APPEND QT_LIBRARIES ${Qt5MacExtras_LIBRARIES})
endif()
if(Qt5WinExtras_FOUND)
  list(APPEND QT_LIBRARIES ${Qt5WinExtras_LIBRARIES})
endif()

find_package(Qt5 ${QT_MIN_VERSION} QUIET COMPONENTS LinguistTools CONFIG)
if (Qt5LinguistTools_FOUND)
  set(QT_LCONVERT_EXECUTABLE Qt5::lconvert)
endif()

if(X11_FOUND)
  find_path(KEYSYMDEF_H NAMES "keysymdef.h" PATHS "${X11_INCLUDE_DIR}" PATH_SUFFIXES "X11")
  find_path(XF86KEYSYM_H NAMES "XF86keysym.h" PATHS "${XCB_INCLUDEDIR}" PATH_SUFFIXES "X11")
  if(KEYSYMDEF_H)
    set(HAVE_KEYSYMDEF_H ON)
  else()
    message(WARNING, "Missing X11/keysymdef.h")
  endif()
  if(XF86KEYSYM_H)
    set(HAVE_XF86KEYSYM_H ON)
  else()
    message(WARNING, "Missing X11/XF86keysym.h")
  endif()
endif(X11_FOUND)

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
# Some distros create their own version numbers for taglib so versions are not reliable anymore.
# Force to use our own copy of taglib unless USE_SYSTEM_TAGLIB is set.

option(USE_SYSTEM_TAGLIB "Use system taglib" OFF)
if (TAGLIB_FOUND AND USE_SYSTEM_TAGLIB)
  if (TAGLIB_VERSION VERSION_GREATER 1.11.1)
    message(STATUS "Using system taglib library")
  else()
    message(WARNING "Using system taglib library. Version 1.11.1 or less has a bug corrupting Ogg files, make sure your systems version has been patched!")
  endif()
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
  add_definitions(-DTAGLIB_STATIC)
endif()

# SingleApplication
pkg_check_modules(SINGLEAPPLICATION singleapplication)
if (NOT SINGLEAPPLICATION_FOUND)
  pkg_check_modules(SINGLEAPPLICATION libsingleapplication)
endif()
pkg_check_modules(SINGLECOREAPPLICATION singlecoreapplication)
if (NOT SINGLECOREAPPLICATION_FOUND)
  pkg_check_modules(SINGLECOREAPPLICATION libsinglecoreapplication)
endif()
if (SINGLEAPPLICATION_FOUND AND SINGLECOREAPPLICATION_FOUND)
  option(USE_SYSTEM_SINGLEAPPLICATION "Use system SingleApplication/SingleCoreApplication libraries" ON)
else(SINGLEAPPLICATION_FOUND AND SINGLECOREAPPLICATION_FOUND)
  option(USE_SYSTEM_SINGLEAPPLICATION "Use system SingleApplication/SingleCoreApplication libraries" OFF)
endif(SINGLEAPPLICATION_FOUND AND SINGLECOREAPPLICATION_FOUND)
if(USE_SYSTEM_SINGLEAPPLICATION)
  if (NOT SINGLEAPPLICATION_FOUND OR NOT SINGLECOREAPPLICATION_FOUND)
    message(FATAL_ERROR "Missing SingleApplication or SingleCoreApplication")
  endif()
  message(STATUS "Using system SingleApplication and SingleCoreApplication libraries")
else(USE_SYSTEM_SINGLEAPPLICATION)
  message(STATUS "Using builtin SingleApplication and SingleCoreApplication libraries")
  add_subdirectory(3rdparty/singleapplication)
  set(SINGLEAPPLICATION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/singleapplication)
  set(SINGLEAPPLICATION_LIBRARIES singleapplication)
  set(SINGLECOREAPPLICATION_LIBRARIES singlecoreapplication)
  unset(SINGLEAPPLICATION_LIBRARY_DIRS)
  unset(SINGLECOREAPPLICATION_LIBRARY_DIRS)
endif(USE_SYSTEM_SINGLEAPPLICATION)

if (APPLE)
  find_library(SPARKLE Sparkle)
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
  DEPENDS "gstreamer-pbutils-1.0" GSTREAMER_PBUTILS_FOUND
)

optional_component(XINE ON "Engine: Xine backend"
  DEPENDS "libxine" LIBXINE_FOUND
)

optional_component(VLC ON "Engine: VLC backend"
  DEPENDS "libvlc" LIBVLC_FOUND
)

optional_component(CHROMAPRINT ON "Chromaprint (Tag fetching from Musicbrainz)"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
)

if (X11_FOUND OR HAVE_DBUS OR APPLE OR WIN32)
  set(HAVE_GLOBALSHORTCUTS_SUPPORT ON)
endif()

optional_component(GLOBALSHORTCUTS ON "Global shortcuts"
  DEPENDS "D-Bus, X11, Windows or macOS" HAVE_GLOBALSHORTCUTS_SUPPORT
)

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" LIBCDIO_FOUND
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

optional_component(IMOBILEDEVICE ON "Devices: iPhone, iPod Touch, iPad and Apple TV support"
  DEPENDS "libimobiledevice" LIBIMOBILEDEVICE_FOUND
  DEPENDS "libplist" LIBPLIST_FOUND
  DEPENDS "libusbmuxd" LIBUSBMUXD_FOUND
  DEPENDS "libgpod" HAVE_LIBGPOD
)

optional_component(SPARKLE ON "Sparkle integration"
  DEPENDS "macOS" APPLE
  DEPENDS "Sparkle" SPARKLE
)

optional_component(TRANSLATIONS ON "Translations"
  DEPENDS "gettext" GETTEXT_FOUND
  DEPENDS "Qt5LinguistTools" Qt5LinguistTools_FOUND
)

optional_component(SUBSONIC ON "Subsonic support")
optional_component(TIDAL ON "Tidal support")

optional_component(MOODBAR ON "Moodbar"
  DEPENDS "fftw3" FFTW3_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

if(LINUX OR APPLE)
  option(USE_BUNDLE "Bundle dependencies" OFF)
elseif(WIN32)
  option(USE_BUNDLE "Bundle dependencies" ON)
endif()

if (USE_BUNDLE AND NOT USE_BUNDLE_DIR)
if(LINUX)
  set(USE_BUNDLE_DIR "../plugins")
endif(LINUX)
if(APPLE)
  set(USE_BUNDLE_DIR "../PlugIns")
endif(APPLE)
endif(USE_BUNDLE AND NOT USE_BUNDLE_DIR)

# Check that we have sqlite3 with FTS5

if(NOT CMAKE_CROSSCOMPILING)
  set(CMAKE_REQUIRED_FLAGS "--std=c++11")
  set(CMAKE_REQUIRED_LIBRARIES ${Qt5Core_LIBRARIES} ${Qt5Sql_LIBRARIES})
  set(CMAKE_REQUIRED_INCLUDES ${Qt5Core_INCLUDE_DIRS} ${Qt5Sql_INCLUDE_DIRS})
  check_cxx_source_runs("
    #include <QSqlDatabase>
    #include <QSqlQuery>
    int main() {
      QSqlDatabase db = QSqlDatabase::addDatabase(\"QSQLITE\");
      db.setDatabaseName(\":memory:\");
      if (!db.open()) { return 1; }
      QSqlQuery q(db);
      q.prepare(\"CREATE VIRTUAL TABLE test_fts USING fts5(test, tokenize = 'unicode61 remove_diacritics 0');\");
      if (!q.exec()) return 1;
    }
    "
    SQLITE3_FTS5
  )
endif()

if(HAVE_XINE)
  check_cxx_source_compiles("
    #define METRONOM_INTERNAL
    #include <iostream>
    #include <xine/metronom.h>
    int main() {
      metronom_t metronom;
      std::cout << metronom.pts_per_smpls;
      return 0;
    }
  "
  XINE_ANALYZER)
endif()

# Set up definitions

add_definitions(-DBOOST_BIND_NO_PLACEHOLDERS)
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DQT_USE_QSTRINGBUILDER)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DQT_NO_CAST_TO_ASCII)

# Subdirectories
add_subdirectory(src)
add_subdirectory(dist)
add_subdirectory(tests)
add_subdirectory(ext/libstrawberry-common)
add_subdirectory(ext/libstrawberry-tagreader)
add_subdirectory(ext/strawberry-tagreader)
if(HAVE_MOODBAR)
  add_subdirectory(ext/gstmoodbar)
endif()

# Uninstall support
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# Show a summary of what we have enabled
summary_show()
if(NOT HAVE_GSTREAMER AND NOT HAVE_XINE AND NOT HAVE_VLC)
  message(FATAL_ERROR "You need to have either GStreamer, Xine or VLC to compile!")
elseif(NOT HAVE_GSTREAMER)
  message(WARNING "GStreamer is the only engine that is fully implemented. Using other engines is possible but not recommended.")
endif()

if(NOT SQLITE3_FTS5 AND NOT CMAKE_CROSSCOMPILING)
  message(WARNING "sqlite3 must be enabled with FTS5. See: https://www.sqlite.org/fts5.html")
endif()
