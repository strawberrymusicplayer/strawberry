cmake_minimum_required(VERSION 3.0)

if(HAVE_TRANSLATIONS)
  include(../cmake/Translations.cmake)
endif(HAVE_TRANSLATIONS)

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
  core/threadsafenetworkdiskcache.cpp
  core/networktimeouts.cpp
  core/networkproxyfactory.cpp
  core/qtfslistener.cpp
  core/settingsprovider.cpp
  core/signalchecker.cpp
  core/song.cpp
  core/songloader.cpp
  core/stylehelper.cpp
  core/stylesheetloader.cpp
  core/tagreaderclient.cpp
  core/taskmanager.cpp
  core/thread.cpp
  core/urlhandler.cpp
  core/utilities.cpp
  core/iconloader.cpp
  core/qtsystemtrayicon.cpp
  core/standarditemiconloader.cpp
  core/systemtrayicon.cpp
  core/scopedtransaction.cpp
  core/translations.cpp

  engine/enginetype.cpp
  engine/enginebase.cpp
  engine/devicefinders.cpp
  engine/devicefinder.cpp

  analyzer/fht.cpp
  analyzer/analyzerbase.cpp
  analyzer/analyzercontainer.cpp
  analyzer/blockanalyzer.cpp
  analyzer/boomanalyzer.cpp
  analyzer/rainbowanalyzer.cpp

  equalizer/equalizer.cpp
  equalizer/equalizerslider.cpp

  context/contextview.cpp
  context/contextalbum.cpp
  context/contextalbumsmodel.cpp
  context/contextalbumsview.cpp

  collection/collection.cpp
  collection/collectionmodel.cpp
  collection/collectionbackend.cpp
  collection/collectionwatcher.cpp
  collection/collectionview.cpp
  collection/collectionitemdelegate.cpp
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
  playlist/songloaderinserter.cpp
  playlist/songplaylistitem.cpp
  playlist/dynamicplaylistcontrols.cpp

  queue/queue.cpp
  queue/queueview.cpp

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

  smartplaylists/playlistgenerator.cpp
  smartplaylists/playlistgeneratorinserter.cpp
  smartplaylists/playlistquerygenerator.cpp
  smartplaylists/smartplaylistquerywizardplugin.cpp
  smartplaylists/smartplaylistsearch.cpp
  smartplaylists/smartplaylistsearchpreview.cpp
  smartplaylists/smartplaylistsearchterm.cpp
  smartplaylists/smartplaylistsearchtermwidget.cpp
  smartplaylists/smartplaylistsmodel.cpp
  smartplaylists/smartplaylistsviewcontainer.cpp
  smartplaylists/smartplaylistsview.cpp
  smartplaylists/smartplaylistwizard.cpp
  smartplaylists/smartplaylistwizardplugin.cpp

  covermanager/albumcovermanager.cpp
  covermanager/albumcovermanagerlist.cpp
  covermanager/albumcoverloader.cpp
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
  covermanager/currentalbumcoverloader.cpp
  covermanager/coverfromurldialog.cpp
  covermanager/jsoncoverprovider.cpp
  covermanager/lastfmcoverprovider.cpp
  covermanager/musicbrainzcoverprovider.cpp
  covermanager/discogscoverprovider.cpp
  covermanager/deezercoverprovider.cpp
  covermanager/qobuzcoverprovider.cpp
  covermanager/musixmatchcoverprovider.cpp
  covermanager/spotifycoverprovider.cpp

  lyrics/lyricsproviders.cpp
  lyrics/lyricsprovider.cpp
  lyrics/lyricsfetcher.cpp
  lyrics/lyricsfetchersearch.cpp
  lyrics/jsonlyricsprovider.cpp
  lyrics/auddlyricsprovider.cpp
  lyrics/ovhlyricsprovider.cpp
  lyrics/lololyricsprovider.cpp
  lyrics/geniuslyricsprovider.cpp
  lyrics/musixmatchlyricsprovider.cpp
  lyrics/chartlyricsprovider.cpp

  settings/settingsdialog.cpp
  settings/settingspage.cpp
  settings/behavioursettingspage.cpp
  settings/collectionsettingspage.cpp
  settings/backendsettingspage.cpp
  settings/playlistsettingspage.cpp
  settings/scrobblersettingspage.cpp
  settings/coverssettingspage.cpp
  settings/lyricssettingspage.cpp
  settings/networkproxysettingspage.cpp
  settings/appearancesettingspage.cpp
  settings/contextsettingspage.cpp
  settings/notificationssettingspage.cpp

  dialogs/about.cpp
  dialogs/console.cpp
  dialogs/errordialog.cpp
  dialogs/edittagdialog.cpp
  dialogs/trackselectiondialog.cpp
  dialogs/addstreamdialog.cpp
  dialogs/userpassdialog.cpp
  dialogs/deleteconfirmationdialog.cpp
  dialogs/lastfmimportdialog.cpp
  dialogs/snapdialog.cpp

  widgets/autoexpandingtreeview.cpp
  widgets/busyindicator.cpp
  widgets/clickablelabel.cpp
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
  widgets/playingwidget.cpp
  widgets/renametablineedit.cpp
  widgets/volumeslider.cpp
  widgets/stickyslider.cpp
  widgets/stretchheaderview.cpp
  widgets/trackslider.cpp
  widgets/tracksliderpopup.cpp
  widgets/tracksliderslider.cpp
  widgets/loginstatewidget.cpp
  widgets/ratingwidget.cpp

  osd/osdbase.cpp
  osd/osdpretty.cpp

  musicbrainz/acoustidclient.cpp
  musicbrainz/musicbrainzclient.cpp

  internet/internetservices.cpp
  internet/internetservice.cpp
  internet/internetplaylistitem.cpp
  internet/internetsearchview.cpp
  internet/internetsearchmodel.cpp
  internet/internetsearchsortmodel.cpp
  internet/internetsearchitemdelegate.cpp
  internet/localredirectserver.cpp
  internet/internetsongsview.cpp
  internet/internettabsview.cpp
  internet/internetcollectionview.cpp
  internet/internetcollectionviewcontainer.cpp
  internet/internetsearchview.cpp

  scrobbler/audioscrobbler.cpp
  scrobbler/scrobblerservices.cpp
  scrobbler/scrobblerservice.cpp
  scrobbler/scrobblercache.cpp
  scrobbler/scrobblercacheitem.cpp
  scrobbler/scrobblingapi20.cpp
  scrobbler/lastfmscrobbler.cpp
  scrobbler/librefmscrobbler.cpp
  scrobbler/listenbrainzscrobbler.cpp
  scrobbler/lastfmimport.cpp

  organize/organize.cpp
  organize/organizeformat.cpp
  organize/organizedialog.cpp
  organize/organizeerrordialog.cpp

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
  core/threadsafenetworkdiskcache.h
  core/networktimeouts.h
  core/qtfslistener.h
  core/songloader.h
  core/tagreaderclient.h
  core/taskmanager.h
  core/urlhandler.h
  core/qtsystemtrayicon.h
  core/standarditemiconloader.h
  core/systemtrayicon.h
  core/mimedata.h
  core/stylesheetloader.h

  engine/enginebase.h
  engine/devicefinders.h

  analyzer/analyzerbase.h
  analyzer/analyzercontainer.h
  analyzer/blockanalyzer.h
  analyzer/boomanalyzer.h
  analyzer/rainbowanalyzer.h

  equalizer/equalizer.h
  equalizer/equalizerslider.h

  context/contextview.h
  context/contextalbum.h
  context/contextalbumsmodel.h
  context/contextalbumsview.h

  collection/collection.h
  collection/collectionmodel.h
  collection/collectionbackend.h
  collection/collectionwatcher.h
  collection/collectionview.h
  collection/collectionitemdelegate.h
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
  playlist/songloaderinserter.h
  playlist/songmimedata.h
  playlist/dynamicplaylistcontrols.h

  queue/queue.h
  queue/queueview.h

  playlistparsers/asxiniparser.h
  playlistparsers/asxparser.h
  playlistparsers/cueparser.h
  playlistparsers/m3uparser.h
  playlistparsers/parserbase.h
  playlistparsers/playlistparser.h
  playlistparsers/plsparser.h
  playlistparsers/xspfparser.h

  smartplaylists/playlistgenerator.h
  smartplaylists/playlistgeneratorinserter.h
  smartplaylists/playlistgeneratormimedata.h
  smartplaylists/smartplaylistquerywizardplugin.h
  smartplaylists/smartplaylistsearchpreview.h
  smartplaylists/smartplaylistsearchtermwidget.h
  smartplaylists/smartplaylistsmodel.h
  smartplaylists/smartplaylistsviewcontainer.h
  smartplaylists/smartplaylistsview.h
  smartplaylists/smartplaylistwizard.h
  smartplaylists/smartplaylistwizardplugin.h

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
  covermanager/currentalbumcoverloader.h
  covermanager/coverfromurldialog.h
  covermanager/jsoncoverprovider.h
  covermanager/lastfmcoverprovider.h
  covermanager/musicbrainzcoverprovider.h
  covermanager/discogscoverprovider.h
  covermanager/deezercoverprovider.h
  covermanager/qobuzcoverprovider.h
  covermanager/musixmatchcoverprovider.h
  covermanager/spotifycoverprovider.h

  lyrics/lyricsproviders.h
  lyrics/lyricsprovider.h
  lyrics/lyricsfetcher.h
  lyrics/lyricsfetchersearch.h
  lyrics/jsonlyricsprovider.h
  lyrics/auddlyricsprovider.h
  lyrics/ovhlyricsprovider.h
  lyrics/lololyricsprovider.h
  lyrics/geniuslyricsprovider.h
  lyrics/musixmatchlyricsprovider.h
  lyrics/chartlyricsprovider.h

  settings/settingsdialog.h
  settings/settingspage.h
  settings/behavioursettingspage.h
  settings/collectionsettingspage.h
  settings/backendsettingspage.h
  settings/playlistsettingspage.h
  settings/scrobblersettingspage.h
  settings/coverssettingspage.h
  settings/lyricssettingspage.h
  settings/networkproxysettingspage.h
  settings/appearancesettingspage.h
  settings/contextsettingspage.h
  settings/notificationssettingspage.h

  dialogs/about.h
  dialogs/errordialog.h
  dialogs/console.h
  dialogs/edittagdialog.h
  dialogs/trackselectiondialog.h
  dialogs/addstreamdialog.h
  dialogs/userpassdialog.h
  dialogs/deleteconfirmationdialog.h
  dialogs/lastfmimportdialog.h
  dialogs/snapdialog.h

  widgets/autoexpandingtreeview.h
  widgets/busyindicator.h
  widgets/clickablelabel.h
  widgets/fancytabwidget.h
  widgets/favoritewidget.h
  widgets/fileview.h
  widgets/fileviewlist.h
  widgets/freespacebar.h
  widgets/groupediconview.h
  widgets/lineedit.h
  widgets/linetextedit.h
  widgets/multiloadingindicator.h
  widgets/playingwidget.h
  widgets/renametablineedit.h
  widgets/volumeslider.h
  widgets/stickyslider.h
  widgets/stretchheaderview.h
  widgets/trackslider.h
  widgets/tracksliderpopup.h
  widgets/tracksliderslider.h
  widgets/loginstatewidget.h
  widgets/qsearchfield.h
  widgets/ratingwidget.h

  osd/osdbase.h
  osd/osdpretty.h

  musicbrainz/acoustidclient.h
  musicbrainz/musicbrainzclient.h

  internet/internetservices.h
  internet/internetservice.h
  internet/internetsongmimedata.h
  internet/internetsearchview.h
  internet/internetsearchmodel.h
  internet/localredirectserver.h
  internet/internetsongsview.h
  internet/internettabsview.h
  internet/internetcollectionview.h
  internet/internetcollectionviewcontainer.h
  internet/internetsearchview.h

  scrobbler/audioscrobbler.h
  scrobbler/scrobblerservices.h
  scrobbler/scrobblerservice.h
  scrobbler/scrobblercache.h
  scrobbler/scrobblercacheitem.h
  scrobbler/scrobblingapi20.h
  scrobbler/lastfmscrobbler.h
  scrobbler/librefmscrobbler.h
  scrobbler/listenbrainzscrobbler.h
  scrobbler/lastfmimport.h

  organize/organize.h
  organize/organizedialog.h
  organize/organizeerrordialog.h

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
  playlist/dynamicplaylistcontrols.ui

  queue/queueview.ui

  smartplaylists/smartplaylistquerysearchpage.ui
  smartplaylists/smartplaylistquerysortpage.ui
  smartplaylists/smartplaylistsearchpreview.ui
  smartplaylists/smartplaylistsearchtermwidget.ui
  smartplaylists/smartplaylistsviewcontainer.ui
  smartplaylists/smartplaylistwizardfinishpage.ui

  covermanager/albumcoverexport.ui
  covermanager/albumcovermanager.ui
  covermanager/albumcoversearcher.ui
  covermanager/coversearchstatisticsdialog.ui
  covermanager/coverfromurldialog.ui

  settings/settingsdialog.ui
  settings/behavioursettingspage.ui
  settings/collectionsettingspage.ui
  settings/backendsettingspage.ui
  settings/contextsettingspage.ui
  settings/playlistsettingspage.ui
  settings/scrobblersettingspage.ui
  settings/coverssettingspage.ui
  settings/lyricssettingspage.ui
  settings/networkproxysettingspage.ui
  settings/appearancesettingspage.ui
  settings/notificationssettingspage.ui

  equalizer/equalizer.ui
  equalizer/equalizerslider.ui

  dialogs/about.ui
  dialogs/errordialog.ui
  dialogs/console.ui
  dialogs/edittagdialog.ui
  dialogs/trackselectiondialog.ui
  dialogs/addstreamdialog.ui
  dialogs/userpassdialog.ui
  dialogs/lastfmimportdialog.ui
  dialogs/snapdialog.ui

  widgets/trackslider.ui
  widgets/fileview.ui
  widgets/loginstatewidget.ui

  osd/osdpretty.ui

  internet/internettabsview.ui
  internet/internetcollectionviewcontainer.ui
  internet/internetsearchview.ui

  organize/organizedialog.ui
  organize/organizeerrordialog.ui

)

