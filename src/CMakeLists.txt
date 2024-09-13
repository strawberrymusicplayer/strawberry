cmake_minimum_required(VERSION 3.13)

if(HAVE_TRANSLATIONS)
  include(../cmake/Translations.cmake)
endif()

set(SOURCES
  core/mainwindow.cpp
  core/application.cpp
  core/player.cpp
  core/commandlineoptions.cpp
  core/database.cpp
  core/sqlquery.cpp
  core/sqlrow.cpp
  core/metatypes.cpp
  core/deletefiles.cpp
  core/filesystemmusicstorage.cpp
  core/filesystemwatcherinterface.cpp
  core/mergedproxymodel.cpp
  core/multisortfilterproxy.cpp
  core/musicstorage.cpp
  core/networkaccessmanager.cpp
  core/threadsafenetworkdiskcache.cpp
  core/networktimeouts.cpp
  core/networkproxyfactory.cpp
  core/qtfslistener.cpp
  core/settings.cpp
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
  core/iconloader.cpp
  core/standarditemiconloader.cpp
  core/scopedtransaction.cpp
  core/translations.cpp
  core/systemtrayicon.cpp
  core/localredirectserver.cpp
  core/mimedata.cpp
  core/potranslator.cpp
  core/temporaryfile.cpp
  utilities/strutils.cpp
  utilities/envutils.cpp
  utilities/colorutils.cpp
  utilities/cryptutils.cpp
  utilities/fileutils.cpp
  utilities/diskutils.cpp
  utilities/imageutils.cpp
  utilities/macaddrutils.cpp
  utilities/mimeutils.cpp
  utilities/randutils.cpp
  utilities/threadutils.cpp
  utilities/timeutils.cpp
  utilities/transliterate.cpp
  utilities/xmlutils.cpp
  utilities/filemanagerutils.cpp
  utilities/coverutils.cpp
  utilities/screenutils.cpp
  utilities/textencodingutils.cpp

  filterparser/filterparser.cpp
  filterparser/filtertree.cpp

  engine/enginebase.cpp
  engine/enginedevice.cpp
  engine/devicefinders.cpp
  engine/devicefinder.cpp
  engine/enginemetadata.cpp

  analyzer/fht.cpp
  analyzer/analyzerbase.cpp
  analyzer/analyzercontainer.cpp
  analyzer/blockanalyzer.cpp
  analyzer/boomanalyzer.cpp
  analyzer/turbineanalyzer.cpp
  analyzer/sonogramanalyzer.cpp
  analyzer/waverubberanalyzer.cpp
  analyzer/rainbowanalyzer.cpp

  equalizer/equalizer.cpp
  equalizer/equalizerslider.cpp

  context/contextview.cpp
  context/contextalbum.cpp

  collection/collection.cpp
  collection/collectionmodel.cpp
  collection/collectionbackend.cpp
  collection/collectionwatcher.cpp
  collection/collectionview.cpp
  collection/collectionitemdelegate.cpp
  collection/collectionviewcontainer.cpp
  collection/collectiondirectorymodel.cpp
  collection/collectionfilteroptions.cpp
  collection/collectionfilterwidget.cpp
  collection/collectionfilter.cpp
  collection/collectionplaylistitem.cpp
  collection/collectionquery.cpp
  collection/savedgroupingmanager.cpp
  collection/groupbydialog.cpp
  collection/collectiontask.cpp
  collection/collectionmodelupdate.cpp

  playlist/playlist.cpp
  playlist/playlistbackend.cpp
  playlist/playlistcontainer.cpp
  playlist/playlistdelegates.cpp
  playlist/playlistfilter.cpp
  playlist/playlistheader.cpp
  playlist/playlistitem.cpp
  playlist/playlistitemmimedata.cpp
  playlist/playlistlistcontainer.cpp
  playlist/playlistlistmodel.cpp
  playlist/playlistlistsortfiltermodel.cpp
  playlist/playlistlistview.cpp
  playlist/playlistmanager.cpp
  playlist/playlistsaveoptionsdialog.cpp
  playlist/playlistsequence.cpp
  playlist/playlisttabbar.cpp
  playlist/playlistundocommands.cpp
  playlist/playlistview.cpp
  playlist/playlistproxystyle.cpp
  playlist/songmimedata.cpp
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
  smartplaylists/playlistgeneratormimedata.cpp
  smartplaylists/playlistquerygenerator.cpp
  smartplaylists/smartplaylistquerywizardplugin.cpp
  smartplaylists/smartplaylistquerywizardpluginsortpage.cpp
  smartplaylists/smartplaylistquerywizardpluginsearchpage.cpp
  smartplaylists/smartplaylistsearch.cpp
  smartplaylists/smartplaylistsearchpreview.cpp
  smartplaylists/smartplaylistsearchterm.cpp
  smartplaylists/smartplaylistsearchtermwidget.cpp
  smartplaylists/smartplaylistsearchtermwidgetoverlay.cpp
  smartplaylists/smartplaylistsmodel.cpp
  smartplaylists/smartplaylistsviewcontainer.cpp
  smartplaylists/smartplaylistsview.cpp
  smartplaylists/smartplaylistwizard.cpp
  smartplaylists/smartplaylistwizardplugin.cpp
  smartplaylists/smartplaylistwizardtypepage.cpp
  smartplaylists/smartplaylistwizardfinishpage.cpp

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
  covermanager/currentalbumcoverloader.cpp
  covermanager/coverfromurldialog.cpp
  covermanager/jsoncoverprovider.cpp
  covermanager/lastfmcoverprovider.cpp
  covermanager/musicbrainzcoverprovider.cpp
  covermanager/discogscoverprovider.cpp
  covermanager/deezercoverprovider.cpp
  covermanager/qobuzcoverprovider.cpp
  covermanager/musixmatchcoverprovider.cpp
  covermanager/opentidalcoverprovider.cpp

  lyrics/lyricsproviders.cpp
  lyrics/lyricsprovider.cpp
  lyrics/lyricssearchrequest.h
  lyrics/lyricssearchresult.h
  lyrics/lyricsfetcher.cpp
  lyrics/lyricsfetchersearch.cpp
  lyrics/jsonlyricsprovider.cpp
  lyrics/htmllyricsprovider.cpp
  lyrics/ovhlyricsprovider.cpp
  lyrics/lololyricsprovider.cpp
  lyrics/geniuslyricsprovider.cpp
  lyrics/musixmatchlyricsprovider.cpp
  lyrics/chartlyricsprovider.cpp
  lyrics/songlyricscomlyricsprovider.cpp
  lyrics/azlyricscomlyricsprovider.cpp
  lyrics/elyricsnetlyricsprovider.cpp
  lyrics/letraslyricsprovider.cpp
  lyrics/lyricfindlyricsprovider.cpp

  providers/musixmatchprovider.cpp

  settings/settingsdialog.cpp
  settings/settingspage.cpp
  settings/behavioursettingspage.cpp
  settings/collectionsettingspage.cpp
  settings/collectionsettingsdirectorymodel.cpp
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
  dialogs/messagedialog.cpp
  dialogs/snapdialog.cpp
  dialogs/saveplaylistsdialog.cpp

  widgets/autoexpandingtreeview.cpp
  widgets/busyindicator.cpp
  widgets/clickablelabel.cpp
  widgets/fancytabwidget.cpp
  widgets/fancytabbar.cpp
  widgets/fancytabdata.cpp
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
  widgets/sliderslider.cpp
  widgets/prettyslider.cpp
  widgets/volumeslider.cpp
  widgets/stickyslider.cpp
  widgets/stretchheaderview.cpp
  widgets/trackslider.cpp
  widgets/tracksliderpopup.cpp
  widgets/tracksliderslider.cpp
  widgets/loginstatewidget.cpp
  widgets/ratingwidget.cpp
  widgets/resizabletextedit.cpp

  osd/osdbase.cpp
  osd/osdpretty.cpp

  streaming/streamingservices.cpp
  streaming/streamingservice.cpp
  streaming/streamplaylistitem.cpp
  streaming/streamingsearchview.cpp
  streaming/streamingsearchmodel.cpp
  streaming/streamingsearchsortmodel.cpp
  streaming/streamingsearchitemdelegate.cpp
  streaming/streamingsongsview.cpp
  streaming/streamingtabsview.cpp
  streaming/streamingcollectionview.cpp
  streaming/streamingcollectionviewcontainer.cpp
  streaming/streamingsearchview.cpp
  streaming/streamsongmimedata.cpp

  radios/radioservices.cpp
  radios/radiobackend.cpp
  radios/radiomodel.cpp
  radios/radioview.cpp
  radios/radioviewcontainer.cpp
  radios/radioservice.cpp
  radios/radioplaylistitem.cpp
  radios/radiochannel.cpp
  radios/somafmservice.cpp
  radios/radioparadiseservice.cpp
  radios/radiomimedata.cpp

  scrobbler/audioscrobbler.cpp
  scrobbler/scrobblersettings.cpp
  scrobbler/scrobblerservice.cpp
  scrobbler/scrobblercache.cpp
  scrobbler/scrobblercacheitem.cpp
  scrobbler/scrobblemetadata.cpp
  scrobbler/scrobblingapi20.cpp
  scrobbler/lastfmscrobbler.cpp
  scrobbler/librefmscrobbler.cpp
  scrobbler/listenbrainzscrobbler.cpp
  scrobbler/lastfmimport.cpp

  organize/organize.cpp
  organize/organizeformat.cpp
  organize/organizeformatvalidator.cpp
  organize/organizesyntaxhighlighter.cpp
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
  core/multisortfilterproxy.h
  core/networkaccessmanager.h
  core/threadsafenetworkdiskcache.h
  core/networktimeouts.h
  core/qtfslistener.h
  core/settings.h
  core/songloader.h
  core/tagreaderclient.h
  core/taskmanager.h
  core/thread.h
  core/urlhandler.h
  core/standarditemiconloader.h
  core/translations.h
  core/potranslator.h
  core/mimedata.h
  core/stylesheetloader.h
  core/localredirectserver.h

  engine/enginebase.h
  engine/devicefinders.h

  analyzer/analyzerbase.h
  analyzer/analyzercontainer.h
  analyzer/blockanalyzer.h
  analyzer/boomanalyzer.h
  analyzer/turbineanalyzer.h
  analyzer/sonogramanalyzer.h
  analyzer/waverubberanalyzer.h
  analyzer/rainbowanalyzer.h

  equalizer/equalizer.h
  equalizer/equalizerslider.h

  context/contextview.h
  context/contextalbum.h

  collection/collection.h
  collection/collectionmodel.h
  collection/collectionbackend.h
  collection/collectionwatcher.h
  collection/collectionview.h
  collection/collectionitemdelegate.h
  collection/collectionviewcontainer.h
  collection/collectiondirectorymodel.h
  collection/collectionfilterwidget.h
  collection/collectionfilter.h
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
  playlist/playlistlistsortfiltermodel.h
  playlist/playlistmanager.h
  playlist/playlistsaveoptionsdialog.h
  playlist/playlistsequence.h
  playlist/playlisttabbar.h
  playlist/playlistview.h
  playlist/playlistproxystyle.h
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
  playlistparsers/wplparser.h
  playlistparsers/xmlparser.h
  playlistparsers/xspfparser.h

  smartplaylists/playlistgenerator.h
  smartplaylists/playlistgeneratorinserter.h
  smartplaylists/playlistquerygenerator.h
  smartplaylists/playlistgeneratormimedata.h
  smartplaylists/smartplaylistquerywizardplugin.h
  smartplaylists/smartplaylistquerywizardpluginsortpage.h
  smartplaylists/smartplaylistquerywizardpluginsearchpage.h
  smartplaylists/smartplaylistsearchpreview.h
  smartplaylists/smartplaylistsearchtermwidget.h
  smartplaylists/smartplaylistsearchtermwidgetoverlay.h
  smartplaylists/smartplaylistsmodel.h
  smartplaylists/smartplaylistsviewcontainer.h
  smartplaylists/smartplaylistsview.h
  smartplaylists/smartplaylistwizard.h
  smartplaylists/smartplaylistwizardplugin.h
  smartplaylists/smartplaylistwizardtypepage.h
  smartplaylists/smartplaylistwizardfinishpage.h

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
  covermanager/opentidalcoverprovider.h

  lyrics/lyricsproviders.h
  lyrics/lyricsprovider.h
  lyrics/lyricsfetcher.h
  lyrics/lyricsfetchersearch.h
  lyrics/jsonlyricsprovider.h
  lyrics/htmllyricsprovider.h
  lyrics/ovhlyricsprovider.h
  lyrics/lololyricsprovider.h
  lyrics/geniuslyricsprovider.h
  lyrics/musixmatchlyricsprovider.h
  lyrics/chartlyricsprovider.h
  lyrics/songlyricscomlyricsprovider.h
  lyrics/azlyricscomlyricsprovider.h
  lyrics/elyricsnetlyricsprovider.h
  lyrics/letraslyricsprovider.h
  lyrics/lyricfindlyricsprovider.h

  settings/settingsdialog.h
  settings/settingspage.h
  settings/behavioursettingspage.h
  settings/collectionsettingspage.h
  settings/collectionsettingsdirectorymodel.h
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
  dialogs/messagedialog.h
  dialogs/snapdialog.h
  dialogs/saveplaylistsdialog.h

  widgets/autoexpandingtreeview.h
  widgets/busyindicator.h
  widgets/clickablelabel.h
  widgets/fancytabwidget.h
  widgets/fancytabbar.h
  widgets/fancytabdata.h
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
  widgets/sliderslider.h
  widgets/prettyslider.h
  widgets/volumeslider.h
  widgets/stickyslider.h
  widgets/stretchheaderview.h
  widgets/trackslider.h
  widgets/tracksliderpopup.h
  widgets/tracksliderslider.h
  widgets/loginstatewidget.h
  widgets/searchfield.h
  widgets/ratingwidget.h
  widgets/forcescrollperpixel.h
  widgets/resizabletextedit.h

  osd/osdbase.h
  osd/osdpretty.h

  streaming/streamingservices.h
  streaming/streamingservice.h
  streaming/streamsongmimedata.h
  streaming/streamingsearchmodel.h
  streaming/streamingsearchsortmodel.h
  streaming/streamingsearchitemdelegate.h
  streaming/streamingsearchview.h
  streaming/streamingsongsview.h
  streaming/streamingtabsview.h
  streaming/streamingcollectionview.h
  streaming/streamingcollectionviewcontainer.h

  radios/radioservices.h
  radios/radiobackend.h
  radios/radiomodel.h
  radios/radioview.h
  radios/radioviewcontainer.h
  radios/radioservice.h
  radios/radiomimedata.h
  radios/somafmservice.h
  radios/radioparadiseservice.h

  scrobbler/audioscrobbler.h
  scrobbler/scrobblersettings.h
  scrobbler/scrobblerservice.h
  scrobbler/scrobblercache.h
  scrobbler/scrobblingapi20.h
  scrobbler/lastfmscrobbler.h
  scrobbler/librefmscrobbler.h
  scrobbler/listenbrainzscrobbler.h
  scrobbler/lastfmimport.h

  organize/organize.h
  organize/organizeformatvalidator.h
  organize/organizesyntaxhighlighter.h
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
  dialogs/messagedialog.ui
  dialogs/saveplaylistsdialog.ui

  widgets/trackslider.ui
  widgets/fileview.ui
  widgets/loginstatewidget.ui

  osd/osdpretty.ui

  streaming/streamingtabsview.ui
  streaming/streamingcollectionviewcontainer.ui
  streaming/streamingsearchview.ui

  radios/radioviewcontainer.ui

  organize/organizedialog.ui
  organize/organizeerrordialog.ui

)

