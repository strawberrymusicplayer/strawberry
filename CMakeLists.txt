cmake_minimum_required(VERSION 3.7)

project(strawberry)

if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

include(CheckCXXCompilerFlag)
include(CheckCXXSourceRuns)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(cmake/Version.cmake)
include(cmake/Summary.cmake)
include(cmake/OptionalSource.cmake)
include(cmake/ParseArguments.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LINUX ON)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  set(FREEBSD ON)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  set(OPENBSD ON)
endif()

if(LINUX)
  include(cmake/Rpm.cmake)
  include(cmake/Deb.cmake)
endif()
if(APPLE)
  include(cmake/Dmg.cmake)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(MSVC)
  set(CMAKE_C_STANDARD 99)
else()
  set(CMAKE_C_STANDARD 11)
endif()

set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  list(APPEND COMPILE_OPTIONS /MP)
else()
  list(APPEND COMPILE_OPTIONS
    $<$<COMPILE_LANGUAGE:C>:-std=c11>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>
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
    $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
  )
  option(BUILD_WERROR "Build with -Werror" OFF)
  if(BUILD_WERROR)
    list(APPEND COMPILE_OPTIONS -Werror)
  endif()
endif()

add_compile_options(${COMPILE_OPTIONS})

if(CMAKE_BUILD_TYPE MATCHES "Release")
  add_definitions(-DNDEBUG)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

option(USE_RPATH "Use RPATH" APPLE)
if(USE_RPATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

find_program(CCACHE_EXECUTABLE NAMES ccache)
if(CCACHE_EXECUTABLE)
  message(STATUS "ccache found: will be used for compilation and linkage")
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
endif()

option(USE_ICU "Use ICU" ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Backtrace)
if(Backtrace_FOUND)
  set(HAVE_BACKTRACE ON)
endif()
find_package(Boost REQUIRED)
if(USE_ICU)
  find_package(ICU COMPONENTS uc i18n REQUIRED)
  if(ICU_FOUND)
    set(HAVE_ICU ON)
  endif()
else()
  find_package(Iconv)
endif()
find_package(Protobuf CONFIG)
if(NOT Protobuf_FOUND)
  find_package(Protobuf REQUIRED)
endif()
if(NOT TARGET protobuf::protoc)
  message(FATAL_ERROR "Missing Protobuf compiler.")
endif()
if(LINUX)
  find_package(ALSA REQUIRED)
  pkg_check_modules(DBUS REQUIRED dbus-1)
else()
  find_package(ALSA)
  pkg_check_modules(DBUS dbus-1)
endif()
if(UNIX AND NOT APPLE)
  find_package(X11)
  pkg_check_modules(XCB xcb)
endif()
if(X11_FOUND)
  set(HAVE_X11 ON)
endif()
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
if(UNIX)
  pkg_check_modules(GIO_UNIX gio-unix-2.0)
endif()
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
pkg_check_modules(CHROMAPRINT libchromaprint>=1.4)
pkg_check_modules(LIBGPOD libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP libmtp>=1.0)
pkg_check_modules(GDK_PIXBUF gdk-pixbuf-2.0)
find_package(Gettext)
find_package(FFTW3)
find_package(GTest)
find_library(GMOCK_LIBRARY gmock)

option(BUILD_WITH_QT5 "Build with Qt 5" OFF)
option(BUILD_WITH_QT6 "Build with Qt 6" OFF)

if(BUILD_WITH_QT6)
  set(QT_VERSION_MAJOR 6)
elseif(BUILD_WITH_QT5)
  set(QT_VERSION_MAJOR 5)
endif()

if(NOT QT_VERSION_MAJOR)
  message(STATUS "QT_VERSION_MAJOR, BUILD_WITH_QT5 or BUILD_WITH_QT6 not set, detecting Qt version...")
  find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
endif()

if(QT_VERSION_MAJOR EQUAL 6)
  set(QT_MIN_VERSION 6.0)
elseif(QT_VERSION_MAJOR EQUAL 5)
  set(QT_MIN_VERSION 5.12)
else()
  message(FATAL_ERROR "Invalid QT_VERSION_MAJOR.")
endif()

set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})

set(QT_COMPONENTS Core Concurrent Gui Widgets Network Sql)
set(QT_OPTIONAL_COMPONENTS LinguistTools Test)
if(DBUS_FOUND AND NOT WIN32)
  list(APPEND QT_COMPONENTS DBus)
endif()
if(X11_FOUND AND QT_VERSION_MAJOR EQUAL 5)
  list(APPEND QT_COMPONENTS X11Extras)
endif()

find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED OPTIONAL_COMPONENTS ${QT_OPTIONAL_COMPONENTS})