set(RESOURCES ../data/data.qrc ../data/icons.qrc)
set(OTHER_SOURCES)

option(USE_INSTALL_PREFIX "Look for data in CMAKE_INSTALL_PREFIX" ON)

if(NOT APPLE)
  set(NOT_APPLE ON)
  optional_source(NOT_APPLE SOURCES widgets/qsearchfield_nonmac.cpp)
endif()

if(HAVE_GLOBALSHORTCUTS)
  optional_source(HAVE_GLOBALSHORTCUTS
    SOURCES globalshortcuts/globalshortcuts.cpp globalshortcuts/globalshortcutbackend.cpp globalshortcuts/globalshortcutgrabber.cpp settings/shortcutssettingspage.cpp
    HEADERS globalshortcuts/globalshortcuts.h globalshortcuts/globalshortcutbackend.h globalshortcuts/globalshortcutgrabber.h settings/shortcutssettingspage.h
    UI globalshortcuts/globalshortcutgrabber.ui settings/shortcutssettingspage.ui
  )
  if(HAVE_X11EXTRAS OR WIN32)
    set(X11_OR_WIN ON)
  endif()
  optional_source(X11_OR_WIN
    SOURCES globalshortcuts/globalshortcutbackend-system.cpp globalshortcuts/globalshortcut.cpp
    HEADERS globalshortcuts/globalshortcutbackend-system.h globalshortcuts/globalshortcut.h
  )
  optional_source(HAVE_X11EXTRAS
    SOURCES globalshortcuts/globalshortcut-x11.cpp
  )
  optional_source(HAVE_DBUS
    SOURCES globalshortcuts/globalshortcutbackend-gsd.cpp globalshortcuts/globalshortcutbackend-kde.cpp
    HEADERS globalshortcuts/globalshortcutbackend-gsd.h globalshortcuts/globalshortcutbackend-kde.h
  )
  optional_source(WIN32
    SOURCES globalshortcuts/globalshortcut-win.cpp
  )