set(RESOURCES ../data/data.qrc ../data/icons.qrc)

option(USE_INSTALL_PREFIX "Look for data in CMAKE_INSTALL_PREFIX" ON)

if(NOT APPLE)
  set(NOT_APPLE ON)
  optional_source(NOT_APPLE SOURCES widgets/searchfield_qt.cpp widgets/searchfield_qt_private.cpp core/qtsystemtrayicon.cpp HEADERS core/qtsystemtrayicon.h widgets/searchfield_qt_private.h)
endif()

if(HAVE_GLOBALSHORTCUTS)
  optional_source(HAVE_GLOBALSHORTCUTS
    SOURCES globalshortcuts/globalshortcutsmanager.cpp globalshortcuts/globalshortcutsbackend.cpp globalshortcuts/globalshortcutgrabber.cpp settings/globalshortcutssettingspage.cpp
    HEADERS globalshortcuts/globalshortcutsmanager.h globalshortcuts/globalshortcutsbackend.h globalshortcuts/globalshortcutgrabber.h settings/globalshortcutssettingspage.h
    UI globalshortcuts/globalshortcutgrabber.ui settings/globalshortcutssettingspage.ui
  )
  optional_source(HAVE_DBUS
    SOURCES globalshortcuts/globalshortcutsbackend-kde.cpp globalshortcuts/globalshortcutsbackend-gnome.cpp globalshortcuts/globalshortcutsbackend-mate.cpp
    HEADERS globalshortcuts/globalshortcutsbackend-kde.h globalshortcuts/globalshortcutsbackend-gnome.h globalshortcuts/globalshortcutsbackend-mate.h
  )
  optional_source(HAVE_X11_GLOBALSHORTCUTS
    SOURCES globalshortcuts/globalshortcutsbackend-x11.cpp globalshortcuts/globalshortcut.cpp globalshortcuts/globalshortcut-x11.cpp
    HEADERS globalshortcuts/globalshortcutsbackend-x11.h globalshortcuts/globalshortcut.h
  )
  optional_source(WIN32
    SOURCES globalshortcuts/globalshortcutsbackend-win.cpp globalshortcuts/globalshortcut.cpp globalshortcuts/globalshortcut-win.cpp
    HEADERS globalshortcuts/globalshortcutsbackend-win.h globalshortcuts/globalshortcut.h
  )