if(Qt${QT_VERSION_MAJOR}DBus_FOUND)
  get_target_property(QT_DBUSXML2CPP_EXECUTABLE Qt${QT_VERSION_MAJOR}::qdbusxml2cpp LOCATION)
endif()
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
  get_target_property(QT_LCONVERT_EXECUTABLE Qt${QT_VERSION_MAJOR}::lconvert LOCATION)
endif()
if(Qt${QT_VERSION_MAJOR}X11Extras_FOUND)
  set(HAVE_X11EXTRAS ON)
endif()

if(QT_VERSION_MAJOR EQUAL 5 AND Qt5Core_VERSION VERSION_LESS 5.15.0)
  macro(qt_add_resources)
    qt5_add_resources(${ARGN})
  endmacro()
  macro(qt_wrap_cpp)
    qt5_wrap_cpp(${ARGN})
  endmacro()
  macro(qt_wrap_ui)
    qt5_wrap_ui(${ARGN})
  endmacro()
  macro(qt_add_dbus_adaptor)
    qt5_add_dbus_adaptor(${ARGN})
  endmacro()
  macro(qt_add_dbus_interface)
    qt5_add_dbus_interface(${ARGN})
  endmacro()
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

  find_path(QPA_QPLATFORMNATIVEINTERFACE_H qpa/qplatformnativeinterface.h PATHS ${Qt${QT_VERSION_MAJOR}Gui_PRIVATE_INCLUDE_DIRS})
  if(QPA_QPLATFORMNATIVEINTERFACE_H)
    set(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H ON)
    message(STATUS "Have qpa/qplatformnativeinterface.h header.")
  else()
    message(STATUS "Missing qpa/qplatformnativeinterface.h header.")
  endif()

  # Check for QX11Application (Qt 6 compiled with XCB).
  if(QT_VERSION_MAJOR EQUAL 6 AND Qt6Gui_VERSION VERSION_GREATER_EQUAL 6.2.0)
    set(CMAKE_REQUIRED_FLAGS "-std=c++17")
    set(CMAKE_REQUIRED_LIBRARIES Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Gui)
    check_cxx_source_compiles("
    #include <QGuiApplication>
    int main() {
      (void)qApp->nativeInterface<QNativeInterface::QX11Application>();
      return 0;
    }
    "
      HAVE_QX11APPLICATION
      )
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_REQUIRED_LIBRARIES)
  endif()

endif(X11_FOUND)

option(USE_TAGLIB "Build with TagLib" OFF)
option(USE_TAGPARSER "Build with TagParser" OFF)

if(NOT USE_TAGLIB AND NOT USE_TAGPARSER)
  set(USE_TAGLIB ON)
endif()

# TAGLIB
if(USE_TAGLIB)
  pkg_check_modules(TAGLIB REQUIRED taglib>=1.11.1)
  if(TAGLIB_FOUND)
    find_path(HAVE_TAGLIB_DSFFILE_H taglib/dsffile.h)
    find_path(HAVE_TAGLIB_DSDIFFFILE_H taglib/dsdifffile.h)
    if(HAVE_TAGLIB_DSFFILE_H)
      set(HAVE_TAGLIB_DSFFILE ON)
    endif(HAVE_TAGLIB_DSFFILE_H)
    if(HAVE_TAGLIB_DSDIFFFILE_H)
      set(HAVE_TAGLIB_DSDIFFFILE ON)
    endif(HAVE_TAGLIB_DSDIFFFILE_H)
  endif()
endif()

# TAGPARSER
if(USE_TAGPARSER)
  pkg_check_modules(TAGPARSER REQUIRED tagparser)
endif()

pkg_check_modules(LIBEBUR128 IMPORTED_TARGET libebur128)

if(NOT TAGLIB_FOUND AND NOT TAGPARSER_FOUND)
  message(FATAL_ERROR "You need either TagLib or TagParser!")
endif()

# SingleApplication
if(QT_VERSION_MAJOR EQUAL 5)
  set(KDSINGLEAPPLICATION_NAME "KDSingleApplication")
else()
  set(KDSINGLEAPPLICATION_NAME "KDSingleApplication-qt${QT_VERSION_MAJOR}")
endif()
find_package(${KDSINGLEAPPLICATION_NAME} 1.1.0)
if(TARGET KDAB::kdsingleapplication)
  if(QT_VERSION_MAJOR EQUAL 5)
    set(KDSINGLEAPPLICATION_VERSION "${KDSingleApplication_VERSION}")
  elseif(QT_VERSION_MAJOR EQUAL 6)
    set(KDSINGLEAPPLICATION_VERSION "${KDSingleApplication-qt6_VERSION}")
  endif()
  message(STATUS "Using system KDSingleApplication (Version ${KDSINGLEAPPLICATION_VERSION})")
  set(SINGLEAPPLICATION_LIBRARIES KDAB::kdsingleapplication)
