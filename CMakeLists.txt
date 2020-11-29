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
if(APPLE)
  include(cmake/Dmg.cmake)
endif()

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

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND COMPILE_OPTIONS
  $<$<COMPILE_LANGUAGE:C>:--std=c99>
  $<$<COMPILE_LANGUAGE:CXX>:--std=c++17>
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
  $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>
  $<$<COMPILE_LANGUAGE:CXX>:-Wno-old-style-cast>
  $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
)

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

if(APPLE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks")
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
find_package(Iconv QUIET)
find_package(GnuTLS REQUIRED)
find_package(Protobuf REQUIRED)
if (NOT Protobuf_PROTOC_EXECUTABLE)
  message(FATAL_ERROR "Missing protobuf compiler.")
endif()
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
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(LIBCDIO libcdio)
pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_APP gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_TAG gstreamer-tag-1.0)
pkg_check_modules(GSTREAMER_PBUTILS gstreamer-pbutils-1.0)
pkg_check_modules(LIBVLC libvlc)
pkg_check_modules(SQLITE REQUIRED sqlite3>=3.9)
pkg_check_modules(LIBPULSE libpulse)
pkg_check_modules(CHROMAPRINT libchromaprint)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(GDK_PIXBUF gdk-pixbuf-2.0)
find_package(Gettext)
find_package(FFTW3)

if(NOT QT_DEFAULT_MAJOR_VERSION)
  set(QT_DEFAULT_MAJOR_VERSION 5)
endif()
set(QT_MAJOR_VERSION ${QT_DEFAULT_MAJOR_VERSION} CACHE STRING "Qt version to use (5 or 6), defaults to ${QT_DEFAULT_MAJOR_VERSION}")

option(BUILD_WITH_QT5 "Use Qt 5" OFF)
option(BUILD_WITH_QT6 "Use Qt 6" OFF)

if(WITH_QT6)
  set(BUILD_WITH_QT6 ON)
endif()

if(BUILD_WITH_QT5)
  set(QT_MAJOR_VERSION 5)
elseif(BUILD_WITH_QT6)
  set(QT_MAJOR_VERSION 6)
else()
  if(QT_MAJOR_VERSION EQUAL 5)
    set(BUILD_WITH_QT5 ON)
  elseif(QT_MAJOR_VERSION EQUAL 6)
    set(BUILD_WITH_QT6 ON)
  else()
    set(BUILD_WITH_QT5 ON)
    set(QT_MAJOR_VERSION 5)
  endif()
endif()

set(QT_COMPONENTS Core Concurrent Widgets Network Sql)
unset(OPTIONAL_COMPONENTS)

if(DBUS_FOUND)
  list(APPEND QT_COMPONENTS DBus)
endif()
if(X11_FOUND)
  list(APPEND OPTIONAL_COMPONENTS X11Extras)
endif()
if(WIN32)
  list(APPEND OPTIONAL_COMPONENTS WinExtras)
endif()

find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS ${QT_COMPONENTS})
if(OPTIONAL_COMPONENTS)
  find_package(Qt${QT_MAJOR_VERSION} OPTIONAL_COMPONENTS ${OPTIONAL_COMPONENTS})
endif(OPTIONAL_COMPONENTS)

set(QtCore_LIBRARIES Qt${QT_MAJOR_VERSION}::Core)
set(QtConcurrent_LIBRARIES Qt${QT_MAJOR_VERSION}::Concurrent)
set(QtWidgets_LIBRARIES Qt${QT_MAJOR_VERSION}::Widgets)
set(QtNetwork_LIBRARIES Qt${QT_MAJOR_VERSION}::Network)
set(QtSql_LIBRARIES Qt${QT_MAJOR_VERSION}::Sql)
set(QT_LIBRARIES Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Concurrent Qt${QT_MAJOR_VERSION}::Widgets Qt${QT_MAJOR_VERSION}::Network Qt${QT_MAJOR_VERSION}::Sql)
if(Qt${QT_MAJOR_VERSION}DBus_FOUND)
  set(QtDBus_LIBRARIES Qt${QT_MAJOR_VERSION}::DBus)
  list(APPEND QT_LIBRARIES Qt${QT_MAJOR_VERSION}::DBus)
  get_target_property(QT_DBUSXML2CPP_EXECUTABLE Qt${QT_MAJOR_VERSION}::qdbusxml2cpp LOCATION)