endif(HAVE_GLOBALSHORTCUTS)

# ALSA
optional_source(HAVE_ALSA SOURCES engine/alsadevicefinder.cpp engine/alsapcmdevicefinder.cpp)

# DBUS
optional_source(HAVE_DBUS SOURCES osd/osddbus.cpp HEADERS osd/osddbus.h)

# GStreamer
optional_source(HAVE_GSTREAMER
  SOURCES engine/gststartup.cpp engine/gstengine.cpp engine/gstenginepipeline.cpp
  HEADERS engine/gststartup.h engine/gstengine.h engine/gstenginepipeline.h
)

# VLC
optional_source(HAVE_VLC SOURCES engine/vlcengine.cpp HEADERS engine/vlcengine.h)

# DBUS and MPRIS - Unix specific
if(UNIX AND HAVE_DBUS)

  optional_source(HAVE_DBUS SOURCES core/mpris2.cpp HEADERS core/mpris2.h)
  optional_source(HAVE_UDISKS2 SOURCES device/udisks2lister.cpp HEADERS device/udisks2lister.h)

  # MPRIS 2.0 DBUS interfaces
  qt_add_dbus_adaptor(SOURCES dbus/org.mpris.MediaPlayer2.Player.xml core/mpris2.h mpris::Mpris2 mpris2_player Mpris2Player)
  qt_add_dbus_adaptor(SOURCES dbus/org.mpris.MediaPlayer2.xml core/mpris2.h mpris::Mpris2 mpris2_root Mpris2Root)
  qt_add_dbus_adaptor(SOURCES dbus/org.mpris.MediaPlayer2.TrackList.xml core/mpris2.h mpris::Mpris2 mpris2_tracklist Mpris2TrackList)

  # MPRIS 2.1 DBUS interfaces
  qt_add_dbus_adaptor(SOURCES dbus/org.mpris.MediaPlayer2.Playlists.xml core/mpris2.h mpris::Mpris2 mpris2_playlists Mpris2Playlists)

  # org.freedesktop.Notifications DBUS interface
  qt_add_dbus_interface(SOURCES dbus/org.freedesktop.Notifications.xml notification)

  # org.gnome.SettingsDaemon interface
  qt_add_dbus_interface(SOURCES dbus/org.gnome.SettingsDaemon.MediaKeys.xml gnomesettingsdaemon)

  # org.mate.SettingsDaemon interface
  qt_add_dbus_interface(SOURCES dbus/org.mate.SettingsDaemon.MediaKeys.xml matesettingsdaemon)

  # org.kde.KGlobalAccel interface
  qt_add_dbus_interface(SOURCES dbus/org.kde.KGlobalAccel.xml kglobalaccel)
  qt_add_dbus_interface(SOURCES dbus/org.kde.KGlobalAccel.Component.xml kglobalaccelcomponent)

  if(HAVE_UDISKS2)
    set_source_files_properties(dbus/org.freedesktop.DBus.ObjectManager.xml PROPERTIES NO_NAMESPACE objectmanager INCLUDE dbus/metatypes.h)
    set_source_files_properties(dbus/org.freedesktop.UDisks2.Filesystem.xml PROPERTIES NO_NAMESPACE udisks2filesystem INCLUDE dbus/metatypes.h)
    set_source_files_properties(dbus/org.freedesktop.UDisks2.Block.xml PROPERTIES NO_NAMESPACE udisks2block INCLUDE dbus/metatypes.h)
    set_source_files_properties(dbus/org.freedesktop.UDisks2.Drive.xml PROPERTIES NO_NAMESPACE udisks2drive INCLUDE dbus/metatypes.h)
    set_source_files_properties(dbus/org.freedesktop.UDisks2.Job.xml PROPERTIES NO_NAMESPACE udisks2job INCLUDE dbus/metatypes.h)
    qt_add_dbus_interface(SOURCES dbus/org.freedesktop.DBus.ObjectManager.xml objectmanager)
    qt_add_dbus_interface(SOURCES dbus/org.freedesktop.UDisks2.Filesystem.xml udisks2filesystem)
    qt_add_dbus_interface(SOURCES dbus/org.freedesktop.UDisks2.Block.xml udisks2block)
    qt_add_dbus_interface(SOURCES dbus/org.freedesktop.UDisks2.Drive.xml udisks2drive)
    qt_add_dbus_interface(SOURCES dbus/org.freedesktop.UDisks2.Job.xml udisks2job)
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
optional_source(HAVE_GIO SOURCES device/giolister.cpp HEADERS device/giolister.h)