else()
  message(STATUS "Using 3rdparty KDSingleApplication")
  set(HAVE_KDSINGLEAPPLICATION_OPTIONS ON)
  add_subdirectory(3rdparty/kdsingleapplication)
  set(SINGLEAPPLICATION_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/kdsingleapplication/KDSingleApplication/src)
  set(SINGLEAPPLICATION_LIBRARIES kdsingleapplication)
  add_definitions(-DKDSINGLEAPPLICATION_STATIC_BUILD)
endif()

if(APPLE)
  add_subdirectory(3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/SPMediaKeyTap)
  set(SPMEDIAKEYTAP_LIBRARIES SPMediaKeyTap)
  add_subdirectory(ext/macdeploycheck)
endif()

if(WIN32)
  pkg_check_modules(QTSPARKLE qtsparkle-qt${QT_VERSION_MAJOR})
  if(QTSPARKLE_FOUND)
    set(HAVE_QTSPARKLE ON)
  endif()
endif()

if(WIN32)
  add_subdirectory(3rdparty/getopt)
  set(GETOPT_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/getopt)
  set(GETOPT_LIBRARIES getopt)
  add_definitions(-DSTATIC_GETOPT -D_UNICODE)
endif()

if(WIN32 AND NOT MSVC)
  # RC compiler
  string(REPLACE "gcc" "windres" CMAKE_RC_COMPILER_INIT ${CMAKE_C_COMPILER})
  enable_language(RC)
  SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -o <OBJECT> <SOURCE> -I ${CMAKE_SOURCE_DIR}/dist/windows")
endif()

if(WIN32)
  option(ENABLE_WIN32_CONSOLE "Show the windows console even outside Debug mode" OFF)
endif()

optional_component(ALSA ON "ALSA integration"
  DEPENDS "alsa" ALSA_FOUND
)

optional_component(LIBPULSE ON "PulseAudio integration"
  DEPENDS "libpulse" LIBPULSE_FOUND
)

