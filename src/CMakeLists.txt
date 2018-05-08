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

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Woverloaded-virtual -Wno-sign-compare -Wno-deprecated-declarations -Wno-unused-local-typedefs -fpermissive --std=c++0x -U__STRICT_ANSI__")

option(BUILD_WERROR "Build with -Werror" ON)

if(BUILD_WERROR)
  if (LINUX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  endif (LINUX)
endif(BUILD_WERROR)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
  include_directories(../3rdparty/qtwin)
endif(WIN32)

# Activate fast QString concatenation
#if (QT_VERSION_MINOR GREATER 5)
  #if (QT_VERSION_MINOR GREATER 7)
    add_definitions(-DQT_USE_QSTRINGBUILDER)
  #else(QT_VERSION_MINOR GREATER 7)
    #add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
  #endif(QT_VERSION_MINOR GREATER 7)
#endif(QT_VERSION_MINOR GREATER 5)
add_definitions(-DQT_NO_URL_CAST_FROM_STRING)
add_definitions(-DBOOST_BIND_NO_PLACEHOLDERS)

if(ENABLE_IMOBILEDEVICE AND IMOBILEDEVICE_VERSION VERSION_GREATER 1.1.1)
  set(IMOBILEDEVICE_USES_UDIDS ON)
endif()

include_directories(${CMAKE_BINARY_DIR})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${LIBXML_INCLUDE_DIRS})
include_directories(${GOBJECT_INCLUDE_DIRS})
include_directories(${QTSINGLEAPPLICATION_INCLUDE_DIRS})
include_directories(${QXT_INCLUDE_DIRS})
include_directories(${SHA2_INCLUDE_DIRS})
include_directories(${CHROMAPRINT_INCLUDE_DIRS})

find_package(OpenGL)
include_directories(${OPENGL_INCLUDE_DIR})

if(HAVE_LIBLASTFM)
  include_directories(${LASTFM5_INCLUDE_DIRS})
endif(HAVE_LIBLASTFM)