endif()
if(Qt${QT_MAJOR_VERSION}X11Extras_FOUND)
  set(QtX11Extras_LIBRARIES Qt${QT_MAJOR_VERSION}::X11Extras)
  list(APPEND QT_LIBRARIES Qt${QT_MAJOR_VERSION}::X11Extras)
  set(HAVE_X11EXTRAS ON)
endif()
if(Qt${QT_MAJOR_VERSION}WinExtras_FOUND)
  set(QtWinExtras_LIBRARIES Qt${QT_MAJOR_VERSION}::WinExtras)
  list(APPEND QT_LIBRARIES Qt${QT_MAJOR_VERSION}::WinExtras)
  set(HAVE_WINEXTRAS ON)
endif()

find_package(Qt${QT_MAJOR_VERSION} QUIET COMPONENTS LinguistTools CONFIG)
if(Qt${QT_MAJOR_VERSION}LinguistTools_FOUND)
  set(QT_LCONVERT_EXECUTABLE Qt${QT_MAJOR_VERSION}::lconvert)
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

find_path(QPA_QPLATFORMNATIVEINTERFACE_H qpa/qplatformnativeinterface.h PATHS ${Qt${QT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS})
if(QPA_QPLATFORMNATIVEINTERFACE_H)
  set(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H ON)
  include_directories(${Qt${QT_MAJOR_VERSION}Gui_PRIVATE_INCLUDE_DIRS})
  message(STATUS "Have qpa/qplatformnativeinterface.h header.")
else()
  message(STATUS "Missing qpa/qplatformnativeinterface.h header.")
endif()

# TAGLIB
option(USE_SYSTEM_TAGLIB "Use system taglib" OFF)
if(USE_SYSTEM_TAGLIB)
  pkg_check_modules(TAGLIB REQUIRED taglib>=1.11.1)
  message(WARNING "Using system taglib library.")
  find_path(HAVE_TAGLIB_DSFFILE_H taglib/dsffile.h)
  find_path(HAVE_TAGLIB_DSDIFFFILE_H taglib/dsdifffile.h)
  if(HAVE_TAGLIB_DSFFILE_H)
    set(HAVE_TAGLIB_DSFFILE ON)
  endif(HAVE_TAGLIB_DSFFILE_H)
  if(HAVE_TAGLIB_DSDIFFFILE_H)
    set(HAVE_TAGLIB_DSDIFFFILE ON)
  endif(HAVE_TAGLIB_DSDIFFFILE_H)
else(USE_SYSTEM_TAGLIB)
  set(TAGLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/taglib/;${CMAKE_BINARY_DIR}/3rdparty/taglib/headers/")
  set(TAGLIB_LIBRARY_DIRS "")
  set(TAGLIB_LIBRARIES tag)
  add_subdirectory(3rdparty/utf8-cpp)
  add_subdirectory(3rdparty/taglib)
  set(HAVE_TAGLIB_DSFFILE ON)
  set(HAVE_TAGLIB_DSDIFFFILE ON)
  add_definitions(-DTAGLIB_STATIC)
endif(USE_SYSTEM_TAGLIB)

# SingleApplication
add_subdirectory(3rdparty/singleapplication)
set(SINGLEAPPLICATION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/singleapplication)
set(SINGLEAPPLICATION_LIBRARIES singleapplication)
set(SINGLECOREAPPLICATION_LIBRARIES singlecoreapplication)

if(APPLE)
  find_library(SPARKLE Sparkle)
endif(APPLE)

if(NOT SPARKLE AND (APPLE OR WIN32))
  if(BUILD_WITH_QT6)
    pkg_check_modules(QTSPARKLE qtsparkle-qt6)
  else()
    pkg_check_modules(QTSPARKLE qtsparkle-qt5)
  endif()
  if(QTSPARKLE_FOUND)
    set(HAVE_QTSPARKLE ON)
  endif()
endif()

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

optional_component(X11_GLOBALSHORTCUTS ON "X11 global shortcuts"
  DEPENDS "X11Extras" Qt${QT_MAJOR_VERSION}X11Extras_FOUND
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
  DEPENDS "gdk-pixbuf" GDK_PIXBUF_FOUND
)

optional_component(LIBMTP ON "Devices: MTP support"
  DEPENDS "libmtp" LIBMTP_FOUND
)

optional_component(SPARKLE ON "Sparkle integration"
  DEPENDS "macOS" APPLE
  DEPENDS "Sparkle" SPARKLE
)

if(BUILD_WITH_QT6)
  optional_component(TRANSLATIONS ON "Translations"
    DEPENDS "gettext" GETTEXT_FOUND
    DEPENDS "Qt6LinguistTools" Qt6LinguistTools_FOUND
  )
else()
  optional_component(TRANSLATIONS ON "Translations"
    DEPENDS "gettext" GETTEXT_FOUND
    DEPENDS "Qt5LinguistTools" Qt5LinguistTools_FOUND
  )
endif()

option(INSTALL_TRANSLATIONS "Install translations" OFF)

optional_component(SUBSONIC ON "Subsonic support")
optional_component(TIDAL ON "Tidal support")
optional_component(QOBUZ ON "Qobuz support")

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
  set(CMAKE_REQUIRED_FLAGS "--std=c++17")
  set(CMAKE_REQUIRED_LIBRARIES ${QtCore_LIBRARIES} ${QtSql_LIBRARIES})
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
add_subdirectory(ext/libstrawberry-common)
add_subdirectory(ext/libstrawberry-tagreader)
add_subdirectory(ext/strawberry-tagreader)
if(HAVE_MOODBAR)
  add_subdirectory(ext/gstmoodbar)
endif()

option(BUILD_TESTS "Build the test suite" OFF)
option(BUILD_TAGLIB_TESTS "Build the test suite" OFF)

if(BUILD_TESTS)
  add_subdirectory(tests)
endif(BUILD_TESTS)
if(NOT USE_SYSTEM_TAGLIB AND BUILD_TAGLIB_TESTS)
  add_subdirectory(tests/taglib)
endif(NOT USE_SYSTEM_TAGLIB AND BUILD_TAGLIB_TESTS)

# Uninstall support
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# Show a summary of what we have enabled
summary_show()
if(NOT HAVE_GSTREAMER AND NOT HAVE_VLC)
  message(FATAL_ERROR "You need to have either GStreamer or VLC to compile!")
elseif(NOT HAVE_GSTREAMER)
  message(WARNING "GStreamer is the only engine that is fully implemented. Using other engines is possible but not recommended.")
endif()

if(NOT SQLITE3_FTS5 AND NOT CMAKE_CROSSCOMPILING)
  message(WARNING "sqlite3 must be enabled with FTS5. See: https://www.sqlite.org/fts5.html")
endif()

if(USE_SYSTEM_TAGLIB AND NOT TAGLIB_VERSION VERSION_GREATER 1.11.2)
  message(WARNING "Using system taglib library.  There is a critical bug in the current latest version of TagLib (1.11.1) that can corrupt Ogg files, make sure your systems version has been patched, see: https://github.com/taglib/taglib/issues/864, TagLib upstream is currently not maintained.  Do not set USE_SYSTEM_TAGLIB unless you are prepared keep the TagLib in your system up to date with critical fixes.")
endif()