endif(HAVE_GLOBALSHORTCUTS)

# ALSA
optional_source(HAVE_ALSA
  SOURCES
    engine/alsadevicefinder.cpp
)

# DBUS
optional_source(HAVE_DBUS
 SOURCES
   osd/osddbus.cpp
 HEADERS
   osd/osddbus.h
)

# GStreamer
optional_source(HAVE_GSTREAMER
  SOURCES engine/gststartup.cpp engine/gstengine.cpp engine/gstenginepipeline.cpp engine/gstelementdeleter.cpp
  HEADERS engine/gststartup.h engine/gstengine.h engine/gstenginepipeline.h engine/gstelementdeleter.h
)

# VLC
optional_source(HAVE_VLC
  SOURCES engine/vlcengine.cpp
  HEADERS engine/vlcengine.h
)

# DBUS and MPRIS - Unix specific
if(UNIX AND HAVE_DBUS)
  file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dbus)

  optional_source(HAVE_DBUS
    SOURCES core/mpris.cpp core/mpris2.cpp
    HEADERS core/mpris.h core/mpris2.h
  )
  optional_source(HAVE_UDISKS2
    SOURCES device/udisks2lister.cpp
    HEADERS device/udisks2lister.h
  )

  if (BUILD_WITH_QT6)

    # MPRIS 2.0 DBUS interfaces
    qt6_add_dbus_adaptor(SOURCES
        dbus/org.mpris.MediaPlayer2.Player.xml
        core/mpris2.h mpris::Mpris2 core/mpris2_player Mpris2Player)
    qt6_add_dbus_adaptor(SOURCES
        dbus/org.mpris.MediaPlayer2.xml
        core/mpris2.h mpris::Mpris2 core/mpris2_root Mpris2Root)
    qt6_add_dbus_adaptor(SOURCES
        dbus/org.mpris.MediaPlayer2.TrackList.xml
        core/mpris2.h mpris::Mpris2 core/mpris2_tracklist Mpris2TrackList)

    # MPRIS 2.1 DBUS interfaces
    qt6_add_dbus_adaptor(SOURCES
        dbus/org.mpris.MediaPlayer2.Playlists.xml
        core/mpris2.h mpris::Mpris2 core/mpris2_playlists Mpris2Playlists)

    # org.freedesktop.Notifications DBUS interface
    qt6_add_dbus_interface(SOURCES
        dbus/org.freedesktop.Notifications.xml
        dbus/notification)

    # org.gnome.SettingsDaemon interface
    qt6_add_dbus_interface(SOURCES
        dbus/org.gnome.SettingsDaemon.MediaKeys.xml
        dbus/gnomesettingsdaemon)

    # org.kde.KGlobalAccel interface
    qt6_add_dbus_interface(SOURCES
        dbus/org.kde.KGlobalAccel.xml
        dbus/kglobalaccel)
    qt6_add_dbus_interface(SOURCES
        dbus/org.kde.KGlobalAccel.Component.xml
        dbus/kglobalaccelcomponent)

  else()

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

    # org.kde.KGlobalAccel interface
    qt5_add_dbus_interface(SOURCES
        dbus/org.kde.KGlobalAccel.xml
        dbus/kglobalaccel)
    qt5_add_dbus_interface(SOURCES
        dbus/org.kde.KGlobalAccel.Component.xml
        dbus/kglobalaccelcomponent)

  endif()

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
       if(BUILD_WITH_QT6)
         qt6_add_dbus_interface(SOURCES
             dbus/org.freedesktop.DBus.ObjectManager.xml
             dbus/objectmanager)
         qt6_add_dbus_interface(SOURCES
             dbus/org.freedesktop.UDisks2.Filesystem.xml
             dbus/udisks2filesystem)
         qt6_add_dbus_interface(SOURCES
             dbus/org.freedesktop.UDisks2.Block.xml
             dbus/udisks2block)
         qt6_add_dbus_interface(SOURCES
             dbus/org.freedesktop.UDisks2.Drive.xml
             dbus/udisks2drive)
         qt6_add_dbus_interface(SOURCES
             dbus/org.freedesktop.UDisks2.Job.xml
             dbus/udisks2job)
       else()
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
         endif()
   endif(HAVE_UDISKS2)