optional_component(DBUS ON "D-Bus support"
  DEPENDS "D-Bus" DBUS_FOUND
  DEPENDS "Qt D-Bus" Qt${QT_VERSION_MAJOR}DBus_FOUND
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

optional_component(SONGFINGERPRINTING ON "Song fingerprinting and tracking"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

optional_component(MUSICBRAINZ ON "MusicBrainz integration"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

if(X11_FOUND OR (HAVE_DBUS AND Qt${QT_VERSION_MAJOR}DBus_FOUND) OR APPLE OR WIN32)
  set(HAVE_GLOBALSHORTCUTS_SUPPORT ON)
endif()

optional_component(GLOBALSHORTCUTS ON "Global shortcuts"
  DEPENDS "D-Bus, X11, Windows or macOS" HAVE_GLOBALSHORTCUTS_SUPPORT
)

if(HAVE_QX11APPLICATION OR HAVE_X11EXTRAS OR HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
  set(X11_GLOBALSHORTCUTS_REQUIREMENT_FOUND ON)
endif()
optional_component(X11_GLOBALSHORTCUTS ON "X11 global shortcuts"
  DEPENDS "X11" X11_FOUND
  DEPENDS "QX11Application, X11Extras or qpa/qplatformnativeinterface.h header" X11_GLOBALSHORTCUTS_REQUIREMENT_FOUND
)

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" LIBCDIO_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

optional_component(UDISKS2 ON "Devices: UDisks2 backend"
  DEPENDS "D-Bus" DBUS_FOUND
  DEPENDS "Qt D-Bus" Qt${QT_VERSION_MAJOR}DBus_FOUND
)

optional_component(GIO ON "Devices: GIO device backend"
  DEPENDS "libgio" GIO_FOUND
  DEPENDS "Unix or Windows" "NOT APPLE"
)

optional_component(GIO_UNIX ON "Devices: GIO device backend (Unix support)"
  DEPENDS "libgio-unix" GIO_UNIX_FOUND
  DEPENDS "Unix or Windows" "NOT APPLE"
)

optional_component(LIBGPOD ON "Devices: iPod classic support"
  DEPENDS "libgpod" LIBGPOD_FOUND
  DEPENDS "gdk-pixbuf" GDK_PIXBUF_FOUND
)

optional_component(LIBMTP ON "Devices: MTP support"
  DEPENDS "libmtp" LIBMTP_FOUND
)

optional_component(TRANSLATIONS ON "Translations"
  DEPENDS "gettext" GETTEXT_FOUND
  DEPENDS "Qt LinguistTools" Qt${QT_VERSION_MAJOR}LinguistTools_FOUND
)

option(INSTALL_TRANSLATIONS "Install translations" OFF)

optional_component(SUBSONIC ON "Streaming: Subsonic")
optional_component(TIDAL ON "Streaming: Tidal")
optional_component(QOBUZ ON "Streaming: Qobuz")

optional_component(MOODBAR ON "Moodbar"
  DEPENDS "fftw3" FFTW3_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

optional_component(EBUR128 ON "EBU R 128 loudness normalization"
  DEPENDS "libebur128" LIBEBUR128_FOUND
  DEPENDS "gstreamer" HAVE_GSTREAMER
)

if(APPLE OR WIN32)
  set(USE_BUNDLE_DEFAULT ON)
else()
  set(USE_BUNDLE_DEFAULT OFF)
endif()
option(USE_BUNDLE "Bundle dependencies" ${USE_BUNDLE_DEFAULT})

if(NOT CMAKE_CROSSCOMPILING)
  # Check that we have Qt with sqlite driver
  set(CMAKE_REQUIRED_FLAGS "-std=c++17")
  set(CMAKE_REQUIRED_LIBRARIES Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
  check_cxx_source_runs("
    #include <QSqlDatabase>
    #include <QSqlQuery>
    int main() {
      QSqlDatabase db = QSqlDatabase::addDatabase(\"QSQLITE\");
      db.setDatabaseName(\":memory:\");
      if (!db.open()) { return 1; }
      QSqlQuery q(db);
      q.prepare(\"CREATE TABLE test (test TEXT);\");
      if (!q.exec()) return 1;
    }
    "
    QT_SQLITE_TEST
  )
  if(QT_SQLITE_TEST)
    # Check that we have sqlite3 with FTS5
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
      SQLITE_FTS5_TEST
    )
  endif()
  unset(CMAKE_REQUIRED_FLAGS)
  unset(CMAKE_REQUIRED_LIBRARIES)
endif()

# Set up definitions

add_definitions(
  -DBOOST_BIND_NO_PLACEHOLDERS
  -DQT_STRICT_ITERATORS
  -DQT_NO_CAST_FROM_BYTEARRAY
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_TO_ASCII
  -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
  -DQT_NO_FOREACH
)

if(WIN32)
  add_definitions(-DUNICODE)
  if(MSVC)
    add_definitions(-DPROTOBUF_USE_DLLS)
  endif()
endif()

# Subdirectories
add_subdirectory(src)
add_subdirectory(dist)
add_subdirectory(ext/libstrawberry-common)
add_subdirectory(ext/libstrawberry-tagreader)
add_subdirectory(ext/strawberry-tagreader)
if(HAVE_MOODBAR)
  add_subdirectory(ext/gstmoodbar)
endif()

if(GTest_FOUND AND GMOCK_LIBRARY AND Qt${QT_VERSION_MAJOR}Test_FOUND)
  add_subdirectory(tests)
endif()

if(LINUX AND LSB_RELEASE_EXEC AND DPKG_BUILDPACKAGE)
  add_subdirectory(debian)
endif()

# Uninstall support
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)

add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

# Show a summary of what we have enabled
summary_show()
if(NOT HAVE_GSTREAMER AND NOT HAVE_VLC)
  message(FATAL_ERROR "You need to have either GStreamer or libvlc to compile!")
elseif(NOT HAVE_GSTREAMER)
  message(WARNING "GStreamer is the only engine that is fully implemented. Using other engines is possible but not recommended.")
endif()

if(QT_VERSION_MAJOR EQUAL 5)
  message(WARNING "It is detected that Strawberry is being built with Qt 5.  There are no bugfix releases for the latest minor LTS version of Qt 5 available to open-source users, only commercial users.  Therefore Strawberry should be built with Qt 6 when possible. Building with Qt 6 will also take advantage of improvements and new features not available in Qt 5.  To build with Qt 6 specify -DBUILD_WITH_QT6=ON to automatically detect Qt 6, or for example -DCMAKE_PREFIX_PATH=/usr/local/lib64/cmake to manually specify the Qt 6 directory.")
endif()

if(NOT CMAKE_CROSSCOMPILING)
  if(QT_SQLITE_TEST)
    if(NOT SQLITE_FTS5_TEST)
      message(WARNING "sqlite must be enabled with FTS5. See: https://www.sqlite.org/fts5.html")
    endif()
  else()
    message(WARNING "The Qt sqlite driver test failed.")
  endif()
endif()

if(USE_TAGLIB AND TAGLIB_FOUND AND NOT TAGLIB_VERSION VERSION_GREATER_EQUAL 1.12)
  message(WARNING "There is a critical bug in TagLib (1.11.1) that can result in corrupt Ogg files, see: https://github.com/taglib/taglib/issues/864, please consider updating TagLib to the newest version.")
endif()