include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-common)
include_directories(${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader)
include_directories(${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader)

include(../cmake/AddEngine.cmake)
include(../cmake/ParseArguments.cmake)

set(SOURCES
  core/mainwindow.cpp
  core/application.cpp
  core/appearance.cpp
  core/player.cpp
  core/commandlineoptions.cpp
  core/database.cpp
  core/metatypes.cpp
  core/deletefiles.cpp
  core/filesystemmusicstorage.cpp
  core/filesystemwatcherinterface.cpp
  core/mergedproxymodel.cpp
  core/multisortfilterproxy.cpp
  core/musicstorage.cpp
  core/network.cpp
  core/networkproxyfactory.cpp
  core/qtfslistener.cpp
  core/settingsprovider.cpp
  core/signalchecker.cpp
  core/song.cpp
  core/songloader.cpp
  core/stylesheetloader.cpp
  core/tagreaderclient.cpp
  core/taskmanager.cpp
  core/thread.cpp
  core/urlhandler.cpp
  core/utilities.cpp
  core/scangiomodulepath.cpp
  core/flowlayout.cpp
  core/iconloader.cpp
  core/qtsystemtrayicon.cpp
  core/standarditemiconloader.cpp
  core/systemtrayicon.cpp
  core/windows7thumbbar.cpp
  core/screensaver.cpp
  core/scopedtransaction.cpp
  
  engine/enginetype.cpp
  engine/enginebase.cpp
  engine/enginedevice.cpp
  engine/devicefinder.cpp
  
  analyzer/fht.cpp
  analyzer/analyzerbase.cpp
  analyzer/analyzercontainer.cpp
  analyzer/blockanalyzer.cpp
  
  equalizer/equalizer.cpp
  equalizer/equalizerslider.cpp

  collection/collection.cpp
  collection/collectionmodel.cpp
  collection/collectionbackend.cpp
  collection/collectionwatcher.cpp
  collection/collectionview.cpp
  collection/collectionviewcontainer.cpp
  collection/collectiondirectorymodel.cpp
  collection/collectionfilterwidget.cpp
  collection/collectionplaylistitem.cpp
  collection/collectionquery.cpp
  collection/sqlrow.cpp
  collection/savedgroupingmanager.cpp
  collection/groupbydialog.cpp

  playlist/playlist.cpp
  playlist/playlistbackend.cpp
  playlist/playlistcontainer.cpp
  playlist/playlistdelegates.cpp
  playlist/playlistfilter.cpp
  playlist/playlistfilterparser.cpp
  playlist/playlistheader.cpp
  playlist/playlistitem.cpp
  playlist/playlistlistcontainer.cpp
  playlist/playlistlistmodel.cpp
  playlist/playlistlistview.cpp
  playlist/playlistmanager.cpp
  playlist/playlistsaveoptionsdialog.cpp
  playlist/playlistsequence.cpp
  playlist/playlisttabbar.cpp
  playlist/playlistundocommands.cpp
  playlist/playlistview.cpp
  playlist/queue.cpp
  playlist/queuemanager.cpp
  playlist/songloaderinserter.cpp
  playlist/songplaylistitem.cpp

  playlistparsers/asxiniparser.cpp
  playlistparsers/asxparser.cpp
  playlistparsers/cueparser.cpp
  playlistparsers/m3uparser.cpp
  playlistparsers/parserbase.cpp
  playlistparsers/playlistparser.cpp
  playlistparsers/plsparser.cpp
  playlistparsers/wplparser.cpp
  playlistparsers/xmlparser.cpp
  playlistparsers/xspfparser.cpp
  
  covermanager/albumcovermanager.cpp
  covermanager/albumcovermanagerlist.cpp
  covermanager/albumcoverloader.cpp
  covermanager/albumcoverloaderoptions.cpp
  covermanager/albumcoverfetcher.cpp
  covermanager/albumcoverfetchersearch.cpp
  covermanager/albumcoversearcher.cpp
  covermanager/albumcoverexport.cpp
  covermanager/albumcoverexporter.cpp
  covermanager/albumcoverchoicecontroller.cpp
  covermanager/coverprovider.cpp
  covermanager/coverproviders.cpp
  covermanager/coversearchstatistics.cpp
  covermanager/coversearchstatisticsdialog.cpp
  covermanager/coverexportrunnable.cpp
  covermanager/currentartloader.cpp
  covermanager/coverfromurldialog.cpp
  covermanager/musicbrainzcoverprovider.cpp
  covermanager/amazoncoverprovider.cpp
  covermanager/discogscoverprovider.cpp
  
  settings/settingsdialog.cpp
  settings/settingspage.cpp
  settings/behavioursettingspage.cpp
  settings/collectionsettingspage.cpp
  settings/backendsettingspage.cpp
  settings/playbacksettingspage.cpp
  settings/playlistsettingspage.cpp
  settings/networkproxysettingspage.cpp
  settings/shortcutssettingspage.cpp
  settings/appearancesettingspage.cpp
  settings/notificationssettingspage.cpp

  dialogs/about.cpp
  dialogs/console.cpp
  dialogs/errordialog.cpp
  dialogs/edittagdialog.cpp
  dialogs/trackselectiondialog.cpp

  widgets/autoexpandingtreeview.cpp
  widgets/busyindicator.cpp
  widgets/clickablelabel.cpp
  widgets/didyoumean.cpp
  widgets/elidedlabel.cpp
  widgets/fancytabwidget.cpp
  widgets/favoritewidget.cpp
  widgets/fileview.cpp
  widgets/fileviewlist.cpp
  widgets/forcescrollperpixel.cpp
  widgets/freespacebar.cpp
  widgets/groupediconview.cpp
  widgets/lineedit.cpp
  widgets/linetextedit.cpp
  widgets/multiloadingindicator.cpp
  widgets/statusview.cpp
  widgets/playingwidget.cpp
  widgets/osd.cpp
  widgets/osdpretty.cpp
  widgets/prettyimage.cpp
  widgets/prettyimageview.cpp
  widgets/progressitemdelegate.cpp
  widgets/ratingwidget.cpp
  widgets/renametablineedit.cpp
  widgets/sliderwidget.cpp
  widgets/stickyslider.cpp
  widgets/stretchheaderview.cpp
  widgets/stylehelper.cpp
  widgets/trackslider.cpp
  widgets/tracksliderpopup.cpp
  widgets/tracksliderslider.cpp
  widgets/widgetfadehelper.cpp
  
  musicbrainz/acoustidclient.cpp
  musicbrainz/musicbrainzclient.cpp
  
  globalshortcuts/globalshortcutbackend.cpp
  globalshortcuts/globalshortcuts.cpp
  globalshortcuts/gnomeglobalshortcutbackend.cpp
  globalshortcuts/qxtglobalshortcutbackend.cpp
  globalshortcuts/globalshortcutgrabber.cpp
  
  device/connecteddevice.cpp
  device/devicedatabasebackend.cpp
  device/devicelister.cpp
  device/devicemanager.cpp
  device/deviceproperties.cpp
  device/devicestatefiltermodel.cpp
  device/deviceview.cpp
  device/deviceviewcontainer.cpp
  device/filesystemdevice.cpp

)

set(HEADERS
  core/mainwindow.h
  core/application.h
  core/player.h
  core/database.h
  core/deletefiles.h
  core/filesystemwatcherinterface.h
  core/mergedproxymodel.h
  core/network.h
  core/qtfslistener.h
  core/songloader.h
  core/tagreaderclient.h
  core/taskmanager.h
  core/urlhandler.h
  core/qtsystemtrayicon.h
  core/standarditemiconloader.h
  core/systemtrayicon.h
  core/windows7thumbbar.h
  core/mimedata.h

  engine/enginebase.h
  engine/enginedevice.h

  analyzer/analyzerbase.h
  analyzer/analyzercontainer.h
  analyzer/blockanalyzer.h
  
  equalizer/equalizer.h
  equalizer/equalizerslider.h

  collection/collection.h
  collection/collectionmodel.h
  collection/collectionbackend.h
  collection/collectionwatcher.h
  collection/collectionview.h
  collection/collectionviewcontainer.h
  collection/collectiondirectorymodel.h
  collection/collectionfilterwidget.h
  collection/savedgroupingmanager.h
  collection/groupbydialog.h

  playlist/playlist.h
  playlist/playlistbackend.h
  playlist/playlistcontainer.h
  playlist/playlistdelegates.h
  playlist/playlistfilter.h
  playlist/playlistheader.h
  playlist/playlistlistcontainer.h
  playlist/playlistlistmodel.h
  playlist/playlistlistview.h
  playlist/playlistmanager.h
  playlist/playlistsaveoptionsdialog.h
  playlist/playlistsequence.h
  playlist/playlisttabbar.h
  playlist/playlistview.h
  playlist/playlistitemmimedata.h
  playlist/queue.h
  playlist/queuemanager.h
  playlist/songloaderinserter.h
  playlist/songmimedata.h

  playlistparsers/asxiniparser.h
  playlistparsers/asxparser.h
  playlistparsers/cueparser.h
  playlistparsers/m3uparser.h
  playlistparsers/parserbase.h
  playlistparsers/playlistparser.h
  playlistparsers/plsparser.h
  playlistparsers/xspfparser.h
  
  covermanager/albumcovermanager.h
  covermanager/albumcovermanagerlist.h
  covermanager/albumcoverloader.h
  covermanager/albumcoverfetcher.h
  covermanager/albumcoverfetchersearch.h
  covermanager/albumcoversearcher.h
  covermanager/albumcoverexport.h
  covermanager/albumcoverexporter.h
  covermanager/albumcoverchoicecontroller.h
  covermanager/coverprovider.h
  covermanager/coverproviders.h
  covermanager/coversearchstatisticsdialog.h
  covermanager/coverexportrunnable.h
  covermanager/currentartloader.h
  covermanager/coverfromurldialog.h
  covermanager/amazoncoverprovider.h
  covermanager/musicbrainzcoverprovider.h
  covermanager/discogscoverprovider.h
  
  settings/settingsdialog.h
  settings/settingspage.h
  settings/behavioursettingspage.h
  settings/collectionsettingspage.h
  settings/backendsettingspage.h
  settings/playbacksettingspage.h
  settings/playlistsettingspage.h
  settings/networkproxysettingspage.h
  settings/shortcutssettingspage.h
  settings/appearancesettingspage.h
  settings/notificationssettingspage.h
  
  dialogs/about.h
  dialogs/errordialog.h
  dialogs/console.h
  dialogs/edittagdialog.h
  dialogs/trackselectiondialog.h

  widgets/autoexpandingtreeview.h
  widgets/busyindicator.h
  widgets/clickablelabel.h
  widgets/didyoumean.h
  widgets/elidedlabel.h
  widgets/fancytabwidget.h
  widgets/favoritewidget.h
  widgets/fileview.h
  widgets/fileviewlist.h
  widgets/freespacebar.h
  widgets/groupediconview.h
  widgets/lineedit.h
  widgets/linetextedit.h
  widgets/multiloadingindicator.h
  widgets/statusview.h
  widgets/playingwidget.h
  widgets/osd.h
  widgets/osdpretty.h
  widgets/prettyimage.h
  widgets/prettyimageview.h
  widgets/progressitemdelegate.h
  widgets/ratingwidget.h
  widgets/renametablineedit.h
  widgets/sliderwidget.h
  widgets/stickyslider.h
  widgets/stretchheaderview.h
  widgets/trackslider.h
  widgets/tracksliderpopup.h
  widgets/tracksliderslider.h
  widgets/widgetfadehelper.h
  
  musicbrainz/acoustidclient.h
  musicbrainz/musicbrainzclient.h
  
  globalshortcuts/globalshortcutbackend.h
  globalshortcuts/globalshortcuts.h
  globalshortcuts/gnomeglobalshortcutbackend.h
  globalshortcuts/globalshortcutgrabber.h
  
  device/connecteddevice.h
  device/devicedatabasebackend.h
  device/devicekitlister.h
  device/devicelister.h
  device/devicemanager.h
  device/deviceproperties.h
  device/devicestatefiltermodel.h
  device/deviceviewcontainer.h
  device/deviceview.h
  device/filesystemdevice.h

)

set(UI

  core/mainwindow.ui

  collection/groupbydialog.ui
  collection/collectionfilterwidget.ui
  collection/collectionviewcontainer.ui
  collection/savedgroupingmanager.ui

  playlist/playlistcontainer.ui
  playlist/playlistlistcontainer.ui
  playlist/playlistsaveoptionsdialog.ui
  playlist/playlistsequence.ui
  playlist/queuemanager.ui

  covermanager/albumcoverexport.ui
  covermanager/albumcovermanager.ui
  covermanager/albumcoversearcher.ui
  covermanager/coversearchstatisticsdialog.ui
  covermanager/coverfromurldialog.ui

  settings/settingsdialog.ui
  settings/behavioursettingspage.ui
  settings/collectionsettingspage.ui
  settings/backendsettingspage.ui
  settings/playbacksettingspage.ui
  settings/playlistsettingspage.ui
  settings/networkproxysettingspage.ui
  settings/shortcutssettingspage.ui
  settings/appearancesettingspage.ui
  settings/notificationssettingspage.ui

  equalizer/equalizer.ui
  equalizer/equalizerslider.ui

  dialogs/about.ui
  dialogs/errordialog.ui
  dialogs/console.ui
  dialogs/edittagdialog.ui
  dialogs/trackselectiondialog.ui

  widgets/trackslider.ui
  widgets/osdpretty.ui
  widgets/fileview.ui
  
  device/deviceproperties.ui
  device/deviceviewcontainer.ui
  
  globalshortcuts/globalshortcutgrabber.ui

)

set(RESOURCES ../data/data.qrc)
set(OTHER_SOURCES)

option(USE_INSTALL_PREFIX "Look for data in CMAKE_INSTALL_PREFIX" ON)

# Engines

set(GST_ENGINE_SRC engine/gstengine.cpp engine/gstenginepipeline.cpp engine/gstelementdeleter.cpp)
set(GST_ENGINE_MOC engine/gstengine.h engine/gstenginepipeline.h engine/gstelementdeleter.h)
set(GST_ENGINE_LIB GSTREAMER GSTREAMER_BASE GSTREAMER_APP GSTREAMER_AUDIO GSTREAMER_TAG GSTREAMER_PBUTILS)
#set(GST_ENGINE_LIB gstreamer-1.0 gstreamer-base-1.0 gstreamer-app-1.0 streamer-audio-1.0 gstreamer-tag-1.0 gstreamer-pbutils-1.0)
#set(GST_ENGINE_LIB ${GSTREAMER_BASE_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMER_APP_LIBRARIES} ${GSTREAMER_TAG_LIBRARIES} ${GSTREAMER_PBUTILS_LIBRARIES})

set(XINE_ENGINE_SRC engine/xineengine.cpp engine/xinescope.c)
set(XINE_ENGINE_MOC engine/xineengine.h)

set(VLC_ENGINE_SRC engine/vlcengine.cpp)
set(VLC_ENGINE_MOC engine/vlcengine.h)

set(PHONON_ENGINE_SRC engine/phononengine.cpp)
set(PHONON_ENGINE_MOC engine/phononengine.h)

add_engine(gstreamer GSTREAMER "${GST_ENGINE_LIB}" "${GST_ENGINE_SRC}" "${GST_ENGINE_MOC}" ON)
add_engine(xine XINE LIBXINE "${XINE_ENGINE_SRC}" "${XINE_ENGINE_MOC}" ON)
add_engine(vlc VLC LIBVLC "${VLC_ENGINE_SRC}" "${VLC_ENGINE_MOC}" ON)
add_engine(phonon PHONON PHONON "${PHONON_ENGINE_SRC}" "${PHONON_ENGINE_MOC}" OFF)

print_engines()

# Lastfm
optional_source(HAVE_LIBLASTFM
  SOURCES
    covermanager/lastfmcoverprovider.cpp
    covermanager/lastfmcompat.cpp
  HEADERS
    covermanager/lastfmcoverprovider.h
)

# Platform specific - Linux
optional_source(LINUX
  SOURCES
    engine/alsadevicefinder.cpp
)

# Platform specific - OS X
optional_source(APPLE
  SOURCES
    core/mac_startup.mm
    core/macsystemtrayicon.mm
    core/macscreensaver.cpp
    core/macfslistener.mm
    core/scoped_nsautorelease_pool.mm
    widgets/osd_mac.mm
    engine/osxdevicefinder.cpp
    device/macdevicelister.mm
    globalshortcuts/shortcutgrabber.mm
    globalshortcuts/macglobalshortcutbackend.mm
    globalshortcuts/globalshortcutgrabber.mm
  HEADERS
    core/mac_startup.h
    core/macsystemtrayicon.h
    core/macscreensaver.h
    core/macfslistener.h
    core/mac_utilities.h
    core/mac_delegate.h
    engine/osxdevicefinder.h
    device/macdevicelister.h
    globalshortcuts/macglobalshortcutbackend.h
)

if(APPLE)
  optional_source(HAVE_LIBMTP
   SOURCES
      device/macdevicelister.mm
    HEADERS
      device/macdevicelister.h
  )
endif()

# Platform specific - Windows
optional_source(WIN32
  SOURCES
    engine/directsounddevicefinder.cpp
    widgets/osd_win.cpp
)

# Platform specific - X11
optional_source(LINUX SOURCES widgets/osd_x11.cpp)

# DBUS and MPRIS - Linux specific
if(HAVE_DBUS)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dbus)

  # MPRIS DBUS interfaces
  #qt5_add_dbus_adaptor(SOURCES
  #    dbus/org.freedesktop.MediaPlayer.player.xml
  #    core/mpris1.h mpris::Mpris1Player core/mpris_player MprisPlayer)
  #qt5_add_dbus_adaptor(SOURCES
  #    dbus/org.freedesktop.MediaPlayer.root.xml
  #    core/mpris1.h mpris::Mpris1Root core/mpris_root MprisRoot)
  #qt5_add_dbus_adaptor(SOURCES
  #    dbus/org.freedesktop.MediaPlayer.tracklist.xml
  #    core/mpris1.h mpris::Mpris1TrackList core/mpris_tracklist MprisTrackList)

  # MPRIS 2.0 DBUS interfaces
  qt5_add_dbus_adaptor(SOURCES
      dbus/org.mpris.MediaPlayer2.Player.xml
      core/mpris2.h mpris::Mpris2 core/mpris2_player Mpris2Player)
  qt5_add_dbus_adaptor(SOURCES
      dbus/org.mpris.MediaPlayer2.xml
      core/mpris2.h mpris::Mpris2 core/mpris2_root Mpris2Root)
  qt5_add_dbus_adaptor(SOURCES
      dbus/org.mpris.MediaPlayer2.TrackList.xml
      core/mpris2.h mpris::Mpris2 core/mpris2_tracklist Mpris2TrackList)

  # MPRIS 2.1 DBUS interfaces
  qt5_add_dbus_adaptor(SOURCES
      dbus/org.mpris.MediaPlayer2.Playlists.xml
      core/mpris2.h mpris::Mpris2 core/mpris2_playlists Mpris2Playlists)

  # org.freedesktop.Notifications DBUS interface
  qt5_add_dbus_interface(SOURCES
      dbus/org.freedesktop.Notifications.xml
      dbus/notification)

  # org.gnome.SettingsDaemon interface
  qt5_add_dbus_interface(SOURCES
      dbus/org.gnome.SettingsDaemon.MediaKeys.xml
      dbus/gnomesettingsdaemon)

  # org.freedesktop.Avahi.Server interface
  add_custom_command(
      OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahiserver.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahiserver.h
      COMMAND ${QT_DBUSXML2CPP_EXECUTABLE}
      dbus/org.freedesktop.Avahi.Server.xml
      -p ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahiserver
      -i dbus/metatypes.h
      DEPENDS dbus/org.freedesktop.Avahi.Server.xml
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  list(APPEND HEADERS ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahiserver.h)
  list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahiserver.cpp)

  # org.freedesktop.Avahi.EntryGroup interface
  add_custom_command(
      OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahientrygroup.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahientrygroup.h
      COMMAND ${QT_DBUSXML2CPP_EXECUTABLE}
      dbus/org.freedesktop.Avahi.EntryGroup.xml
      -p ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahientrygroup
      -i dbus/metatypes.h
      DEPENDS dbus/org.freedesktop.Avahi.EntryGroup.xml
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  list(APPEND HEADERS ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahientrygroup.h)
  list(APPEND SOURCES ${CMAKE_CURRENT_BINARY_DIR}/dbus/avahientrygroup.cpp)

  # DeviceKit DBUS interfaces
  if(HAVE_DEVICEKIT)
    set_source_files_properties(dbus/org.freedesktop.UDisks.xml
        PROPERTIES NO_NAMESPACE dbus/udisks)
    set_source_files_properties(dbus/org.freedesktop.UDisks.Device.xml
        PROPERTIES NO_NAMESPACE dbus/udisksdevice)
    qt5_add_dbus_interface(SOURCES
        dbus/org.freedesktop.UDisks.xml
        dbus/udisks)
    qt5_add_dbus_interface(SOURCES
        dbus/org.freedesktop.UDisks.Device.xml
        dbus/udisksdevice)
  endif(HAVE_DEVICEKIT)

  if(HAVE_UDISKS2)
       set_source_files_properties(dbus/org.freedesktop.DBus.ObjectManager.xml
           PROPERTIES NO_NAMESPACE dbus/objectmanager INCLUDE dbus/metatypes.h)
       set_source_files_properties(dbus/org.freedesktop.UDisks2.Filesystem.xml
           PROPERTIES NO_NAMESPACE dbus/udisks2filesystem INCLUDE dbus/metatypes.h)
       set_source_files_properties(dbus/org.freedesktop.UDisks2.Block.xml
           PROPERTIES NO_NAMESPACE dbus/udisks2block INCLUDE dbus/metatypes.h)
       set_source_files_properties(dbus/org.freedesktop.UDisks2.Drive.xml
           PROPERTIES NO_NAMESPACE dbus/udisks2drive INCLUDE dbus/metatypes.h)
       set_source_files_properties(dbus/org.freedesktop.UDisks2.Job.xml
           PROPERTIES NO_NAMESPACE dbus/udisks2job INCLUDE dbus/metatypes.h)
       qt5_add_dbus_interface(SOURCES
           dbus/org.freedesktop.DBus.ObjectManager.xml
           dbus/objectmanager)
       qt5_add_dbus_interface(SOURCES
           dbus/org.freedesktop.UDisks2.Filesystem.xml
           dbus/udisks2filesystem)
       qt5_add_dbus_interface(SOURCES
           dbus/org.freedesktop.UDisks2.Block.xml
           dbus/udisks2block)
       qt5_add_dbus_interface(SOURCES
           dbus/org.freedesktop.UDisks2.Drive.xml
           dbus/udisks2drive)
       qt5_add_dbus_interface(SOURCES
           dbus/org.freedesktop.UDisks2.Job.xml
           dbus/udisks2job)
   endif(HAVE_UDISKS2)

endif(HAVE_DBUS)

optional_source(HAVE_DBUS
  SOURCES
    core/mpris.cpp
    core/mpris2.cpp
    core/dbusscreensaver.cpp
  HEADERS
    core/mpris.h
    core/mpris2.h
)

optional_source(HAVE_DEVICEKIT
  SOURCES device/devicekitlister.cpp
  HEADERS device/devicekitlister.h
)

optional_source(HAVE_UDISKS2
  SOURCES device/udisks2lister.cpp
  HEADERS device/udisks2lister.h
)

# Libgpod device backend
optional_source(HAVE_LIBGPOD
  INCLUDE_DIRECTORIES ${LIBGPOD_INCLUDE_DIRS}
  SOURCES
    device/gpoddevice.cpp
    device/gpodloader.cpp
  HEADERS
    device/gpoddevice.h
    device/gpodloader.h
)

# GIO device backend
optional_source(HAVE_GIO
  INCLUDE_DIRECTORIES ${GIO_INCLUDE_DIRS}
  SOURCES device/giolister.cpp
  HEADERS device/giolister.h
)

# libimobiledevice backend and device
optional_source(HAVE_IMOBILEDEVICE
  INCLUDE_DIRECTORIES
    ${IMOBILEDEVICE_INCLUDE_DIRS}
    ${PLIST_INCLUDE_DIRS}
    ${PLISTPP_INCLUDE_DIRS}
  SOURCES
    device/afcdevice.cpp
    device/afcfile.cpp
    device/afctransfer.cpp
    device/ilister.cpp
    device/imobiledeviceconnection.cpp
  HEADERS
    device/afcdevice.h
    device/afcfile.h
    device/afctransfer.h
    device/ilister.h
)

# mtp device
optional_source(HAVE_LIBMTP
  INCLUDE_DIRECTORIES ${LIBMTP_INCLUDE_DIRS}
  SOURCES
    device/mtpconnection.cpp
    device/mtpdevice.cpp
    device/mtploader.cpp
  HEADERS
    device/mtpdevice.h
    device/mtploader.h
)

# Pulse audio integration
optional_source(HAVE_LIBPULSE
  INCLUDE_DIRECTORIES
    ${LIBPULSE_INCLUDE_DIRS}
  SOURCES
    engine/pulsedevicefinder.cpp
)

optional_source(HAVE_GSTREAMER
SOURCES
  core/organise.cpp
  core/organiseformat.cpp
  settings/transcodersettingspage.cpp
  dialogs/organisedialog.cpp
  dialogs/organiseerrordialog.cpp
  musicbrainz/chromaprinter.cpp
  musicbrainz/tagfetcher.cpp
  transcoder/transcoder.cpp
  transcoder/transcodedialog.cpp
  transcoder/transcoderoptionsaac.cpp
  transcoder/transcoderoptionsdialog.cpp
  transcoder/transcoderoptionsflac.cpp
  transcoder/transcoderoptionsmp3.cpp
  transcoder/transcoderoptionsopus.cpp
  transcoder/transcoderoptionsspeex.cpp
  transcoder/transcoderoptionsvorbis.cpp
  transcoder/transcoderoptionswma.cpp
HEADERS
  core/organise.h
  settings/transcodersettingspage.h
  dialogs/organisedialog.h
  dialogs/organiseerrordialog.h
  musicbrainz/tagfetcher.h
  transcoder/transcoder.h
  transcoder/transcodedialog.h
  transcoder/transcoderoptionsdialog.h
  transcoder/transcoderoptionsmp3.h
UI
  settings/transcodersettingspage.ui
  dialogs/organisedialog.ui
  dialogs/organiseerrordialog.ui
  transcoder/transcodedialog.ui
  transcoder/transcodelogdialog.ui
  transcoder/transcoderoptionsaac.ui
  transcoder/transcoderoptionsdialog.ui
  transcoder/transcoderoptionsflac.ui
  transcoder/transcoderoptionsmp3.ui
  transcoder/transcoderoptionsopus.ui
  transcoder/transcoderoptionsspeex.ui
  transcoder/transcoderoptionsvorbis.ui
  transcoder/transcoderoptionswma.ui
)

# CDIO backend and device
if(HAVE_GSTREAMER)
optional_source(HAVE_AUDIOCD
  SOURCES
    device/cddadevice.cpp
    device/cddalister.cpp
    device/cddasongloader.cpp
  HEADERS
    device/cddadevice.h
    device/cddalister.h
    device/cddasongloader.h
  UI
)
endif(HAVE_GSTREAMER)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/version.h)

qt5_wrap_cpp(MOC ${HEADERS})
qt5_wrap_ui(UIC ${UI})
qt5_add_resources(QRC ${RESOURCES})

add_library(strawberry_lib STATIC
  ${SOURCES}
  ${MOC}
  ${UIC}
  ${QRC}
  ${POT}
  ${PO}
  ${OTHER_UIC_SOURCES}
)

target_link_libraries(strawberry_lib
  libstrawberry-common
  libstrawberry-tagreader
  #gstafc
  ${GLIB_LIBRARIES}
  ${GIO_LIBRARIES}
  ${SHA2_LIBRARIES}
  ${TAGLIB_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${QT_LIBRARIES}
  ${ENGINE_LIBRARIES}
  ${CHROMAPRINT_LIBRARIES}
  ${QTSINGLEAPPLICATION_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${SQLITE_LIBRARIES}
  z
  Qocoa
)

if(LINUX)
  target_link_libraries(strawberry_lib ${ALSA_LIBRARIES})
endif(LINUX)

if(HAVE_LIBLASTFM)
  target_link_libraries(strawberry_lib ${LASTFM5_LIBRARIES})
endif(HAVE_LIBLASTFM)

if(HAVE_LIBGPOD)
  target_link_libraries(strawberry_lib ${LIBGPOD_LIBRARIES})
endif(HAVE_LIBGPOD)

if(HAVE_GIO)
  target_link_libraries(strawberry_lib ${GIO_LIBRARIES})
endif(HAVE_GIO)

if(HAVE_AUDIOCD)
  target_link_libraries(strawberry_lib ${CDIO_LIBRARIES})
endif(HAVE_AUDIOCD)

if(HAVE_IMOBILEDEVICE)
  target_link_libraries(strawberry_lib
    ${IMOBILEDEVICE_LIBRARIES}
    ${PLIST_LIBRARIES}
    ${USBMUXD_LIBRARIES}
  )
  link_directories(${IMOBILEDEVICE_LIBRARY_DIRS})
  link_directories(${USBMUXD_LIBRARY_DIRS})
endif(HAVE_IMOBILEDEVICE)

if(HAVE_LIBMTP)
  target_link_libraries(strawberry_lib ${LIBMTP_LIBRARIES})
endif(HAVE_LIBMTP)

if(HAVE_LIBPULSE)
  target_link_libraries(strawberry_lib ${LIBPULSE_LIBRARIES})
endif()

if (APPLE)
  target_link_libraries(strawberry_lib
    "-framework AppKit"
    "-framework Carbon"
    "-framework CoreAudio"
    "-framework DiskArbitration"
    "-framework Foundation"
    "-framework IOKit"
    "-framework ScriptingBridge"
  )
  target_link_libraries(strawberry_lib ${SPMEDIAKEYTAP_LIBRARIES})
  if (HAVE_SPARKLE)
    include_directories(${SPARKLE}/Headers)
    target_link_libraries(strawberry_lib ${SPARKLE})
  endif (HAVE_SPARKLE)
else (APPLE)
  target_link_libraries(strawberry_lib ${QXT_LIBRARIES})
endif (APPLE)

if (WIN32)
  target_link_libraries(strawberry_lib
    ${ZLIB_LIBRARIES}
    qtwin
    dsound
    ${QT_QTGUI_LIBRARY}
  )
endif (WIN32)

if (UNIX AND NOT APPLE)
  # Hack: the Gold linker pays attention to the order that libraries are
  # specified on the link line.  -lX11 and -ldl are provided earlier in the link
  # command but they're actually used by libraries that appear after them, so
  # they end up getting ignored.  This appends them to the very end of the link
  # line, ensuring they're always used.
  find_package(X11)
  if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    target_link_libraries(strawberry_lib ${X11_X11_LIB})
  else ()
    target_link_libraries(strawberry_lib ${X11_X11_LIB} ${CMAKE_DL_LIBS})
  endif ()
endif ()

add_dependencies(strawberry_lib qtsingleapplication)


###############################################################################

set(EXECUTABLE_OUTPUT_PATH ..)

# Show the console window in debug mode on Windows
if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_WIN32_CONSOLE)
  set(STRAWBERRY-WIN32-FLAG WIN32)
endif (NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_WIN32_CONSOLE)

# Resource file for windows
if(WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../dist/windres.rc.in ${CMAKE_CURRENT_BINARY_DIR}/windres.rc)
  set(STRAWBERRY-WIN32-RESOURCES windres.rc)
endif(WIN32)

add_executable(strawberry
  MACOSX_BUNDLE
  ${STRAWBERRY-WIN32-FLAG}
  ${STRAWBERRY-WIN32-RESOURCES}
  core/main.cpp
)

if (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
  target_link_libraries(strawberry execinfo)
endif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")

target_link_libraries(strawberry
  strawberry_lib
)

# macdeploy.py relies on the blob being built first.
add_dependencies(strawberry strawberry-tagreader)

set_target_properties(strawberry PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "../dist/Info.plist"
)

if (APPLE)
  install(FILES ../dist/strawberry.icns
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources")
  install(FILES ../dist/qt.conf
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources")
  install(FILES ../dist/sparkle_pub.pem
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources")

  install(DIRECTORY "${QT_QTGUI_LIBRARY_RELEASE}/Versions/Current/Resources/"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Resources")

  if (HAVE_SPARKLE)
    install(DIRECTORY "${SPARKLE}/Versions/Current/Resources"
      DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/Sparkle.framework")
  endif (HAVE_SPARKLE)

  install(FILES "${QT_QTCORE_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtCore.framework/Versions/4/Resources")
  install(FILES "${QT_QTGUI_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtGui.framework/Versions/4/Resources")
  install(FILES "${QT_QTNETWORK_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtNetwork.framework/Versions/4/Resources")
  install(FILES "${QT_QTOPENGL_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtOpenGL.framework/Versions/4/Resources")
  install(FILES "${QT_QTSQL_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtSql.framework/Versions/4/Resources")
  install(FILES "${QT_QTSVG_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtSvg.framework/Versions/4/Resources")
  install(FILES "${QT_QTXML_LIBRARY_RELEASE}/Contents/Info.plist"
    DESTINATION "${CMAKE_BINARY_DIR}/strawberry.app/Contents/Frameworks/QtXml.framework/Versions/4/Resources")

  add_custom_command(TARGET strawberry
    POST_BUILD
    COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/../dist/macdeploy.py ${PROJECT_BINARY_DIR}/strawberry.app -f
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )

  if (APPLE_DEVELOPER_ID)
    add_custom_target(
      sign
      COMMAND
      ${PROJECT_SOURCE_DIR}/dist/codesign.py ${APPLE_DEVELOPER_ID} ${PROJECT_BINARY_DIR}/strawberry.app
      DEPENDS strawberry
      VERBATIM
    )
  endif()


  add_custom_command(
    OUTPUT ${PROJECT_BINARY_DIR}/strawberry-${STRAWBERRY_VERSION_PACKAGE}.dmg
    ${CMAKE_COMMAND} -E remove -f ${PROJECT_BINARY_DIR}/strawberry-${STRAWBERRY_VERSION_PACKAGE}.dmg
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../dist/create-dmg.sh ${PROJECT_BINARY_DIR}/strawberry.app
    COMMAND ${CMAKE_COMMAND} -E rename
      ${PROJECT_BINARY_DIR}/strawberry.dmg
      ${PROJECT_BINARY_DIR}/strawberry-${STRAWBERRY_VERSION_PACKAGE}.dmg
    DEPENDS strawberry
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  )
  add_custom_target(dmg
    DEPENDS ${PROJECT_BINARY_DIR}/strawberry-${STRAWBERRY_VERSION_PACKAGE}.dmg)
else (APPLE)
  install(TARGETS strawberry
    RUNTIME DESTINATION bin
  )
endif (APPLE)