endif(UNIX AND HAVE_DBUS)

optional_source(UNIX
  SOURCES
    device/connecteddevice.cpp
    device/devicedatabasebackend.cpp
    device/devicelister.cpp
    device/devicemanager.cpp
    device/devicestatefiltermodel.cpp
    device/filesystemdevice.cpp
    device/deviceviewcontainer.cpp
    device/deviceview.cpp
    device/deviceproperties.cpp
    device/deviceinfo.cpp
  HEADERS
    device/connecteddevice.h
    device/devicedatabasebackend.h
    device/devicelister.h
    device/devicemanager.h
    device/devicestatefiltermodel.h
    device/filesystemdevice.h
    device/deviceviewcontainer.h
    device/deviceview.h
    device/deviceproperties.h
  UI
    device/deviceproperties.ui
    device/deviceviewcontainer.ui
)

# Libgpod device backend
optional_source(HAVE_LIBGPOD
  SOURCES
    device/gpoddevice.cpp
    device/gpodloader.cpp
  HEADERS
    device/gpoddevice.h
    device/gpodloader.h
)

# GIO device backend
optional_source(HAVE_GIO
  SOURCES device/giolister.cpp
  HEADERS device/giolister.h
)

# mtp device
optional_source(HAVE_LIBMTP
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
  SOURCES
    engine/pulsedevicefinder.cpp
)