# MTP device
optional_source(HAVE_LIBMTP
  SOURCES
    device/mtpconnection.cpp
    device/mtpdevice.cpp
    device/mtploader.cpp
  HEADERS
    device/mtpconnection.h
    device/mtpdevice.h
    device/mtploader.h
)

# Pulse audio integration
optional_source(HAVE_LIBPULSE SOURCES engine/pulsedevicefinder.cpp)

# Transcoder require GStreamer
optional_source(HAVE_GSTREAMER
SOURCES
  transcoder/transcoder.cpp
  transcoder/transcoderoptionsinterface.cpp
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
  transcoder/transcoderoptionsinterface.h
  transcoder/transcoderoptionsflac.h
  transcoder/transcoderoptionswavpack.h
  transcoder/transcoderoptionsvorbis.h
  transcoder/transcoderoptionsopus.h
  transcoder/transcoderoptionsspeex.h
  transcoder/transcoderoptionsaac.h
  transcoder/transcoderoptionsasf.h
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

# CHROMAPRINT
if(HAVE_SONGFINGERPRINTING OR HAVE_MUSICBRAINZ)
  optional_source(CHROMAPRINT_FOUND SOURCES engine/chromaprinter.cpp)
endif()

# MusicBrainz
optional_source(HAVE_MUSICBRAINZ
  SOURCES
    musicbrainz/acoustidclient.cpp
    musicbrainz/musicbrainzclient.cpp
    musicbrainz/tagfetcher.cpp
  HEADERS
    musicbrainz/acoustidclient.h
    musicbrainz/musicbrainzclient.h
    musicbrainz/tagfetcher.h
)

# CDIO backend and device
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

# Platform specific - macOS
optional_source(APPLE
  SOURCES
    utilities/macosutils.mm
    core/scoped_nsautorelease_pool.mm
    core/mac_startup.mm
    core/macsystemtrayicon.mm
    core/macfslistener.mm
    osd/osdmac.mm
    widgets/searchfield_mac.mm
    engine/macosdevicefinder.cpp
    globalshortcuts/globalshortcutsbackend-macos.mm
    globalshortcuts/globalshortcutgrabber.mm
    device/macosdevicelister.mm
  HEADERS
    core/macsystemtrayicon.h
    core/macfslistener.h
    osd/osdmac.h
    globalshortcuts/globalshortcutsbackend-macos.h
    device/macosdevicelister.h
)

# Platform specific - Windows
optional_source(WIN32
  SOURCES
    utilities/winutils.cpp
    engine/directsounddevicefinder.cpp
    engine/mmdevicefinder.cpp
    core/scopedwchararray.cpp
    core/windows7thumbbar.cpp
  HEADERS
    core/windows7thumbbar.h
)
optional_source(MSVC SOURCES engine/uwpdevicefinder.cpp engine/asiodevicefinder.cpp)

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

optional_source(HAVE_SPOTIFY
  SOURCES
    spotify/spotifyservice.cpp
    spotify/spotifybaserequest.cpp
    spotify/spotifyrequest.cpp
    spotify/spotifyfavoriterequest.cpp
    settings/spotifysettingspage.cpp
    covermanager/spotifycoverprovider.cpp
  HEADERS
    spotify/spotifyservice.h
    spotify/spotifybaserequest.h
    spotify/spotifyrequest.h
    spotify/spotifyfavoriterequest.h
    settings/spotifysettingspage.h
    covermanager/spotifycoverprovider.h
  UI
    settings/spotifysettingspage.ui
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
    moodbar/gstfastspectrumplugin.cpp
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

# EBU R 128
optional_source(HAVE_EBUR128
  SOURCES engine/ebur128analysis.cpp
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

qt_wrap_cpp(MOC ${HEADERS})
qt_wrap_ui(UIC ${UI})
qt_add_resources(QRC ${RESOURCES})

if(HAVE_TRANSLATIONS)

  set(LINGUAS "All" CACHE STRING "A space-seperated list of translations to compile in to Strawberry, or \"None\".")
  if(LINGUAS STREQUAL "All")
    # build LANGUAGES from all existing .po files
    file(GLOB pofiles translations/*.po)
    foreach(pofile ${pofiles})
      get_filename_component(lang ${pofile} NAME_WE)
      list(APPEND LANGUAGES ${lang})
    endforeach(pofile)
  else(LINGUAS STREQUAL "All")
    if(NOT LINGUAS OR LINGUAS STREQUAL "None")
      set(LANGUAGES "")
    else(NOT LINGUAS OR LINGUAS STREQUAL "None")
      string(REGEX MATCHALL [a-zA-Z_@]+ LANGUAGES ${LINGUAS})
    endif(NOT LINGUAS OR LINGUAS STREQUAL "None")
  endif(LINGUAS STREQUAL "All")

  if(NOT MSVC)
    add_pot(POT
      ${CMAKE_CURRENT_SOURCE_DIR}/translations/header
      ${CMAKE_CURRENT_SOURCE_DIR}/translations/translations.pot
      ${SOURCES}
      ${MOC}
      ${UIC}
      ${CMAKE_SOURCE_DIR}/data/html/oauthsuccess.html
    )
  endif()
  add_po(PO strawberry_ LANGUAGES ${LANGUAGES} DIRECTORY translations)

endif(HAVE_TRANSLATIONS)

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
  ${SQLITE_INCLUDE_DIRS}
  ${PROTOBUF_INCLUDE_DIRS}
  ${ICU_INCLUDE_DIRS}
)

if(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
  target_include_directories(strawberry_lib SYSTEM PUBLIC ${Qt${QT_VERSION_MAJOR}Gui_PRIVATE_INCLUDE_DIRS})
endif()

target_include_directories(strawberry_lib PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-common
  ${CMAKE_SOURCE_DIR}/ext/libstrawberry-tagreader
  ${CMAKE_BINARY_DIR}/ext/libstrawberry-tagreader
  ${SINGLEAPPLICATION_INCLUDE_DIRS}
)

target_link_directories(strawberry_lib PUBLIC
  ${Boost_LIBRARY_DIRS}
  ${GLIB_LIBRARY_DIRS}
  ${GOBJECT_LIBRARY_DIRS}
  ${SQLITE_LIBRARY_DIRS}
  ${PROTOBUF_LIBRARY_DIRS}
  ${SINGLEAPPLICATION_LIBRARY_DIRS}
  ${ICU_LIBRARY_DIRS}
)

target_link_libraries(strawberry_lib PUBLIC
  ${CMAKE_THREAD_LIBS_INIT}
  ${GLIB_LIBRARIES}
  ${GOBJECT_LIBRARIES}
  ${SQLITE_LIBRARIES}
  ${ICU_LIBRARIES}
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Concurrent
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Sql
  ${Protobuf_LIBRARIES}
  ${SINGLEAPPLICATION_LIBRARIES}
  libstrawberry-common
  libstrawberry-tagreader
)

if(HAVE_DBUS)
  target_link_libraries(strawberry_lib PUBLIC Qt${QT_VERSION_MAJOR}::DBus)
endif()

if(HAVE_ALSA)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${ALSA_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${ALSA_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${ALSA_LIBRARIES})
endif()

if(HAVE_LIBPULSE)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBPULSE_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${LIBPULSE_LIBRARY_DIRS})
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
  target_link_directories(strawberry_lib PRIVATE
    ${GSTREAMER_LIBRARY_DIRS}
    ${GSTREAMER_BASE_LIBRARY_DIRS}
    ${GSTREAMER_APP_LIBRARY_DIRS}
    ${GSTREAMER_AUDIO_LIBRARY_DIRS}
    ${GSTREAMER_TAG_LIBRARY_DIRS}
    ${GSTREAMER_PBUTILS_LIBRARY_DIRS}
  )
  target_link_libraries(strawberry_lib PRIVATE
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
    ${GSTREAMER_AUDIO_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_TAG_LIBRARIES}
    ${GSTREAMER_PBUTILS_LIBRARIES}
  )
endif()

if(HAVE_MOODBAR)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/3rdparty/gstfastspectrum)
  target_link_libraries(strawberry_lib PRIVATE gstfastspectrum)
endif()

if(HAVE_VLC)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBVLC_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${LIBVLC_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBVLC_LIBRARIES})
endif()

if(HAVE_SONGFINGERPRINTING OR HAVE_MUSICBRAINZ)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${CHROMAPRINT_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${CHROMAPRINT_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${CHROMAPRINT_LIBRARIES})
endif()

if(HAVE_EBUR128)
  target_link_libraries(strawberry_lib PRIVATE PkgConfig::LIBEBUR128)
endif()

if(X11_FOUND)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${X11_INCLUDE_DIR})
  target_link_directories(strawberry_lib PRIVATE ${X11_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${X11_LIBRARIES})
endif()

if(XCB_FOUND)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${XCB_INCLUDE_DIR})
  target_link_directories(strawberry_lib PRIVATE ${XCB_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${XCB_LIBRARIES})
endif()

if(HAVE_GIO)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${GIO_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${GIO_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${GIO_LIBRARIES})
endif()

if(HAVE_GIO_UNIX)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${GIO_UNIX_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${GIO_UNIX_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${GIO_UNIX_LIBRARIES})
endif()

if(HAVE_AUDIOCD)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBCDIO_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${LIBCDIO_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBCDIO_LIBRARIES})
endif()

if(HAVE_LIBGPOD)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBGPOD_INCLUDE_DIRS} ${GDK_PIXBUF_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${LIBGPOD_LIBRARY_DIRS} ${GDK_PIXBUF_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBGPOD_LIBRARIES} ${GDK_PIXBUF_LIBRARIES})
endif()

if(HAVE_LIBMTP)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${LIBMTP_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${LIBMTP_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${LIBMTP_LIBRARIES})
endif()

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
endif()

if(WIN32)
  target_link_libraries(strawberry_lib PRIVATE dsound dwmapi)
  if(MSVC)
    target_link_libraries(strawberry_lib PRIVATE WindowsApp)
  endif()
  target_link_libraries(strawberry_lib PRIVATE getopt-win::getopt)
endif()

if(HAVE_QTSPARKLE)
  target_include_directories(strawberry_lib SYSTEM PRIVATE ${QTSPARKLE_INCLUDE_DIRS})
  target_link_directories(strawberry_lib PRIVATE ${QTSPARKLE_LIBRARY_DIRS})
  target_link_libraries(strawberry_lib PRIVATE ${QTSPARKLE_LIBRARIES})
endif()

###############################################################################

set(EXECUTABLE_OUTPUT_PATH ..)

# Show the console window in debug mode on Windows
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT ENABLE_WIN32_CONSOLE)
  set(STRAWBERRY-WIN32-FLAG WIN32)
endif()

# Resource file for windows
if(WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/../dist/windows/windres.rc.in ${CMAKE_CURRENT_BINARY_DIR}/windres.rc)
  set(STRAWBERRY-WIN32-RESOURCES windres.rc)
endif()

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
endif()
