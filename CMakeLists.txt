cmake_minimum_required(VERSION 3.13)

project(strawberry C CXX)

if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

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
include(cmake/OptionalComponent.cmake)
include(cmake/OptionalSource.cmake)
include(cmake/ParseArguments.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(LINUX ON)
endif()

if(LINUX)
  include(cmake/Rpm.cmake)
  include(cmake/Deb.cmake)
endif()

if(APPLE)
  include(cmake/Dmg.cmake)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if(CMAKE_BUILD_TYPE MATCHES "Release")
  add_definitions(-DNDEBUG)
  set(ENABLE_DEBUG_OUTPUT_DEFAULT OFF)
else()
  set(ENABLE_DEBUG_OUTPUT_DEFAULT ON)
endif()

if(APPLE OR WIN32)
  set(USE_BUNDLE_DEFAULT ON)
else()
  set(USE_BUNDLE_DEFAULT OFF)
endif()

if(WIN32)
  if(CMAKE_BUILD_TYPE MATCHES "Release")
    set(ENABLE_WIN32_CONSOLE_DEFAULT OFF)
  else()
    set(ENABLE_WIN32_CONSOLE_DEFAULT ON)
  endif()
endif()

option(BUILD_WERROR "Build with -Werror" OFF)
option(USE_RPATH "Use RPATH" APPLE)
option(USE_INSTALL_PREFIX "Look for data in CMAKE_INSTALL_PREFIX" ON)
option(INSTALL_TRANSLATIONS "Install translations" OFF)
option(ENABLE_DEBUG_OUTPUT "Enable debug output" ${ENABLE_DEBUG_OUTPUT_DEFAULT})
option(USE_BUNDLE "Bundle dependencies" ${USE_BUNDLE_DEFAULT})

if(WIN32)
  option(ENABLE_WIN32_CONSOLE "Show the windows console even outside Debug mode" ${ENABLE_WIN32_CONSOLE_DEFAULT})
endif()

if(MSVC)
  set(CMAKE_C_STANDARD 99)
else()
  set(CMAKE_C_STANDARD 11)
endif()

set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  list(APPEND COMPILE_OPTIONS -MP -W4 -wd4702)
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
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND COMPILE_OPTIONS -Wno-unused-command-line-argument)
  endif()
endif()

add_compile_options(${COMPILE_OPTIONS})

add_definitions(
  -DBOOST_BIND_NO_PLACEHOLDERS
  -DQT_STRICT_ITERATORS
  -DQT_NO_CAST_FROM_BYTEARRAY
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_TO_ASCII
  -DQT_NO_NARROWING_CONVERSIONS_IN_CONNECT
  -DQT_NO_FOREACH
  -DQT_ASCII_CAST_WARNINGS
  -DQT_NO_CAST_FROM_ASCII
  -DQT_NO_KEYWORDS
  -DQT_NO_SIGNALS_SLOTS_KEYWORDS
)

if(WIN32)
  add_definitions(
    -DUNICODE
    -DNOMINMAX
  )
endif()

if(BUILD_WERROR)
  set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
endif()

if(NOT ENABLE_DEBUG_OUTPUT)
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

if(USE_RPATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

find_program(CCACHE_EXECUTABLE NAMES ccache)
if(CCACHE_EXECUTABLE)
  message(STATUS "ccache found: will be used for compilation and linkage")
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_EXECUTABLE})
  SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ${CCACHE_EXECUTABLE})
endif()

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Backtrace)
if(Backtrace_FOUND)
  set(HAVE_BACKTRACE ON)
endif()
find_package(Boost CONFIG)
if(NOT Boost_FOUND)
  find_package(Boost REQUIRED)
endif()
find_package(ICU COMPONENTS uc i18n REQUIRED)
if(UNIX AND NOT APPLE)
  if(LINUX)
    find_package(ALSA REQUIRED)
  else()
    find_package(ALSA)
  endif()
  find_package(X11 COMPONENTS X11_xcb)
endif()
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GOBJECT REQUIRED IMPORTED_TARGET gobject-2.0)
if(NOT APPLE)
  pkg_check_modules(GIO REQUIRED IMPORTED_TARGET gio-2.0)
  if(GIO_FOUND AND UNIX)
    pkg_check_modules(GIO_UNIX IMPORTED_TARGET gio-unix-2.0)
  endif()
endif()
pkg_check_modules(LIBCDIO IMPORTED_TARGET libcdio)
pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE REQUIRED IMPORTED_TARGET gstreamer-base-1.0)
pkg_check_modules(GSTREAMER_AUDIO REQUIRED IMPORTED_TARGET gstreamer-audio-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED IMPORTED_TARGET gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_TAG REQUIRED IMPORTED_TARGET gstreamer-tag-1.0)
pkg_check_modules(GSTREAMER_PBUTILS REQUIRED IMPORTED_TARGET gstreamer-pbutils-1.0)
pkg_check_modules(SQLITE REQUIRED IMPORTED_TARGET sqlite3>=3.9)
if(UNIX AND NOT APPLE)
  pkg_check_modules(LIBPULSE IMPORTED_TARGET libpulse)
endif()
pkg_check_modules(CHROMAPRINT IMPORTED_TARGET libchromaprint>=1.4)
pkg_check_modules(FFTW3 IMPORTED_TARGET fftw3)
pkg_check_modules(LIBEBUR128 IMPORTED_TARGET libebur128)
pkg_check_modules(LIBGPOD IMPORTED_TARGET libgpod-1.0>=0.7.92)
pkg_check_modules(LIBMTP IMPORTED_TARGET libmtp>=1.0)
pkg_check_modules(GDK_PIXBUF IMPORTED_TARGET gdk-pixbuf-2.0)

find_package(TagLib 2.0)
if(TARGET TagLib::TagLib)
  set(TAGLIB_FOUND ON)
  set(TAGLIB_LIBRARIES TagLib::TagLib)
  set(HAVE_TAGLIB_DSFFILE ON)
  set(HAVE_TAGLIB_DSDIFFFILE ON)
else()
  pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib>=1.12)
endif()

find_package(GTest)

pkg_check_modules(LIBSPARSEHASH IMPORTED_TARGET libsparsehash)

find_package(RapidJSON)

set(QT_VERSION_MAJOR 6)
set(QT_MIN_VERSION 6.4.0)
set(QT_DEFAULT_MAJOR_VERSION ${QT_VERSION_MAJOR})
set(QT_COMPONENTS Core Concurrent Gui Widgets Network Sql)
set(QT_OPTIONAL_COMPONENTS GuiPrivate LinguistTools Test)
if(UNIX AND NOT APPLE)
  list(APPEND QT_OPTIONAL_COMPONENTS DBus)
endif()
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt${QT_VERSION_MAJOR} ${QT_MIN_VERSION} COMPONENTS ${QT_COMPONENTS} REQUIRED OPTIONAL_COMPONENTS ${QT_OPTIONAL_COMPONENTS})

if(TARGET "Qt${QT_VERSION_MAJOR}::GuiPrivate")
  set(QT_GUI_PRIVATE_FOUND ON)
endif()

if(Qt${QT_VERSION_MAJOR}DBus_FOUND)
  set(DBUS_FOUND ON)
endif()

if(X11_FOUND)
  # Check for QX11Application (Qt 6 compiled with XCB).
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

find_package(KDSingleApplication-qt${QT_VERSION_MAJOR} 1.1.0 REQUIRED)

if(APPLE)
  find_library(SPARKLE Sparkle)
  #find_package(SPMediaKeyTap REQUIRED)
endif()

if(WIN32)
  find_package(getopt NAMES getopt getopt-win unofficial-getopt-win32 REQUIRED)
  if(TARGET getopt::getopt)
    set(GETOPT_LIBRARIES getopt::getopt)
  elseif(TARGET getopt-win::getopt)
    set(GETOPT_LIBRARIES getopt-win::getopt)
  elseif(TARGET getopt::getopt_shared)
    set(GETOPT_LIBRARIES getopt::getopt_shared)
  else()
    message(FATAL_ERROR "Missing getopt")
  endif()
endif()

if(APPLE OR WIN32)
  find_package(qtsparkle-qt${QT_VERSION_MAJOR})
  if(TARGET "qtsparkle-qt${QT_VERSION_MAJOR}::qtsparkle")
    set(QTSPARKLE_FOUND ON)
  endif()
endif()