# MusicBrainz and transcoder require GStreamer
optional_source(HAVE_GSTREAMER
SOURCES
  transcoder/transcoder.cpp
  transcoder/transcodedialog.cpp
  transcoder/transcoderoptionsdialog.cpp
  transcoder/transcoderoptionsflac.cpp
  transcoder/transcoderoptionswavpack.cpp
  transcoder/transcoderoptionsvorbis.cpp
  transcoder/transcoderoptionsopus.cpp
  transcoder/transcoderoptionsspeex.cpp
  transcoder/transcoderoptionsaac.cpp
  transcoder/transcoderoptionsasf.cpp
  transcoder/transcoderoptionsmp3.cpp
  settings/transcodersettingspage.cpp
HEADERS
  transcoder/transcoder.h
  transcoder/transcodedialog.h
  transcoder/transcoderoptionsdialog.h
  transcoder/transcoderoptionsmp3.h
  settings/transcodersettingspage.h
UI
  transcoder/transcodedialog.ui
  transcoder/transcodelogdialog.ui
  transcoder/transcoderoptionsaac.ui
  transcoder/transcoderoptionsdialog.ui
  transcoder/transcoderoptionsflac.ui
  transcoder/transcoderoptionswavpack.ui
  transcoder/transcoderoptionsvorbis.ui
  transcoder/transcoderoptionsopus.ui
  transcoder/transcoderoptionsspeex.ui
  transcoder/transcoderoptionsasf.ui
  transcoder/transcoderoptionsmp3.ui
  settings/transcodersettingspage.ui
)

# CDIO backend and device
if(HAVE_GSTREAMER)
  optional_source(HAVE_CHROMAPRINT
    SOURCES
      musicbrainz/chromaprinter.cpp
      musicbrainz/tagfetcher.cpp
    HEADERS
      musicbrainz/tagfetcher.h
  )
  optional_source(HAVE_AUDIOCD
    SOURCES
      device/cddadevice.cpp
      device/cddalister.cpp
      device/cddasongloader.cpp
    HEADERS
      device/cddadevice.h
      device/cddalister.h
      device/cddasongloader.h
  )
endif()

# Platform specific - macOS
optional_source(APPLE
  SOURCES
    core/mac_utilities.mm
    core/mac_startup.mm
    core/macsystemtrayicon.mm
    core/macfslistener.mm
    osd/osdmac.mm
    widgets/qsearchfield_mac.mm
    engine/macosdevicefinder.cpp
    globalshortcuts/globalshortcutbackend-macos.mm
    globalshortcuts/globalshortcutgrabber.mm
  HEADERS
    core/macsystemtrayicon.h
    core/macfslistener.h
    osd/osdmac.h
    globalshortcuts/globalshortcutbackend-macos.h
)

if (APPLE)
  optional_source(HAVE_LIBMTP
   SOURCES
      device/macosdevicelister.mm
    HEADERS
      device/macosdevicelister.h
  )
endif()

# Platform specific - Windows
optional_source(WIN32
  SOURCES
    engine/directsounddevicefinder.cpp
    engine/mmdevicefinder.cpp
    core/windows7thumbbar.cpp
  HEADERS
    core/windows7thumbbar.h
)

optional_source(HAVE_SUBSONIC
  SOURCES
    subsonic/subsonicservice.cpp
    subsonic/subsonicurlhandler.cpp
    subsonic/subsonicbaserequest.cpp
    subsonic/subsonicrequest.cpp
    subsonic/subsonicscrobblerequest.cpp
    settings/subsonicsettingspage.cpp
    scrobbler/subsonicscrobbler.cpp
  HEADERS
    subsonic/subsonicservice.h
    subsonic/subsonicurlhandler.h
    subsonic/subsonicbaserequest.h
    subsonic/subsonicrequest.h
    subsonic/subsonicscrobblerequest.h
    settings/subsonicsettingspage.h
    scrobbler/subsonicscrobbler.h
  UI
    settings/subsonicsettingspage.ui
)

optional_source(HAVE_TIDAL
  SOURCES
    tidal/tidalservice.cpp
    tidal/tidalurlhandler.cpp
    tidal/tidalbaserequest.cpp
    tidal/tidalrequest.cpp
    tidal/tidalstreamurlrequest.cpp
    tidal/tidalfavoriterequest.cpp
    settings/tidalsettingspage.cpp
    covermanager/tidalcoverprovider.cpp
  HEADERS
    tidal/tidalservice.h
    tidal/tidalurlhandler.h
    tidal/tidalbaserequest.h
    tidal/tidalrequest.h
    tidal/tidalstreamurlrequest.h
    tidal/tidalfavoriterequest.h
    settings/tidalsettingspage.h
    covermanager/tidalcoverprovider.h
  UI
    settings/tidalsettingspage.ui
)

optional_source(HAVE_QOBUZ
  SOURCES
    qobuz/qobuzservice.cpp
    qobuz/qobuzurlhandler.cpp
    qobuz/qobuzbaserequest.cpp
    qobuz/qobuzrequest.cpp
    qobuz/qobuzstreamurlrequest.cpp
    qobuz/qobuzfavoriterequest.cpp
    settings/qobuzsettingspage.cpp
  HEADERS
    qobuz/qobuzservice.h
    qobuz/qobuzurlhandler.h
    qobuz/qobuzbaserequest.h
    qobuz/qobuzrequest.h
    qobuz/qobuzstreamurlrequest.h
    qobuz/qobuzfavoriterequest.h
    settings/qobuzsettingspage.h
  UI
    settings/qobuzsettingspage.ui
)