if(UNIX AND NOT APPLE)
  optional_component(ALSA ON "ALSA integration"
    DEPENDS "alsa" ALSA_FOUND
  )
  optional_component(PULSE ON "PulseAudio integration"
    DEPENDS "libpulse" LIBPULSE_FOUND
  )
  optional_component(DBUS ON "D-Bus support"
    DEPENDS "Qt D-Bus" DBUS_FOUND
  )
  optional_component(MPRIS2 ON "MPRIS2 D-Bus Interface"
    DEPENDS "D-Bus support" HAVE_DBUS
  )
endif()

optional_component(SONGFINGERPRINTING ON "Song fingerprinting and tracking"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
)

optional_component(MUSICBRAINZ ON "MusicBrainz integration"
  DEPENDS "chromaprint" CHROMAPRINT_FOUND
)

if(UNIX AND NOT APPLE)
  optional_component(X11_GLOBALSHORTCUTS ON "X11 global shortcuts"
    DEPENDS "X11" X11_FOUND
    DEPENDS "QX11Application" HAVE_QX11APPLICATION
  )
  optional_component(KGLOBALACCEL_GLOBALSHORTCUTS ON "KGlobalAccel global shortcuts"
    DEPENDS "D-Bus support" HAVE_DBUS
  )
  optional_component(UDISKS2 ON "Devices: UDisks2 backend"
    DEPENDS "D-Bus support" HAVE_DBUS
  )
endif()

if(NOT APPLE)
  optional_component(GIO ON "Devices: GIO device backend"
    DEPENDS "libgio" GIO_FOUND
  )
  if(UNIX)
    optional_component(GIO_UNIX ON "Devices: GIO device backend (Unix support)"
      DEPENDS "libgio-unix" GIO_UNIX_FOUND
    )
  endif()
endif()

optional_component(AUDIOCD ON "Devices: Audio CD support"
  DEPENDS "libcdio" LIBCDIO_FOUND
)

optional_component(MTP ON "Devices: MTP support"
  DEPENDS "libmtp" LIBMTP_FOUND
)

optional_component(GPOD ON "Devices: iPod classic support"
  DEPENDS "libgpod" LIBGPOD_FOUND
  DEPENDS "gdk-pixbuf" GDK_PIXBUF_FOUND
)

optional_component(TRANSLATIONS ON "Translations"
  DEPENDS "Qt LinguistTools" Qt${QT_VERSION_MAJOR}LinguistTools_FOUND
)

optional_component(SUBSONIC ON "Streaming: Subsonic")
optional_component(TIDAL ON "Streaming: Tidal")
optional_component(SPOTIFY ON "Streaming: Spotify")
optional_component(QOBUZ ON "Streaming: Qobuz")

optional_component(MOODBAR ON "Moodbar"
  DEPENDS "fftw3" FFTW3_FOUND
)

optional_component(EBUR128 ON "EBU R 128 loudness normalization"
  DEPENDS "libebur128" LIBEBUR128_FOUND
)

if(APPLE)
  optional_component(SPARKLE ON "Sparkle integration"
    DEPENDS "Sparkle" SPARKLE
  )
endif()

if(APPLE OR WIN32)
  optional_component(QTSPARKLE ON "QtSparkle integration"
    DEPENDS "QtSparkle" QTSPARKLE_FOUND
  )
endif()

optional_component(QPA_QPLATFORMNATIVEINTERFACE ON "QPA Platform Native Interface"
  DEPENDS "Qt Gui Private" QT_GUI_PRIVATE_FOUND
)

optional_component(STREAMTAGREADER ON "Stream tagreader"
  DEPENDS "sparsehash" LIBSPARSEHASH_FOUND
)

optional_component(DISCORD_RPC ON "Discord Rich Presence"
  DEPENDS "RapidJSON" RapidJSON_FOUND
)

if(HAVE_SONGFINGERPRINTING OR HAVE_MUSICBRAINZ)
  set(HAVE_CHROMAPRINT ON)
endif()

if(HAVE_X11_GLOBALSHORTCUTS OR HAVE_KGLOBALACCEL_GLOBALSHORTCUTS OR APPLE OR WIN32)
  set(HAVE_GLOBALSHORTCUTS ON)
endif()