# Moodbar
optional_source(HAVE_MOODBAR
  SOURCES
    moodbar/moodbarbuilder.cpp
    moodbar/moodbarcontroller.cpp
    moodbar/moodbaritemdelegate.cpp
    moodbar/moodbarloader.cpp
    moodbar/moodbarpipeline.cpp
    moodbar/moodbarproxystyle.cpp
    moodbar/moodbarrenderer.cpp
    settings/moodbarsettingspage.cpp
  HEADERS
    moodbar/moodbarcontroller.h
    moodbar/moodbaritemdelegate.h
    moodbar/moodbarloader.h
    moodbar/moodbarpipeline.h
    moodbar/moodbarproxystyle.h
    settings/moodbarsettingspage.h
  UI
    settings/moodbarsettingspage.ui
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

if(BUILD_WITH_QT6)
  qt6_wrap_cpp(MOC ${HEADERS})
  qt6_wrap_ui(UIC ${UI})
  qt6_add_resources(QRC ${RESOURCES})
else()
  qt5_wrap_cpp(MOC ${HEADERS})
  qt5_wrap_ui(UIC ${UI})
  qt5_add_resources(QRC ${RESOURCES})
endif()

if(HAVE_TRANSLATIONS)

  set(LINGUAS "All" CACHE STRING "A space-seperated list of translations to compile in to Strawberry, or \"None\".")
  if (LINGUAS STREQUAL "All")
    # build LANGUAGES from all existing .po files
    file(GLOB pofiles translations/*.po)
    foreach(pofile ${pofiles})
      get_filename_component(lang ${pofile} NAME_WE)
      list(APPEND LANGUAGES ${lang})
    endforeach(pofile)
  else (LINGUAS STREQUAL "All")
    if (NOT LINGUAS OR LINGUAS STREQUAL "None")
      set (LANGUAGES "")
    else (NOT LINGUAS OR LINGUAS STREQUAL "None")
      string(REGEX MATCHALL [a-zA-Z_@]+ LANGUAGES ${LINGUAS})
    endif (NOT LINGUAS OR LINGUAS STREQUAL "None")
  endif (LINGUAS STREQUAL "All")

  add_pot(POT
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/header
    ${CMAKE_CURRENT_SOURCE_DIR}/translations/translations.pot
    ${SOURCES}
    ${MOC}
    ${UIC}
    ${OTHER_SOURCES}
    ../data/html/oauthsuccess.html
  )
  add_po(PO strawberry_ LANGUAGES ${LANGUAGES} DIRECTORY translations)

endif(HAVE_TRANSLATIONS)

link_directories(
  ${Boost_LIBRARY_DIRS}
  ${GLIB_LIBRARY_DIRS}
  ${GOBJECT_LIBRARY_DIRS}
  ${GNUTLS_LIBRARY_DIRS}
  ${SQLITE_LIBRARY_DIRS}
  ${TAGLIB_LIBRARY_DIRS}
  ${SINGLEAPPLICATION_LIBRARY_DIRS}
  ${SINGLECOREAPPLICATION_LIBRARY_DIRS}
  ${QTSPARKLE_LIBRARY_DIRS}
  ${Iconv_LIBRARY_DIRS}
)

if(HAVE_ALSA)
  link_directories(${ALSA_LIBRARY_DIRS})
endif(HAVE_ALSA)

if(HAVE_LIBPULSE)
  link_directories(${LIBPULSE_LIBRARY_DIRS})
endif()

if(HAVE_GSTREAMER)
  link_directories(
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_BASE_LIBRARY_DIRS}
    ${GSTREAMER_APP_LIBRARY_DIRS}
    ${GSTREAMER_AUDIO_LIBRARY_DIRS}
    ${GSTREAMER_TAG_LIBRARY_DIRS}
    ${GSTREAMER_PBUTILS_LIBRARY_DIRS}
  )
endif(HAVE_GSTREAMER)

if(HAVE_VLC)
  link_directories(${LIBVLC_LIBRARY_DIRS})
endif()

if(HAVE_CHROMAPRINT)
  link_directories(${CHROMAPRINT_LIBRARY_DIRS})
endif(HAVE_CHROMAPRINT)

if(X11_FOUND)
  link_directories(${X11_LIBRARY_DIRS})
endif(X11_FOUND)

if(XCB_FOUND)
  link_directories(${XCB_LIBRARY_DIRS})
endif(XCB_FOUND)

if(HAVE_GIO)
  link_directories(${GIO_LIBRARY_DIRS})
endif(HAVE_GIO)

if(HAVE_AUDIOCD)
  link_directories(${LIBCDIO_LIBRARY_DIRS})
endif(HAVE_AUDIOCD)

if(HAVE_LIBGPOD)
  link_directories(${LIBGPOD_LIBRARY_DIRS})
  link_directories(${GDK_PIXBUF_LIBRARY_DIRS})
endif(HAVE_LIBGPOD)

if(HAVE_LIBMTP)
  link_directories(${LIBMTP_LIBRARY_DIRS})
endif(HAVE_LIBMTP)

add_library(strawberry_lib STATIC
  ${SOURCES}
  ${MOC}
  ${UIC}
  ${QRC}
  ${POT}
  ${PO}
  ${OTHER_UIC_SOURCES}
)

target_include_directories(strawberry_lib SYSTEM PUBLIC
  ${Boost_INCLUDE_DIRS}
  ${GLIB_INCLUDE_DIRS}
  ${GOBJECT_INCLUDE_DIRS}
  ${GNUTLS_INCLUDE_DIRS}
  ${SQLITE_INCLUDE_DIRS}
)

target_include_directories(strawberry_lib PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
  ${SINGLEAPPLICATION_INCLUDE_DIRS}
  ${SINGLECOREAPPLICATION_INCLUDE_DIRS}
  ${TAGLIB_INCLUDE_DIRS}
)

target_link_libraries(strawberry_lib PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${GNUTLS_LIBRARIES}
  ${SQLITE_LIBRARIES}
  ${TAGLIB_LIBRARIES}
  ${QT_LIBRARIES}
  ${SINGLEAPPLICATION_LIBRARIES}
  ${SINGLECOREAPPLICATION_LIBRARIES}
  ${QTSPARKLE_LIBRARIES}
  ${Iconv_LIBRARY}
  libstrawberry-common
  libstrawberry-tagreader
)

if(HAVE_ALSA)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${ALSA_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${ALSA_LIBRARIES})
endif(HAVE_ALSA)

if(HAVE_LIBPULSE)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBPULSE_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBPULSE_LIBRARIES})
endif()

if(HAVE_GSTREAMER)
  target_include_directories(strawberry_lib SYSTEM PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_BASE_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_AUDIO_INCLUDE_DIRS}
    ${GSTREAMER_TAG_INCLUDE_DIRS}
    ${GSTREAMER_PBUTILS_INCLUDE_DIRS}
  )
  target_link_libraries(strawberry_lib PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_TAG_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
  )
endif(HAVE_GSTREAMER)

if(HAVE_MOODBAR)
  target_link_libraries(strawberry_lib PRIVATE gstmoodbar)
endif()

if(HAVE_VLC)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBVLC_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBVLC_LIBRARIES})
endif()

if(HAVE_CHROMAPRINT)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${CHROMAPRINT_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${CHROMAPRINT_LIBRARIES})
endif(HAVE_CHROMAPRINT)

if(X11_FOUND)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(strawberry_lib PRIVATE ${X11_LIBRARIES})
endif(X11_FOUND)

if(XCB_FOUND)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${XCB_INCLUDE_DIR})
  target_link_libraries(strawberry_lib PRIVATE ${XCB_LIBRARIES})
endif(XCB_FOUND)

if(HAVE_GIO)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${GIO_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${GIO_LIBRARIES})
endif(HAVE_GIO)

if(HAVE_AUDIOCD)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBCDIO_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBCDIO_LIBRARIES})
endif(HAVE_AUDIOCD)

if(HAVE_LIBGPOD)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBGPOD_INCLUDE_DIRS} ${GDK_PIXBUF_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBGPOD_LIBRARIES} ${GDK_PIXBUF_LIBRARIES})
endif(HAVE_LIBGPOD)

if(HAVE_LIBMTP)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBMTP_INCLUDE_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBMTP_LIBRARIES})
endif(HAVE_LIBMTP)


if(APPLE)
  target_link_libraries(strawberry_lib PRIVATE
    "-framework AppKit"
    "-framework Carbon"
    "-framework CoreAudio"
    "-framework DiskArbitration"
    "-framework Foundation"
    "-framework IOKit"
    "-framework ScriptingBridge"
  )
  target_link_libraries(strawberry_lib PRIVATE ${SPMEDIAKEYTAP_LIBRARIES})
  if(HAVE_SPARKLE)
    target_include_directories(strawberry_lib SYSTEM PRIVATE ${SPARKLE}/Headers)
    target_link_libraries(strawberry_lib PRIVATE ${SPARKLE})
  endif(HAVE_SPARKLE)
endif(APPLE)

if(WIN32)
  target_link_libraries(strawberry_lib PRIVATE dsound)
endif(WIN32)

if(X11_FOUND)
  # Hack: the Gold linker pays attention to the order that libraries are specified on the link line.
  # -lX11 and -ldl are provided earlier in the link command but they're actually used by libraries that appear after them, so they end up getting ignored.
  # This appends them to the very end of the link line, ensuring they're always used.
  if(FREEBSD)
    target_link_libraries(strawberry_lib PRIVATE ${X11_X11_LIB})
  else()
    target_link_libraries(strawberry_lib PRIVATE ${X11_X11_LIB} ${CMAKE_DL_LIBS})
  endif()
endif()

###############################################################################

set(EXECUTABLE_OUTPUT_PATH ..)

# Show the console window in debug mode on Windows
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_WIN32_CONSOLE)
  set(STRAWBERRY-WIN32-FLAG WIN32)
endif(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_WIN32_CONSOLE)

# Resource file for windows
if(WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../dist/windows/windres.rc.in ${CMAKE_CURRENT_BINARY_DIR}/windres.rc)
  set(STRAWBERRY-WIN32-RESOURCES windres.rc)
endif(WIN32)

add_executable(strawberry
  MACOSX_BUNDLE
  ${STRAWBERRY-WIN32-FLAG}
  ${STRAWBERRY-WIN32-RESOURCES}
  main.cpp
)

if(FREEBSD)
  target_link_libraries(strawberry PRIVATE execinfo)
endif()

target_link_libraries(strawberry PRIVATE strawberry_lib)

# macdeploy.py relies on the blob being built first.
add_dependencies(strawberry strawberry-tagreader)

if(NOT APPLE)
  install(TARGETS strawberry RUNTIME DESTINATION bin)
endif()

if(HAVE_TRANSLATIONS AND INSTALL_TRANSLATIONS AND INSTALL_TRANSLATIONS_FILES)
  install(FILES ${INSTALL_TRANSLATIONS_FILES} DESTINATION share/strawberry/translations)
endif()

if(APPLE)
  set_target_properties(strawberry PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../dist/macos/Info.plist")
endif (APPLE)