if(NOT CMAKE_CROSSCOMPILING)
  # Check that we have Qt with sqlite driver
  set(CMAKE_REQUIRED_FLAGS "-std=c++17")
  set(CMAKE_REQUIRED_LIBRARIES Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Sql)
  check_cxx_source_runs("
    #include <QCoreApplication>
    #include <QSqlDatabase>
    #include <QSqlQuery>
    int main(int argc, char *argv[]) {
      QCoreApplication app(argc, argv);
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
endif()

add_executable(strawberry)

if(APPLE)
  set_target_properties(strawberry PROPERTIES MACOSX_BUNDLE TRUE)
  set_target_properties(strawberry PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/dist/macos/Info.plist")
endif()

if(WIN32 AND NOT ENABLE_WIN32_CONSOLE)
  set_target_properties(strawberry PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

set(SOURCES
  src/core/logging.cpp
  src/core/mainwindow.cpp
  src/core/application.cpp
  src/core/playerinterface.cpp
  src/core/player.cpp
  src/core/commandlineoptions.cpp
  src/core/database.cpp
  src/core/memorydatabase.cpp
  src/core/sqlquery.cpp
  src/core/sqlrow.cpp
  src/core/metatypes.cpp
  src/core/deletefiles.cpp
  src/core/filesystemmusicstorage.cpp
  src/core/filesystemwatcherinterface.cpp
  src/core/mergedproxymodel.cpp
  src/core/multisortfilterproxy.cpp
  src/core/musicstorage.cpp
  src/core/networkaccessmanager.cpp
  src/core/threadsafenetworkdiskcache.cpp
  src/core/networktimeouts.cpp
  src/core/networkproxyfactory.cpp
  src/core/qtfslistener.cpp
  src/core/settings.cpp
  src/core/settingsprovider.cpp
  src/core/signalchecker.cpp
  src/core/song.cpp
  src/core/songloader.cpp
  src/core/stylehelper.cpp
  src/core/stylesheetloader.cpp
  src/core/taskmanager.cpp
  src/core/thread.cpp
  src/core/urlhandler.cpp
  src/core/urlhandlers.cpp
  src/core/iconloader.cpp
  src/core/standarditemiconloader.cpp
  src/core/scopedtransaction.cpp
  src/core/translations.cpp
  src/core/localredirectserver.cpp
  src/core/mimedata.cpp
  src/core/temporaryfile.cpp
  src/core/enginemetadata.cpp
  src/core/songmimedata.cpp
  src/core/platforminterface.cpp
  src/core/standardpaths.cpp
  src/core/httpbaserequest.cpp
  src/core/jsonbaserequest.cpp
  src/core/oauthenticator.cpp

  src/utilities/strutils.cpp
  src/utilities/envutils.cpp
  src/utilities/colorutils.cpp
  src/utilities/cryptutils.cpp
  src/utilities/fileutils.cpp
  src/utilities/diskutils.cpp
  src/utilities/imageutils.cpp
  src/utilities/macaddrutils.cpp
  src/utilities/mimeutils.cpp
  src/utilities/randutils.cpp
  src/utilities/threadutils.cpp
  src/utilities/timeutils.cpp
  src/utilities/transliterate.cpp
  src/utilities/xmlutils.cpp
  src/utilities/filemanagerutils.cpp
  src/utilities/coverutils.cpp
  src/utilities/screenutils.cpp
  src/utilities/textencodingutils.cpp
  src/utilities/coveroptions.cpp
  src/utilities/musixmatchprovider.cpp

  src/tagreader/tagreaderclient.cpp
  src/tagreader/tagreaderresult.cpp
  src/tagreader/tagreaderbase.cpp
  src/tagreader/tagreadertaglib.cpp
  src/tagreader/tagreadergme.cpp
  src/tagreader/tagreaderrequest.cpp
  src/tagreader/tagreaderismediafilerequest.cpp
  src/tagreader/tagreaderreadfilerequest.cpp
  src/tagreader/tagreaderwritefilerequest.cpp
  src/tagreader/tagreaderloadcoverdatarequest.cpp
  src/tagreader/tagreaderloadcoverimagerequest.cpp
  src/tagreader/tagreadersavecoverrequest.cpp
  src/tagreader/tagreadersaveplaycountrequest.cpp
  src/tagreader/tagreadersaveratingrequest.cpp
  src/tagreader/albumcovertagdata.cpp
  src/tagreader/savetagcoverdata.cpp
  src/tagreader/tagreaderreply.cpp
  src/tagreader/tagreaderreadfilereply.cpp
  src/tagreader/tagreaderloadcoverdatareply.cpp
  src/tagreader/tagreaderloadcoverimagereply.cpp

  src/filterparser/filterparser.cpp
  src/filterparser/filtertree.cpp
  src/filterparser/filtertreeand.cpp
  src/filterparser/filtertreecolumnterm.cpp
  src/filterparser/filtertreenop.cpp
  src/filterparser/filtertreenot.cpp
  src/filterparser/filtertreeor.cpp
  src/filterparser/filtertreeterm.cpp
  src/filterparser/filterparserfloateqcomparator.cpp
  src/filterparser/filterparserfloatgecomparator.cpp
  src/filterparser/filterparserfloatgtcomparator.cpp
  src/filterparser/filterparserfloatlecomparator.cpp
  src/filterparser/filterparserfloatltcomparator.cpp
  src/filterparser/filterparserfloatnecomparator.cpp
  src/filterparser/filterparserint64eqcomparator.cpp
  src/filterparser/filterparserint64gecomparator.cpp
  src/filterparser/filterparserint64gtcomparator.cpp
  src/filterparser/filterparserint64lecomparator.cpp
  src/filterparser/filterparserint64ltcomparator.cpp
  src/filterparser/filterparserint64necomparator.cpp
  src/filterparser/filterparserinteqcomparator.cpp
  src/filterparser/filterparserintgecomparator.cpp
  src/filterparser/filterparserintgtcomparator.cpp
  src/filterparser/filterparserintlecomparator.cpp
  src/filterparser/filterparserintltcomparator.cpp
  src/filterparser/filterparserintnecomparator.cpp
  src/filterparser/filterparsersearchtermcomparator.cpp
  src/filterparser/filterparsertextcontainscomparator.cpp
  src/filterparser/filterparsertexteqcomparator.cpp
  src/filterparser/filterparsertextnecomparator.cpp
  src/filterparser/filterparseruinteqcomparator.cpp
  src/filterparser/filterparseruintgecomparator.cpp
  src/filterparser/filterparseruintgtcomparator.cpp
  src/filterparser/filterparseruintlecomparator.cpp
  src/filterparser/filterparseruintltcomparator.cpp
  src/filterparser/filterparseruintnecomparator.cpp

  src/engine/enginebase.cpp
  src/engine/enginedevice.cpp
  src/engine/devicefinders.cpp
  src/engine/devicefinder.cpp
  src/engine/gststartup.cpp
  src/engine/gstengine.cpp
  src/engine/gstenginepipeline.cpp

  src/analyzer/fht.cpp
  src/analyzer/analyzerbase.cpp
  src/analyzer/analyzercontainer.cpp
  src/analyzer/blockanalyzer.cpp
  src/analyzer/boomanalyzer.cpp
  src/analyzer/turbineanalyzer.cpp
  src/analyzer/sonogramanalyzer.cpp
  src/analyzer/waverubberanalyzer.cpp
  src/analyzer/rainbowanalyzer.cpp

  src/equalizer/equalizer.cpp
  src/equalizer/equalizerslider.cpp

  src/context/contextview.cpp
  src/context/contextalbum.cpp

  src/collection/collectionlibrary.cpp
  src/collection/collectionmodel.cpp
  src/collection/collectionbackend.cpp
  src/collection/collectionwatcher.cpp
  src/collection/collectionview.cpp
  src/collection/collectionitem.cpp
  src/collection/collectionitemdelegate.cpp
  src/collection/collectionviewcontainer.cpp
  src/collection/collectiondirectorymodel.cpp
  src/collection/collectionfilteroptions.cpp
  src/collection/collectionfilterwidget.cpp
  src/collection/collectionfilter.cpp
  src/collection/collectionplaylistitem.cpp
  src/collection/collectionquery.cpp
  src/collection/savedgroupingmanager.cpp
  src/collection/groupbydialog.cpp
  src/collection/collectiontask.cpp
  src/collection/collectionmodelupdate.cpp

  src/playlist/playlist.cpp
  src/playlist/playlistbackend.cpp
  src/playlist/playlistcontainer.cpp
  src/playlist/playlistdelegates.cpp
  src/playlist/playlistfilter.cpp
  src/playlist/playlistheader.cpp
  src/playlist/playlistitem.cpp
  src/playlist/songplaylistitem.cpp
  src/playlist/streamplaylistitem.cpp
  src/playlist/playlistitemmimedata.cpp
  src/playlist/playlistlistcontainer.cpp
  src/playlist/playlistlistmodel.cpp
  src/playlist/playlistlistsortfiltermodel.cpp
  src/playlist/playlistlistview.cpp
  src/playlist/playlistmanagerinterface.cpp
  src/playlist/playlistmanager.cpp
  src/playlist/playlistsaveoptionsdialog.cpp
  src/playlist/playlistsequence.cpp
  src/playlist/playlisttabbar.cpp
  src/playlist/playlistview.cpp
  src/playlist/playlistproxystyle.cpp
  src/playlist/songloaderinserter.cpp
  src/playlist/dynamicplaylistcontrols.cpp
  src/playlist/playlistundocommandbase.cpp
  src/playlist/playlistundocommandinsertitems.cpp
  src/playlist/playlistundocommandremoveitems.cpp
  src/playlist/playlistundocommandmoveitems.cpp
  src/playlist/playlistundocommandreorderitems.cpp
  src/playlist/playlistundocommandsortitems.cpp
  src/playlist/playlistundocommandshuffleitems.cpp

  src/queue/queue.cpp
  src/queue/queueview.cpp

  src/playlistparsers/asxiniparser.cpp
  src/playlistparsers/asxparser.cpp
  src/playlistparsers/cueparser.cpp
  src/playlistparsers/m3uparser.cpp
  src/playlistparsers/parserbase.cpp
  src/playlistparsers/playlistparser.cpp
  src/playlistparsers/plsparser.cpp
  src/playlistparsers/wplparser.cpp
  src/playlistparsers/xmlparser.cpp
  src/playlistparsers/xspfparser.cpp

  src/smartplaylists/playlistgenerator.cpp
  src/smartplaylists/playlistgeneratorinserter.cpp
  src/smartplaylists/playlistgeneratormimedata.cpp
  src/smartplaylists/playlistquerygenerator.cpp
  src/smartplaylists/smartplaylistquerywizardplugin.cpp
  src/smartplaylists/smartplaylistquerywizardpluginsortpage.cpp
  src/smartplaylists/smartplaylistquerywizardpluginsearchpage.cpp
  src/smartplaylists/smartplaylistsearch.cpp
  src/smartplaylists/smartplaylistsearchpreview.cpp
  src/smartplaylists/smartplaylistsearchterm.cpp
  src/smartplaylists/smartplaylistsearchtermwidget.cpp
  src/smartplaylists/smartplaylistsearchtermwidgetoverlay.cpp
  src/smartplaylists/smartplaylistsmodel.cpp
  src/smartplaylists/smartplaylistsviewcontainer.cpp
  src/smartplaylists/smartplaylistsview.cpp
  src/smartplaylists/smartplaylistwizard.cpp
  src/smartplaylists/smartplaylistwizardplugin.cpp
  src/smartplaylists/smartplaylistwizardtypepage.cpp
  src/smartplaylists/smartplaylistwizardfinishpage.cpp

  src/covermanager/albumcovermanager.cpp
  src/covermanager/albumcovermanagerlist.cpp
  src/covermanager/albumcoverloader.cpp
  src/covermanager/albumcoverloaderoptions.cpp
  src/covermanager/albumcoverfetcher.cpp
  src/covermanager/albumcoverfetchersearch.cpp
  src/covermanager/albumcoversearcher.cpp
  src/covermanager/albumcoverexport.cpp
  src/covermanager/albumcoverexporter.cpp
  src/covermanager/albumcoverchoicecontroller.cpp
  src/covermanager/coverprovider.cpp
  src/covermanager/coverproviders.cpp
  src/covermanager/coversearchstatistics.cpp
  src/covermanager/coversearchstatisticsdialog.cpp
  src/covermanager/coverexportrunnable.cpp
  src/covermanager/currentalbumcoverloader.cpp
  src/covermanager/coverfromurldialog.cpp
  src/covermanager/jsoncoverprovider.cpp
  src/covermanager/lastfmcoverprovider.cpp
  src/covermanager/musicbrainzcoverprovider.cpp
  src/covermanager/discogscoverprovider.cpp
  src/covermanager/deezercoverprovider.cpp
  src/covermanager/musixmatchcoverprovider.cpp
  src/covermanager/opentidalcoverprovider.cpp

  src/lyrics/lyricsproviders.cpp
  src/lyrics/lyricsprovider.cpp
  src/lyrics/lyricssearchrequest.h
  src/lyrics/lyricssearchresult.h
  src/lyrics/lyricsfetcher.cpp
  src/lyrics/lyricsfetchersearch.cpp
  src/lyrics/jsonlyricsprovider.cpp
  src/lyrics/htmllyricsprovider.cpp
  src/lyrics/ovhlyricsprovider.cpp
  src/lyrics/lololyricsprovider.cpp
  src/lyrics/geniuslyricsprovider.cpp
  src/lyrics/musixmatchlyricsprovider.cpp
  src/lyrics/chartlyricsprovider.cpp
  src/lyrics/songlyricscomlyricsprovider.cpp
  src/lyrics/azlyricscomlyricsprovider.cpp
  src/lyrics/elyricsnetlyricsprovider.cpp
  src/lyrics/letraslyricsprovider.cpp
  src/lyrics/lyricfindlyricsprovider.cpp

  src/settings/settingsdialog.cpp
  src/settings/settingspage.cpp
  src/settings/settingsitemdelegate.cpp
  src/settings/behavioursettingspage.cpp
  src/settings/collectionsettingspage.cpp
  src/settings/collectionsettingsdirectorymodel.cpp
  src/settings/backendsettingspage.cpp
  src/settings/playlistsettingspage.cpp
  src/settings/scrobblersettingspage.cpp
  src/settings/coverssettingspage.cpp
  src/settings/lyricssettingspage.cpp
  src/settings/networkproxysettingspage.cpp
  src/settings/appearancesettingspage.cpp
  src/settings/contextsettingspage.cpp
  src/settings/notificationssettingspage.cpp
  src/settings/transcodersettingspage.cpp

  src/dialogs/about.cpp
  src/dialogs/console.cpp
  src/dialogs/errordialog.cpp
  src/dialogs/edittagdialog.cpp
  src/dialogs/trackselectiondialog.cpp
  src/dialogs/addstreamdialog.cpp
  src/dialogs/userpassdialog.cpp
  src/dialogs/deleteconfirmationdialog.cpp
  src/dialogs/lastfmimportdialog.cpp
  src/dialogs/messagedialog.cpp
  src/dialogs/snapdialog.cpp
  src/dialogs/saveplaylistsdialog.cpp

  src/widgets/autoexpandingtreeview.cpp
  src/widgets/busyindicator.cpp
  src/widgets/clickablelabel.cpp
  src/widgets/fancytabwidget.cpp
  src/widgets/fancytabbar.cpp
  src/widgets/fancytabdata.cpp
  src/widgets/favoritewidget.cpp
  src/widgets/forcescrollperpixel.cpp
  src/widgets/freespacebar.cpp
  src/widgets/groupediconview.cpp
  src/widgets/lineedit.cpp
  src/widgets/linetextedit.cpp
  src/widgets/multiloadingindicator.cpp
  src/widgets/playingwidget.cpp
  src/widgets/renametablineedit.cpp
  src/widgets/sliderslider.cpp
  src/widgets/prettyslider.cpp
  src/widgets/volumeslider.cpp
  src/widgets/stickyslider.cpp
  src/widgets/stretchheaderview.cpp
  src/widgets/trackslider.cpp
  src/widgets/tracksliderpopup.cpp
  src/widgets/tracksliderslider.cpp
  src/widgets/loginstatewidget.cpp
  src/widgets/ratingwidget.cpp
  src/widgets/resizabletextedit.cpp

  src/osd/osdbase.cpp
  src/osd/osdpretty.cpp

  src/streaming/streamingservices.cpp
  src/streaming/streamingservice.cpp
  src/streaming/streamserviceplaylistitem.cpp
  src/streaming/streamingsearchview.cpp
  src/streaming/streamingsearchmodel.cpp
  src/streaming/streamingsearchsortmodel.cpp
  src/streaming/streamingsearchitemdelegate.cpp
  src/streaming/streamingsongsview.cpp
  src/streaming/streamingtabsview.cpp
  src/streaming/streamingcollectionview.cpp
  src/streaming/streamingcollectionviewcontainer.cpp
  src/streaming/streamingsearchview.cpp
  src/streaming/streamsongmimedata.cpp

  src/radios/radioservices.cpp
  src/radios/radiobackend.cpp
  src/radios/radiomodel.cpp
  src/radios/radioview.cpp
  src/radios/radioviewcontainer.cpp
  src/radios/radioservice.cpp
  src/radios/radiostreamplaylistitem.cpp
  src/radios/radiochannel.cpp
  src/radios/somafmservice.cpp
  src/radios/radioparadiseservice.cpp
  src/radios/radiomimedata.cpp

  src/scrobbler/audioscrobbler.cpp
  src/scrobbler/scrobblersettingsservice.cpp
  src/scrobbler/scrobblerservice.cpp
  src/scrobbler/scrobblercache.cpp
  src/scrobbler/scrobblercacheitem.cpp
  src/scrobbler/scrobblemetadata.cpp
  src/scrobbler/scrobblingapi20.cpp
  src/scrobbler/lastfmscrobbler.cpp
  src/scrobbler/librefmscrobbler.cpp
  src/scrobbler/listenbrainzscrobbler.cpp
  src/scrobbler/lastfmimport.cpp

  src/organize/organize.cpp
  src/organize/organizeformat.cpp
  src/organize/organizeformatvalidator.cpp
  src/organize/organizesyntaxhighlighter.cpp
  src/organize/organizedialog.cpp
  src/organize/organizeerrordialog.cpp

  src/transcoder/transcoder.cpp
  src/transcoder/transcoderoptionsinterface.cpp
  src/transcoder/transcodedialog.cpp
  src/transcoder/transcoderoptionsdialog.cpp
  src/transcoder/transcoderoptionsflac.cpp
  src/transcoder/transcoderoptionswavpack.cpp
  src/transcoder/transcoderoptionsvorbis.cpp
  src/transcoder/transcoderoptionsopus.cpp
  src/transcoder/transcoderoptionsspeex.cpp
  src/transcoder/transcoderoptionsaac.cpp
  src/transcoder/transcoderoptionsasf.cpp
  src/transcoder/transcoderoptionsmp3.cpp

  src/systemtrayicon/systemtrayicon.cpp

  src/fileview/fileview.cpp
  src/fileview/fileviewlist.cpp

  src/device/devicemanager.cpp
  src/device/devicelister.cpp
  src/device/devicedatabasebackend.cpp
  src/device/deviceinfo.cpp
  src/device/deviceproperties.cpp
  src/device/filesystemdevice.cpp
  src/device/connecteddevice.cpp
  src/device/devicestatefiltermodel.cpp
  src/device/deviceviewcontainer.cpp
  src/device/deviceview.cpp
)

set(HEADERS
  src/core/logging.h
  src/core/mainwindow.h
  src/core/application.h
  src/core/player.h
  src/core/playerinterface.h
  src/core/database.h
  src/core/memorydatabase.h
  src/core/deletefiles.h
  src/core/filesystemwatcherinterface.h
  src/core/mergedproxymodel.h
  src/core/multisortfilterproxy.h
  src/core/networkaccessmanager.h
  src/core/threadsafenetworkdiskcache.h
  src/core/networktimeouts.h
  src/core/qtfslistener.h
  src/core/settings.h
  src/core/songloader.h
  src/core/taskmanager.h
  src/core/thread.h
  src/core/urlhandlers.h
  src/core/urlhandler.h
  src/core/standarditemiconloader.h
  src/core/mimedata.h
  src/core/stylesheetloader.h
  src/core/localredirectserver.h
  src/core/songmimedata.h
  src/core/httpbaserequest.h
  src/core/jsonbaserequest.h
  src/core/oauthenticator.h

  src/tagreader/tagreaderclient.h
  src/tagreader/tagreaderreply.h
  src/tagreader/tagreaderreadfilereply.h
  src/tagreader/tagreaderloadcoverdatareply.h
  src/tagreader/tagreaderloadcoverimagereply.h

  src/engine/enginebase.h
  src/engine/devicefinders.h
  src/engine/gstengine.h
  src/engine/gstenginepipeline.h

  src/analyzer/analyzerbase.h
  src/analyzer/analyzercontainer.h
  src/analyzer/blockanalyzer.h
  src/analyzer/boomanalyzer.h
  src/analyzer/turbineanalyzer.h
  src/analyzer/sonogramanalyzer.h
  src/analyzer/waverubberanalyzer.h
  src/analyzer/rainbowanalyzer.h

  src/equalizer/equalizer.h
  src/equalizer/equalizerslider.h

  src/context/contextview.h
  src/context/contextalbum.h

  src/collection/collectionlibrary.h
  src/collection/collectionmodel.h
  src/collection/collectionbackend.h
  src/collection/collectionwatcher.h
  src/collection/collectionview.h
  src/collection/collectionitemdelegate.h
  src/collection/collectionviewcontainer.h
  src/collection/collectiondirectorymodel.h
  src/collection/collectionfilterwidget.h
  src/collection/collectionfilter.h
  src/collection/savedgroupingmanager.h
  src/collection/groupbydialog.h

  src/playlist/playlist.h
  src/playlist/playlistbackend.h
  src/playlist/playlistcontainer.h
  src/playlist/playlistdelegates.h
  src/playlist/playlistfilter.h
  src/playlist/playlistheader.h
  src/playlist/playlistlistcontainer.h
  src/playlist/playlistlistmodel.h
  src/playlist/playlistlistview.h
  src/playlist/playlistlistsortfiltermodel.h
  src/playlist/playlistmanagerinterface.h
  src/playlist/playlistmanager.h
  src/playlist/playlistsaveoptionsdialog.h
  src/playlist/playlistsequence.h
  src/playlist/playlisttabbar.h
  src/playlist/playlistview.h
  src/playlist/playlistproxystyle.h
  src/playlist/playlistitemmimedata.h
  src/playlist/songloaderinserter.h
  src/playlist/dynamicplaylistcontrols.h

  src/queue/queue.h
  src/queue/queueview.h

  src/playlistparsers/asxiniparser.h
  src/playlistparsers/asxparser.h
  src/playlistparsers/cueparser.h
  src/playlistparsers/m3uparser.h
  src/playlistparsers/parserbase.h
  src/playlistparsers/playlistparser.h
  src/playlistparsers/plsparser.h
  src/playlistparsers/wplparser.h
  src/playlistparsers/xmlparser.h
  src/playlistparsers/xspfparser.h

  src/smartplaylists/playlistgenerator.h
  src/smartplaylists/playlistgeneratorinserter.h
  src/smartplaylists/playlistquerygenerator.h
  src/smartplaylists/playlistgeneratormimedata.h
  src/smartplaylists/smartplaylistquerywizardplugin.h
  src/smartplaylists/smartplaylistquerywizardpluginsortpage.h
  src/smartplaylists/smartplaylistquerywizardpluginsearchpage.h
  src/smartplaylists/smartplaylistsearchpreview.h
  src/smartplaylists/smartplaylistsearchtermwidget.h
  src/smartplaylists/smartplaylistsearchtermwidgetoverlay.h
  src/smartplaylists/smartplaylistsmodel.h
  src/smartplaylists/smartplaylistsviewcontainer.h
  src/smartplaylists/smartplaylistsview.h
  src/smartplaylists/smartplaylistwizard.h
  src/smartplaylists/smartplaylistwizardplugin.h
  src/smartplaylists/smartplaylistwizardtypepage.h
  src/smartplaylists/smartplaylistwizardfinishpage.h

  src/covermanager/albumcovermanager.h
  src/covermanager/albumcovermanagerlist.h
  src/covermanager/albumcoverloader.h
  src/covermanager/albumcoverfetcher.h
  src/covermanager/albumcoverfetchersearch.h
  src/covermanager/albumcoversearcher.h
  src/covermanager/albumcoverexport.h
  src/covermanager/albumcoverexporter.h
  src/covermanager/albumcoverchoicecontroller.h
  src/covermanager/coverprovider.h
  src/covermanager/coverproviders.h
  src/covermanager/coversearchstatisticsdialog.h
  src/covermanager/coverexportrunnable.h
  src/covermanager/currentalbumcoverloader.h
  src/covermanager/coverfromurldialog.h
  src/covermanager/jsoncoverprovider.h
  src/covermanager/lastfmcoverprovider.h
  src/covermanager/musicbrainzcoverprovider.h
  src/covermanager/discogscoverprovider.h
  src/covermanager/deezercoverprovider.h
  src/covermanager/musixmatchcoverprovider.h
  src/covermanager/opentidalcoverprovider.h

  src/lyrics/lyricsproviders.h
  src/lyrics/lyricsprovider.h
  src/lyrics/lyricsfetcher.h
  src/lyrics/lyricsfetchersearch.h
  src/lyrics/jsonlyricsprovider.h
  src/lyrics/htmllyricsprovider.h
  src/lyrics/ovhlyricsprovider.h
  src/lyrics/lololyricsprovider.h
  src/lyrics/geniuslyricsprovider.h
  src/lyrics/musixmatchlyricsprovider.h
  src/lyrics/chartlyricsprovider.h
  src/lyrics/songlyricscomlyricsprovider.h
  src/lyrics/azlyricscomlyricsprovider.h
  src/lyrics/elyricsnetlyricsprovider.h
  src/lyrics/letraslyricsprovider.h
  src/lyrics/lyricfindlyricsprovider.h

  src/settings/settingsdialog.h
  src/settings/settingspage.h
  src/settings/settingsitemdelegate.h
  src/settings/behavioursettingspage.h
  src/settings/collectionsettingspage.h
  src/settings/collectionsettingsdirectorymodel.h
  src/settings/backendsettingspage.h
  src/settings/playlistsettingspage.h
  src/settings/scrobblersettingspage.h
  src/settings/coverssettingspage.h
  src/settings/lyricssettingspage.h
  src/settings/networkproxysettingspage.h
  src/settings/appearancesettingspage.h
  src/settings/contextsettingspage.h
  src/settings/notificationssettingspage.h
  src/settings/transcodersettingspage.h

  src/dialogs/about.h
  src/dialogs/errordialog.h
  src/dialogs/console.h
  src/dialogs/edittagdialog.h
  src/dialogs/trackselectiondialog.h
  src/dialogs/addstreamdialog.h
  src/dialogs/userpassdialog.h
  src/dialogs/deleteconfirmationdialog.h
  src/dialogs/lastfmimportdialog.h
  src/dialogs/messagedialog.h
  src/dialogs/snapdialog.h
  src/dialogs/saveplaylistsdialog.h

  src/widgets/autoexpandingtreeview.h
  src/widgets/busyindicator.h
  src/widgets/clickablelabel.h
  src/widgets/fancytabwidget.h
  src/widgets/fancytabbar.h
  src/widgets/fancytabdata.h
  src/widgets/favoritewidget.h
  src/widgets/freespacebar.h
  src/widgets/groupediconview.h
  src/widgets/lineedit.h
  src/widgets/linetextedit.h
  src/widgets/multiloadingindicator.h
  src/widgets/playingwidget.h
  src/widgets/renametablineedit.h
  src/widgets/sliderslider.h
  src/widgets/prettyslider.h
  src/widgets/volumeslider.h
  src/widgets/stickyslider.h
  src/widgets/stretchheaderview.h
  src/widgets/trackslider.h
  src/widgets/tracksliderpopup.h
  src/widgets/tracksliderslider.h
  src/widgets/loginstatewidget.h
  src/widgets/searchfield.h
  src/widgets/ratingwidget.h
  src/widgets/forcescrollperpixel.h
  src/widgets/resizabletextedit.h

  src/osd/osdbase.h
  src/osd/osdpretty.h

  src/streaming/streamingservices.h
  src/streaming/streamingservice.h
  src/streaming/streamsongmimedata.h
  src/streaming/streamingsearchmodel.h
  src/streaming/streamingsearchsortmodel.h
  src/streaming/streamingsearchitemdelegate.h
  src/streaming/streamingsearchview.h
  src/streaming/streamingsongsview.h
  src/streaming/streamingtabsview.h
  src/streaming/streamingcollectionview.h
  src/streaming/streamingcollectionviewcontainer.h

  src/radios/radioservices.h
  src/radios/radiobackend.h
  src/radios/radiomodel.h
  src/radios/radioview.h
  src/radios/radioviewcontainer.h
  src/radios/radioservice.h
  src/radios/radiomimedata.h
  src/radios/somafmservice.h
  src/radios/radioparadiseservice.h

  src/scrobbler/audioscrobbler.h
  src/scrobbler/scrobblersettingsservice.h
  src/scrobbler/scrobblerservice.h
  src/scrobbler/scrobblercache.h
  src/scrobbler/scrobblingapi20.h
  src/scrobbler/lastfmscrobbler.h
  src/scrobbler/librefmscrobbler.h
  src/scrobbler/listenbrainzscrobbler.h
  src/scrobbler/lastfmimport.h

  src/organize/organize.h
  src/organize/organizeformatvalidator.h
  src/organize/organizesyntaxhighlighter.h
  src/organize/organizedialog.h
  src/organize/organizeerrordialog.h

  src/transcoder/transcoder.h
  src/transcoder/transcodedialog.h
  src/transcoder/transcoderoptionsdialog.h
  src/transcoder/transcoderoptionsinterface.h
  src/transcoder/transcoderoptionsflac.h
  src/transcoder/transcoderoptionswavpack.h
  src/transcoder/transcoderoptionsvorbis.h
  src/transcoder/transcoderoptionsopus.h
  src/transcoder/transcoderoptionsspeex.h
  src/transcoder/transcoderoptionsaac.h
  src/transcoder/transcoderoptionsasf.h
  src/transcoder/transcoderoptionsmp3.h

  src/fileview/fileview.h
  src/fileview/fileviewlist.h

  src/device/devicemanager.h
  src/device/devicelister.h
  src/device/devicedatabasebackend.h
  src/device/deviceproperties.h
  src/device/filesystemdevice.h
  src/device/connecteddevice.h
  src/device/devicestatefiltermodel.h
  src/device/deviceviewcontainer.h
  src/device/deviceview.h
)

set(UI

  src/core/mainwindow.ui

  src/collection/groupbydialog.ui
  src/collection/collectionfilterwidget.ui
  src/collection/collectionviewcontainer.ui
  src/collection/savedgroupingmanager.ui

  src/playlist/playlistcontainer.ui
  src/playlist/playlistlistcontainer.ui
  src/playlist/playlistsaveoptionsdialog.ui
  src/playlist/playlistsequence.ui
  src/playlist/dynamicplaylistcontrols.ui

  src/queue/queueview.ui

  src/smartplaylists/smartplaylistquerysearchpage.ui
  src/smartplaylists/smartplaylistquerysortpage.ui
  src/smartplaylists/smartplaylistsearchpreview.ui
  src/smartplaylists/smartplaylistsearchtermwidget.ui
  src/smartplaylists/smartplaylistsviewcontainer.ui
  src/smartplaylists/smartplaylistwizardfinishpage.ui

  src/covermanager/albumcoverexport.ui
  src/covermanager/albumcovermanager.ui
  src/covermanager/albumcoversearcher.ui
  src/covermanager/coversearchstatisticsdialog.ui
  src/covermanager/coverfromurldialog.ui

  src/settings/settingsdialog.ui
  src/settings/behavioursettingspage.ui
  src/settings/collectionsettingspage.ui
  src/settings/backendsettingspage.ui
  src/settings/contextsettingspage.ui
  src/settings/playlistsettingspage.ui
  src/settings/scrobblersettingspage.ui
  src/settings/coverssettingspage.ui
  src/settings/lyricssettingspage.ui
  src/settings/networkproxysettingspage.ui
  src/settings/appearancesettingspage.ui
  src/settings/notificationssettingspage.ui
  src/settings/transcodersettingspage.ui

  src/equalizer/equalizer.ui
  src/equalizer/equalizerslider.ui

  src/dialogs/about.ui
  src/dialogs/errordialog.ui
  src/dialogs/console.ui
  src/dialogs/edittagdialog.ui
  src/dialogs/trackselectiondialog.ui
  src/dialogs/addstreamdialog.ui
  src/dialogs/userpassdialog.ui
  src/dialogs/lastfmimportdialog.ui
  src/dialogs/messagedialog.ui
  src/dialogs/saveplaylistsdialog.ui

  src/widgets/trackslider.ui
  src/widgets/loginstatewidget.ui

  src/osd/osdpretty.ui

  src/streaming/streamingtabsview.ui
  src/streaming/streamingcollectionviewcontainer.ui
  src/streaming/streamingsearchview.ui

  src/radios/radioviewcontainer.ui

  src/organize/organizedialog.ui
  src/organize/organizeerrordialog.ui

  src/transcoder/transcodedialog.ui
  src/transcoder/transcodelogdialog.ui
  src/transcoder/transcoderoptionsaac.ui
  src/transcoder/transcoderoptionsdialog.ui
  src/transcoder/transcoderoptionsflac.ui
  src/transcoder/transcoderoptionswavpack.ui
  src/transcoder/transcoderoptionsvorbis.ui
  src/transcoder/transcoderoptionsopus.ui
  src/transcoder/transcoderoptionsspeex.ui
  src/transcoder/transcoderoptionsasf.ui
  src/transcoder/transcoderoptionsmp3.ui

  src/fileview/fileview.ui

  src/device/deviceproperties.ui
  src/device/deviceviewcontainer.ui
)

if(APPLE)
  optional_source(APPLE
    SOURCES
      src/utilities/macosutils.mm
      src/core/scoped_nsautorelease_pool.mm
      src/core/mac_startup.mm
      src/systemtrayicon/macsystemtrayicon.mm
      src/osd/osdmac.mm
      src/widgets/searchfield_mac.mm
      src/engine/macosdevicefinder.cpp
      src/device/macosdevicelister.mm
    HEADERS
      src/systemtrayicon/macsystemtrayicon.h
      src/osd/osdmac.h
      src/device/macosdevicelister.h
  )
  optional_source(HAVE_SPARKLE SOURCES src/core/sparkleupdater.mm HEADERS src/core/sparkleupdater.h)
else()
  list(APPEND SOURCES src/systemtrayicon/qtsystemtrayicon.cpp src/widgets/searchfield_qt.cpp src/widgets/searchfield_qt_private.cpp)
  list(APPEND HEADERS src/systemtrayicon/qtsystemtrayicon.h src/widgets/searchfield_qt_private.h)
endif()

optional_source(WIN32
  SOURCES
    src/utilities/scopedwchararray.cpp
    src/utilities/winutils.cpp
    src/engine/directsounddevicefinder.cpp
    src/engine/mmdevicefinder.cpp
    src/core/windows7thumbbar.cpp
  HEADERS
    src/core/windows7thumbbar.h
)

optional_source(HAVE_STREAMTAGREADER
  SOURCES src/tagreader/streamtagreader.cpp src/tagreader/tagreaderreadstreamrequest.cpp src/tagreader/tagreaderreadstreamreply.cpp
  HEADERS src/tagreader/tagreaderreadstreamreply.h
)

optional_source(HAVE_DISCORD_RPC
  SOURCES src/discord/richpresence.cpp
  HEADERS src/discord/richpresence.h
)

if(HAVE_GLOBALSHORTCUTS)

  optional_source(HAVE_GLOBALSHORTCUTS
    SOURCES src/globalshortcuts/globalshortcutsmanager.cpp src/globalshortcuts/globalshortcutsbackend.cpp src/globalshortcuts/globalshortcutgrabber.cpp src/settings/globalshortcutssettingspage.cpp
    HEADERS src/globalshortcuts/globalshortcutsmanager.h src/globalshortcuts/globalshortcutsbackend.h src/globalshortcuts/globalshortcutgrabber.h src/settings/globalshortcutssettingspage.h
    UI src/globalshortcuts/globalshortcutgrabber.ui src/settings/globalshortcutssettingspage.ui
  )

  if(HAVE_KGLOBALACCEL_GLOBALSHORTCUTS)
    optional_source(HAVE_KGLOBALACCEL_GLOBALSHORTCUTS
      SOURCES src/globalshortcuts/globalshortcutsbackend-kglobalaccel.cpp
      HEADERS src/globalshortcuts/globalshortcutsbackend-kglobalaccel.h
    )
    qt_add_dbus_interface(SOURCES src/globalshortcuts/org.kde.KGlobalAccel.xml kglobalaccel)
    qt_add_dbus_interface(SOURCES src/globalshortcuts/org.kde.KGlobalAccel.Component.xml kglobalaccelcomponent)
  endif()

  optional_source(HAVE_X11_GLOBALSHORTCUTS
    SOURCES src/globalshortcuts/globalshortcutsbackend-x11.cpp src/globalshortcuts/globalshortcut.cpp src/globalshortcuts/globalshortcut-x11.cpp
    HEADERS src/globalshortcuts/globalshortcutsbackend-x11.h src/globalshortcuts/globalshortcut.h
  )

  optional_source(APPLE
    SOURCES src/globalshortcuts/globalshortcutsbackend-macos.mm src/globalshortcuts/globalshortcutgrabber.mm
    HEADERS src/globalshortcuts/globalshortcutsbackend-macos.h src/globalshortcuts/globalshortcutgrabber.h
  )

  optional_source(WIN32
    SOURCES src/globalshortcuts/globalshortcutsbackend-win.cpp src/globalshortcuts/globalshortcut.cpp src/globalshortcuts/globalshortcut-win.cpp
    HEADERS src/globalshortcuts/globalshortcutsbackend-win.h src/globalshortcuts/globalshortcut.h
  )

endif()

optional_source(HAVE_ALSA SOURCES src/engine/alsadevicefinder.cpp src/engine/alsapcmdevicefinder.cpp)
optional_source(HAVE_PULSE SOURCES src/engine/pulsedevicefinder.cpp)
optional_source(MSVC SOURCES src/engine/uwpdevicefinder.cpp src/engine/asiodevicefinder.cpp)
optional_source(HAVE_CHROMAPRINT SOURCES src/engine/chromaprinter.cpp)

optional_source(HAVE_MUSICBRAINZ
  SOURCES
    src/musicbrainz/acoustidclient.cpp
    src/musicbrainz/musicbrainzclient.cpp
    src/musicbrainz/tagfetcher.cpp
  HEADERS
    src/musicbrainz/acoustidclient.h
    src/musicbrainz/musicbrainzclient.h
    src/musicbrainz/tagfetcher.h
)

optional_source(HAVE_EBUR128 SOURCES src/engine/ebur128analysis.cpp)

if(HAVE_DBUS)
  optional_source(HAVE_DBUS SOURCES src/osd/osddbus.cpp HEADERS src/osd/osddbus.h)
  qt_add_dbus_interface(SOURCES src/osd/org.freedesktop.Notifications.xml notification)
endif()

if(HAVE_MPRIS2)
  optional_source(HAVE_MPRIS2 SOURCES src/mpris2/mpris2.cpp HEADERS src/mpris2/mpris2.h)
  qt_add_dbus_adaptor(SOURCES src/mpris2/org.mpris.MediaPlayer2.xml src/mpris2/mpris2.h mpris::Mpris2 mpris2_root Mpris2Root)
  qt_add_dbus_adaptor(SOURCES src/mpris2/org.mpris.MediaPlayer2.Player.xml src/mpris2/mpris2.h mpris::Mpris2 mpris2_player Mpris2Player)
  qt_add_dbus_adaptor(SOURCES src/mpris2/org.mpris.MediaPlayer2.TrackList.xml src/mpris2/mpris2.h mpris::Mpris2 mpris2_tracklist Mpris2TrackList)
  qt_add_dbus_adaptor(SOURCES src/mpris2/org.mpris.MediaPlayer2.Playlists.xml src/mpris2/mpris2.h mpris::Mpris2 mpris2_playlists Mpris2Playlists)
endif()

optional_source(HAVE_MOODBAR
  SOURCES
    src/moodbar/moodbarbuilder.cpp
    src/moodbar/moodbarcontroller.cpp
    src/moodbar/moodbaritemdelegate.cpp
    src/moodbar/moodbarloader.cpp
    src/moodbar/moodbarpipeline.cpp
    src/moodbar/moodbarproxystyle.cpp
    src/moodbar/moodbarrenderer.cpp
    src/engine/gstfastspectrumplugin.cpp
    src/engine/gstfastspectrum.cpp
    src/settings/moodbarsettingspage.cpp
  HEADERS
    src/moodbar/moodbarcontroller.h
    src/moodbar/moodbaritemdelegate.h
    src/moodbar/moodbarloader.h
    src/moodbar/moodbarpipeline.h
    src/moodbar/moodbarproxystyle.h
    src/settings/moodbarsettingspage.h
  UI
    src/settings/moodbarsettingspage.ui
)

optional_source(HAVE_GIO SOURCES src/device/giolister.cpp HEADERS src/device/giolister.h)

if(HAVE_UDISKS2)
  optional_source(HAVE_UDISKS2 SOURCES src/device/udisks2lister.cpp HEADERS src/device/udisks2lister.h)
  set_source_files_properties(src/device/org.freedesktop.DBus.ObjectManager.xml PROPERTIES NO_NAMESPACE objectmanager INCLUDE includes/dbus_metatypes.h)
  set_source_files_properties(src/device/org.freedesktop.UDisks2.Filesystem.xml PROPERTIES NO_NAMESPACE udisks2filesystem INCLUDE includes/dbus_metatypes.h)
  set_source_files_properties(src/device/org.freedesktop.UDisks2.Block.xml PROPERTIES NO_NAMESPACE udisks2block INCLUDE includes/dbus_metatypes.h)
  set_source_files_properties(src/device/org.freedesktop.UDisks2.Drive.xml PROPERTIES NO_NAMESPACE udisks2drive INCLUDE includes/dbus_metatypes.h)
  set_source_files_properties(src/device/org.freedesktop.UDisks2.Job.xml PROPERTIES NO_NAMESPACE udisks2job INCLUDE includes/dbus_metatypes.h)
  qt_add_dbus_interface(SOURCES src/device/org.freedesktop.DBus.ObjectManager.xml objectmanager)
  qt_add_dbus_interface(SOURCES src/device/org.freedesktop.UDisks2.Filesystem.xml udisks2filesystem)
  qt_add_dbus_interface(SOURCES src/device/org.freedesktop.UDisks2.Block.xml udisks2block)
  qt_add_dbus_interface(SOURCES src/device/org.freedesktop.UDisks2.Drive.xml udisks2drive)
  qt_add_dbus_interface(SOURCES src/device/org.freedesktop.UDisks2.Job.xml udisks2job)
endif()

optional_source(HAVE_MTP
  SOURCES
    src/device/mtpconnection.cpp
    src/device/mtpdevice.cpp
    src/device/mtploader.cpp
  HEADERS
    src/device/mtpconnection.h
    src/device/mtpdevice.h
    src/device/mtploader.h
)

optional_source(HAVE_AUDIOCD
  SOURCES
    src/device/cddadevice.cpp
    src/device/cddalister.cpp
    src/device/cddasongloader.cpp
  HEADERS
    src/device/cddadevice.h
    src/device/cddalister.h
    src/device/cddasongloader.h
)

optional_source(HAVE_GPOD
  SOURCES
    src/device/gpoddevice.cpp
    src/device/gpodloader.cpp
  HEADERS
    src/device/gpoddevice.h
    src/device/gpodloader.h
)

optional_source(HAVE_SUBSONIC
  SOURCES
    src/subsonic/subsonicservice.cpp
    src/subsonic/subsonicurlhandler.cpp
    src/subsonic/subsonicbaserequest.cpp
    src/subsonic/subsonicrequest.cpp
    src/subsonic/subsonicscrobblerequest.cpp
    src/settings/subsonicsettingspage.cpp
    src/scrobbler/subsonicscrobbler.cpp
  HEADERS
    src/subsonic/subsonicservice.h
    src/subsonic/subsonicurlhandler.h
    src/subsonic/subsonicbaserequest.h
    src/subsonic/subsonicrequest.h
    src/subsonic/subsonicscrobblerequest.h
    src/settings/subsonicsettingspage.h
    src/scrobbler/subsonicscrobbler.h
  UI
    src/settings/subsonicsettingspage.ui
)

optional_source(HAVE_TIDAL
  SOURCES
    src/tidal/tidalservice.cpp
    src/tidal/tidalurlhandler.cpp
    src/tidal/tidalbaserequest.cpp
    src/tidal/tidalrequest.cpp
    src/tidal/tidalstreamurlrequest.cpp
    src/tidal/tidalfavoriterequest.cpp
    src/settings/tidalsettingspage.cpp
    src/covermanager/tidalcoverprovider.cpp
  HEADERS
    src/tidal/tidalservice.h
    src/tidal/tidalurlhandler.h
    src/tidal/tidalbaserequest.h
    src/tidal/tidalrequest.h
    src/tidal/tidalstreamurlrequest.h
    src/tidal/tidalfavoriterequest.h
    src/settings/tidalsettingspage.h
    src/covermanager/tidalcoverprovider.h
  UI
    src/settings/tidalsettingspage.ui
)

optional_source(HAVE_SPOTIFY
  SOURCES
    src/spotify/spotifyservice.cpp
    src/spotify/spotifybaserequest.cpp
    src/spotify/spotifyrequest.cpp
    src/spotify/spotifyfavoriterequest.cpp
    src/settings/spotifysettingspage.cpp
    src/covermanager/spotifycoverprovider.cpp
  HEADERS
    src/spotify/spotifyservice.h
    src/spotify/spotifybaserequest.h
    src/spotify/spotifyrequest.h
    src/spotify/spotifyfavoriterequest.h
    src/settings/spotifysettingspage.h
    src/covermanager/spotifycoverprovider.h
  UI
    src/settings/spotifysettingspage.ui
)

optional_source(HAVE_QOBUZ
  SOURCES
    src/qobuz/qobuzservice.cpp
    src/qobuz/qobuzurlhandler.cpp
    src/qobuz/qobuzbaserequest.cpp
    src/qobuz/qobuzrequest.cpp
    src/qobuz/qobuzstreamurlrequest.cpp
    src/qobuz/qobuzfavoriterequest.cpp
    src/settings/qobuzsettingspage.cpp
    src/covermanager/qobuzcoverprovider.cpp
  HEADERS
    src/qobuz/qobuzservice.h
    src/qobuz/qobuzurlhandler.h
    src/qobuz/qobuzbaserequest.h
    src/qobuz/qobuzrequest.h
    src/qobuz/qobuzstreamurlrequest.h
    src/qobuz/qobuzfavoriterequest.h
    src/settings/qobuzsettingspage.h
    src/covermanager/qobuzcoverprovider.h
  UI
    src/settings/qobuzsettingspage.ui
)

qt_wrap_cpp(SOURCES ${HEADERS})
qt_wrap_ui(SOURCES ${UI})
qt_add_resources(SOURCES data/data.qrc data/icons.qrc)

add_library(strawberry_lib STATIC ${SOURCES})

target_sources(strawberry PRIVATE src/main.cpp)

if(WIN32)
  target_sources(strawberry PRIVATE windres.rc)
endif()

add_subdirectory(src)
add_subdirectory(dist)

if(TARGET GTest::gtest AND TARGET GTest::gmock AND Qt${QT_VERSION_MAJOR}Test_FOUND)
  add_subdirectory(tests)
endif()

if(LINUX AND LSB_RELEASE_EXEC AND DPKG_BUILDPACKAGE)
  add_subdirectory(debian)
endif()

if(HAVE_DISCORD_RPC)
  add_subdirectory(3rdparty/discord-rpc)
  target_include_directories(strawberry_lib PUBLIC 3rdparty/discord-rpc)
endif()

if(HAVE_TRANSLATIONS)
  qt_add_lupdate(strawberry_lib TS_FILES "${CMAKE_SOURCE_DIR}/src/translations/strawberry_en_US.ts" OPTIONS -locations none -no-ui-lines -no-obsolete)
  file(GLOB_RECURSE ts_files ${CMAKE_SOURCE_DIR}/src/translations/*.ts)
  set_source_files_properties(${ts_files} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/data")
  qt_add_lrelease(strawberry TS_FILES ${ts_files} QM_FILES_OUTPUT_VARIABLE INSTALL_TRANSLATIONS_FILES)
  if(NOT INSTALL_TRANSLATIONS)
    qt_add_resources(strawberry "translations" PREFIX "/i18n" BASE "${CMAKE_CURRENT_BINARY_DIR}/data" FILES "${INSTALL_TRANSLATIONS_FILES}")
  endif()
endif()

target_include_directories(strawberry_lib PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
)

if(SINGLEAPPLICATION_INCLUDE_DIRS)
  target_include_directories(strawberry_lib SYSTEM PUBLIC ${SINGLEAPPLICATION_INCLUDE_DIRS})
endif()

target_link_libraries(strawberry_lib PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  $<$<BOOL:${HAVE_BACKTRACE}>:${Backtrace_LIBRARIES}>
  PkgConfig::GLIB
  PkgConfig::GOBJECT
  PkgConfig::SQLITE
  PkgConfig::GSTREAMER
  PkgConfig::GSTREAMER_BASE
  PkgConfig::GSTREAMER_AUDIO
  PkgConfig::GSTREAMER_APP
  PkgConfig::GSTREAMER_TAG
  PkgConfig::GSTREAMER_PBUTILS
  ${TAGLIB_LIBRARIES}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Sql
  $<$<BOOL:${HAVE_DBUS}>:Qt${QT_VERSION_MAJOR}::DBus>
  $<$<BOOL:${HAVE_QPA_QPLATFORMNATIVEINTERFACE}>:Qt${QT_VERSION_MAJOR}::GuiPrivate>
  ICU::uc
  ICU::i18n
  $<$<BOOL:${HAVE_STREAMTAGREADER}>:PkgConfig::LIBSPARSEHASH>
  $<$<BOOL:${HAVE_ALSA}>:ALSA::ALSA>
  $<$<BOOL:${HAVE_PULSE}>:PkgConfig::LIBPULSE>
  $<$<BOOL:${HAVE_CHROMAPRINT}>:PkgConfig::CHROMAPRINT>
  $<$<BOOL:${HAVE_MOODBAR}>:PkgConfig::FFTW3>
  $<$<BOOL:${HAVE_EBUR128}>:PkgConfig::LIBEBUR128>
  $<$<BOOL:${HAVE_X11_GLOBALSHORTCUTS}>:X11::X11_xcb>
  $<$<BOOL:${HAVE_GIO}>:PkgConfig::GIO>
  $<$<BOOL:${HAVE_GIO_UNIX}>:PkgConfig::GIO_UNIX>
  $<$<BOOL:${HAVE_AUDIOCD}>:PkgConfig::LIBCDIO>
  $<$<BOOL:${HAVE_MTP}>:PkgConfig::LIBMTP>
  $<$<BOOL:${HAVE_GPOD}>:PkgConfig::LIBGPOD PkgConfig::GDK_PIXBUF>
  $<$<BOOL:${HAVE_QTSPARKLE}>:qtsparkle-qt${QT_VERSION_MAJOR}::qtsparkle>
  $<$<BOOL:${WIN32}>:dsound dwmapi ${GETOPT_LIBRARIES}>
  $<$<BOOL:${MSVC}>:WindowsApp>
  KDAB::kdsingleapplication
  $<$<BOOL:${HAVE_DISCORD_RPC}>:discord-rpc>
)

if(APPLE)
  target_link_libraries(strawberry_lib PUBLIC
    "-framework Foundation"
    "-framework AppKit"
    "-framework Carbon"
    "-framework CoreAudio"
    "-framework DiskArbitration"
    "-framework IOKit"
    "-framework ScriptingBridge"
  )
  if(HAVE_SPARKLE)
    target_include_directories(strawberry_lib SYSTEM PRIVATE ${SPARKLE}/Headers)
    target_link_libraries(strawberry_lib PRIVATE ${SPARKLE})
  endif()
endif()

target_link_libraries(strawberry PUBLIC strawberry_lib)

if(NOT APPLE)
  install(TARGETS strawberry RUNTIME DESTINATION bin)
endif()

if(HAVE_TRANSLATIONS AND INSTALL_TRANSLATIONS AND INSTALL_TRANSLATIONS_FILES)
  install(FILES ${INSTALL_TRANSLATIONS_FILES} DESTINATION share/strawberry/translations)
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

optional_component_summary_show()

if(NOT CMAKE_CROSSCOMPILING AND NOT QT_SQLITE_TEST)
  message(WARNING "The Qt sqlite driver test failed.")
endif()
