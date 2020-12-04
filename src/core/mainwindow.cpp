/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2020, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "version.h"

#include <memory>
#include <functional>
#include <algorithm>
#include <cmath>

#include <QMainWindow>
#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QThread>
#include <QSystemTrayIcon>
#include <QSortFilterProxyModel>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QIcon>
#include <QMimeData>
#include <QPalette>
#include <QTimer>
#include <QKeySequence>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QShortcut>
#include <QMessageBox>
#include <QtEvents>
#include <QSettings>
#include <QColor>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLayout>
#include <QSize>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QToolButton>
#include <QClipboard>

#include "core/logging.h"
#include "core/closure.h"
#include "core/network.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "utilities.h"
#include "timeconstants.h"
#include "commandlineoptions.h"
#include "mimedata.h"
#include "iconloader.h"
#include "taskmanager.h"
#include "song.h"
#include "stylehelper.h"
#include "stylesheetloader.h"
#include "systemtrayicon.h"
#include "application.h"
#include "database.h"
#include "player.h"
#include "appearance.h"
#include "filesystemmusicstorage.h"
#include "deletefiles.h"
#include "engine/enginetype.h"
#include "engine/enginebase.h"
#include "engine/engine_fwd.h"
#include "dialogs/errordialog.h"
#include "dialogs/about.h"
#include "dialogs/console.h"
#include "dialogs/trackselectiondialog.h"
#include "dialogs/edittagdialog.h"
#include "dialogs/addstreamdialog.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "dialogs/lastfmimportdialog.h"
#include "dialogs/snapdialog.h"
#include "organize/organizedialog.h"
#include "widgets/fancytabwidget.h"
#include "widgets/playingwidget.h"
#include "widgets/volumeslider.h"
#include "widgets/fileview.h"
#include "widgets/multiloadingindicator.h"
#include "widgets/trackslider.h"
#include "osd/osdbase.h"
#include "context/contextview.h"
#include "context/contextalbumsview.h"
#include "collection/collection.h"
#include "collection/collectionbackend.h"
#include "collection/collectiondirectorymodel.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionmodel.h"
#include "collection/collectionquery.h"
#include "collection/collectionview.h"
#include "collection/collectionviewcontainer.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistcontainer.h"
#include "playlist/playlistlistcontainer.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "playlist/playlistview.h"
#include "queue/queue.h"
#include "queue/queueview.h"
#include "playlistparsers/playlistparser.h"
#include "analyzer/analyzercontainer.h"
#include "equalizer/equalizer.h"
#ifdef HAVE_GLOBALSHORTCUTS
#  include "globalshortcuts/globalshortcuts.h"
#endif
#include "covermanager/albumcovermanager.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "lyrics/lyricsproviders.h"
#ifndef Q_OS_WIN
#  include "device/devicemanager.h"
#  include "device/devicestatefiltermodel.h"
#  include "device/deviceview.h"
#  include "device/deviceviewcontainer.h"
#endif
#include "transcoder/transcodedialog.h"
#include "settings/settingsdialog.h"
#include "settings/behavioursettingspage.h"
#include "settings/backendsettingspage.h"
#include "settings/playlistsettingspage.h"
#ifdef HAVE_SUBSONIC
#  include "settings/subsonicsettingspage.h"
#  include "scrobbler/subsonicscrobbler.h"
#endif
#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "settings/tidalsettingspage.h"
#endif
#ifdef HAVE_QOBUZ
#  include "settings/qobuzsettingspage.h"
#endif

#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "internet/internetsongsview.h"
#include "internet/internettabsview.h"
#include "internet/internetcollectionview.h"
#include "internet/internetsearchview.h"

#include "scrobbler/audioscrobbler.h"
#include "scrobbler/lastfmimport.h"

#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
#  include "musicbrainz/tagfetcher.h"
#endif

#ifdef Q_OS_MACOS
#  include "core/macsystemtrayicon.h"
#endif

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarcontroller.h"
#  include "moodbar/moodbarproxystyle.h"
#endif

#include "smartplaylists/smartplaylistsviewcontainer.h"
#include "smartplaylists/smartplaylistsview.h"

#ifdef Q_OS_WIN
#  include "windows7thumbbar.h"
#endif

#ifdef HAVE_QTSPARKLE
#  if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
#    include <qtsparkle-qt6/Updater>
#  else
#    include <qtsparkle-qt5/Updater>
#  endif
#endif  // HAVE_QTSPARKLE

const char *MainWindow::kSettingsGroup = "MainWindow";
const char *MainWindow::kAllFilesFilterSpec = QT_TR_NOOP("All Files (*)");

namespace {
const int kTrackSliderUpdateTimeMs = 200;
const int kTrackPositionUpdateTimeMs = 1000;
}

MainWindow::MainWindow(Application *app, SystemTrayIcon *tray_icon, OSDBase *osd, const CommandlineOptions &options, QWidget *parent) :
      QMainWindow(parent),
      ui_(new Ui_MainWindow),
#ifdef Q_OS_WIN
      thumbbar_(new Windows7ThumbBar(this)),
#endif
      app_(app),
      tray_icon_(tray_icon),
      osd_(osd),
      console_([=]() {
        Console *console = new Console(app);
        return console;
      }),
      edit_tag_dialog_(std::bind(&MainWindow::CreateEditTagDialog, this)),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
#ifdef HAVE_GLOBALSHORTCUTS
      global_shortcuts_(new GlobalShortcuts(this)),
#endif
      context_view_(new ContextView(this)),
      collection_view_(new CollectionViewContainer(this)),
      file_view_(new FileView(this)),
#ifndef Q_OS_WIN
      device_view_(new DeviceViewContainer(this)),
#endif
      playlist_list_(new PlaylistListContainer(this)),
      queue_view_(new QueueView(this)),
      settings_dialog_(std::bind(&MainWindow::CreateSettingsDialog, this)),
      cover_manager_([=]() {
        AlbumCoverManager *cover_manager = new AlbumCoverManager(app, app->collection_backend(), this);
        cover_manager->Init();

        // Cover manager connections
        connect(cover_manager, SIGNAL(AddToPlaylist(QMimeData*)), this, SLOT(AddToPlaylist(QMimeData*)));
        return cover_manager;
      }),
      equalizer_(new Equalizer),
      organize_dialog_([=]() {
        OrganizeDialog *dialog = new OrganizeDialog(app->task_manager(), app->collection_backend(), this);
        dialog->SetDestinationModel(app->collection()->model()->directory_model());
        return dialog;
      }),
#ifdef HAVE_GSTREAMER
      transcode_dialog_([=]() {
        TranscodeDialog *dialog = new TranscodeDialog(this);
        return dialog;
      }),
#endif
      add_stream_dialog_([=]() {
        AddStreamDialog *add_stream_dialog = new AddStreamDialog;
        connect(add_stream_dialog, SIGNAL(accepted()), this, SLOT(AddStreamAccepted()));
        return add_stream_dialog;
      }),
      smartplaylists_view_(new SmartPlaylistsViewContainer(app, this)),
#ifdef HAVE_SUBSONIC
      subsonic_view_(new InternetSongsView(app_, app->internet_services()->ServiceBySource(Song::Source_Subsonic), SubsonicSettingsPage::kSettingsGroup, SettingsDialog::Page_Subsonic, this)),
#endif
#ifdef HAVE_TIDAL
      tidal_view_(new InternetTabsView(app_, app->internet_services()->ServiceBySource(Song::Source_Tidal), TidalSettingsPage::kSettingsGroup, SettingsDialog::Page_Tidal, this)),
#endif
#ifdef HAVE_QOBUZ
      qobuz_view_(new InternetTabsView(app_, app->internet_services()->ServiceBySource(Song::Source_Qobuz), QobuzSettingsPage::kSettingsGroup, SettingsDialog::Page_Qobuz, this)),
#endif
      lastfm_import_dialog_(new LastFMImportDialog(app_->lastfm_import(), this)),
      collection_show_all_(nullptr),
      collection_show_duplicates_(nullptr),
      collection_show_untagged_(nullptr),
      playlist_menu_(new QMenu(this)),
      playlist_play_pause_(nullptr),
      playlist_stop_after_(nullptr),
      playlist_undoredo_(nullptr),
      playlist_copy_url_(nullptr),
      playlist_show_in_collection_(nullptr),
      playlist_copy_to_collection_(nullptr),
      playlist_move_to_collection_(nullptr),
      playlist_open_in_browser_(nullptr),
      playlist_organize_(nullptr),
#ifndef Q_OS_WIN
      playlist_copy_to_device_(nullptr),
#endif
      playlist_delete_(nullptr),
      playlist_queue_(nullptr),
      playlist_queue_play_next_(nullptr),
      playlist_skip_(nullptr),
      playlist_add_to_another_(nullptr),
      playlistitem_actions_separator_(nullptr),
      playlist_rescan_songs_(nullptr),
      collection_sort_model_(new QSortFilterProxyModel(this)),
      track_position_timer_(new QTimer(this)),
      track_slider_timer_(new QTimer(this)),
      keep_running_(false),
      playing_widget_(true),
      doubleclick_addmode_(BehaviourSettingsPage::AddBehaviour_Append),
      doubleclick_playmode_(BehaviourSettingsPage::PlayBehaviour_Never),
      doubleclick_playlist_addmode_(BehaviourSettingsPage::PlaylistAddBehaviour_Play),
      menu_playmode_(BehaviourSettingsPage::PlayBehaviour_Never),
      initialized_(false),
      was_maximized_(true),
      was_minimized_(false),
      hidden_(false),
      exit_count_(0),
      delete_files_(false)
      {

  qLog(Debug) << "Starting";

  connect(app, SIGNAL(ErrorAdded(QString)), SLOT(ShowErrorDialog(QString)));
  connect(app, SIGNAL(SettingsDialogRequested(SettingsDialog::Page)), SLOT(OpenSettingsDialogAtPage(SettingsDialog::Page)));

  // Initialize the UI
  ui_->setupUi(this);

  album_cover_choice_controller_->Init(app);

  ui_->multi_loading_indicator->SetTaskManager(app_->task_manager());
  context_view_->Init(app_, collection_view_->view(), album_cover_choice_controller_);
  ui_->widget_playing->Init(app_, album_cover_choice_controller_);

  // Initialize the search widget
  StyleHelper::setBaseColor(palette().color(QPalette::Highlight).darker());

  // Add tabs to the fancy tab widget
  ui_->tabs->AddTab(context_view_, "context", IconLoader::Load("strawberry"), tr("Context"));
  ui_->tabs->AddTab(collection_view_, "collection", IconLoader::Load("library-music"), tr("Collection"));
  ui_->tabs->AddTab(queue_view_, "queue", IconLoader::Load("footsteps"), tr("Queue"));
  ui_->tabs->AddTab(playlist_list_, "playlists", IconLoader::Load("view-media-playlist"), tr("Playlists"));
  ui_->tabs->AddTab(smartplaylists_view_, "smartplaylists", IconLoader::Load("view-media-playlist"), tr("Smart playlists"));
  ui_->tabs->AddTab(file_view_, "files", IconLoader::Load("document-open"), tr("Files"));
#ifndef Q_OS_WIN
  ui_->tabs->AddTab(device_view_, "devices", IconLoader::Load("device"), tr("Devices"));
#endif
#ifdef HAVE_SUBSONIC
  ui_->tabs->AddTab(subsonic_view_, "subsonic", IconLoader::Load("subsonic"), tr("Subsonic"));
#endif
#ifdef HAVE_TIDAL
  ui_->tabs->AddTab(tidal_view_, "tidal", IconLoader::Load("tidal"), tr("Tidal"));
#endif
#ifdef HAVE_QOBUZ
  ui_->tabs->AddTab(qobuz_view_, "qobuz", IconLoader::Load("qobuz"), tr("Qobuz"));
#endif

  // Add the playing widget to the fancy tab widget
  ui_->tabs->addBottomWidget(ui_->widget_playing);
  //ui_->tabs->SetBackgroundPixmap(QPixmap(":/pictures/strawberry-background.png"));
  ui_->tabs->Load(kSettingsGroup);

  track_position_timer_->setInterval(kTrackPositionUpdateTimeMs);
  connect(track_position_timer_, SIGNAL(timeout()), SLOT(UpdateTrackPosition()));
  track_slider_timer_->setInterval(kTrackSliderUpdateTimeMs);
  connect(track_slider_timer_, SIGNAL(timeout()), SLOT(UpdateTrackSliderPosition()));

  // Start initializing the player
  qLog(Debug) << "Initializing player";
  app_->player()->SetAnalyzer(ui_->analyzer);
  app_->player()->SetEqualizer(equalizer_.get());
  app_->player()->Init();
  EngineChanged(app_->player()->engine()->type());
  int volume = app_->player()->GetVolume();
  ui_->volume->setValue(volume);
  VolumeChanged(volume);

  // Models
  qLog(Debug) << "Creating models";
  collection_sort_model_->setSourceModel(app_->collection()->model());
  collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  collection_sort_model_->setDynamicSortFilter(true);
  collection_sort_model_->setSortLocaleAware(true);
  collection_sort_model_->sort(0);

  qLog(Debug) << "Creating models finished";

  connect(ui_->playlist, SIGNAL(ViewSelectionModelChanged()), SLOT(PlaylistViewSelectionModelChanged()));

  ui_->playlist->SetManager(app_->playlist_manager());

  ui_->playlist->view()->Init(app_);

  collection_view_->view()->setModel(collection_sort_model_);
  collection_view_->view()->SetApplication(app_);
#ifndef Q_OS_WIN
  device_view_->view()->SetApplication(app_);
#endif
  playlist_list_->SetApplication(app_);

  organize_dialog_->SetDestinationModel(app_->collection()->model()->directory_model());

  // Icons
  qLog(Debug) << "Creating UI";

  // Help menu

  ui_->action_about_strawberry->setIcon(IconLoader::Load("strawberry"));
  ui_->action_about_qt->setIcon(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"));

  // Music menu

  ui_->action_open_file->setIcon(IconLoader::Load("document-open"));
  ui_->action_open_cd->setIcon(IconLoader::Load("media-optical"));
  ui_->action_previous_track->setIcon(IconLoader::Load("media-skip-backward"));
  ui_->action_play_pause->setIcon(IconLoader::Load("media-playback-start"));
  ui_->action_stop->setIcon(IconLoader::Load("media-playback-stop"));
  ui_->action_stop_after_this_track->setIcon(IconLoader::Load("media-playback-stop"));
  ui_->action_next_track->setIcon(IconLoader::Load("media-skip-forward"));
  ui_->action_quit->setIcon(IconLoader::Load("application-exit"));

  // Playlist

  ui_->action_add_file->setIcon(IconLoader::Load("document-open"));
  ui_->action_add_folder->setIcon(IconLoader::Load("document-open-folder"));
  ui_->action_add_stream->setIcon(IconLoader::Load("document-open-remote"));
  ui_->action_shuffle_mode->setIcon(IconLoader::Load("media-playlist-shuffle"));
  ui_->action_repeat_mode->setIcon(IconLoader::Load("media-playlist-repeat"));
  ui_->action_new_playlist->setIcon(IconLoader::Load("document-new"));
  ui_->action_save_playlist->setIcon(IconLoader::Load("document-save"));
  ui_->action_load_playlist->setIcon(IconLoader::Load("document-open"));
  ui_->action_jump->setIcon(IconLoader::Load("go-jump"));
  ui_->action_clear_playlist->setIcon(IconLoader::Load("edit-clear-list"));
  ui_->action_shuffle->setIcon(IconLoader::Load("media-playlist-shuffle"));
  ui_->action_remove_duplicates->setIcon(IconLoader::Load("list-remove"));
  ui_->action_remove_unavailable->setIcon(IconLoader::Load("list-remove"));
  ui_->action_remove_from_playlist->setIcon(IconLoader::Load("list-remove"));

  // Configure

  ui_->action_cover_manager->setIcon(IconLoader::Load("document-download"));
  ui_->action_edit_track->setIcon(IconLoader::Load("edit-rename"));
  ui_->action_edit_value->setIcon(IconLoader::Load("edit-rename"));
  ui_->action_selection_set_value->setIcon(IconLoader::Load("edit-rename"));
  ui_->action_equalizer->setIcon(IconLoader::Load("equalizer"));
  ui_->action_transcoder->setIcon(IconLoader::Load("tools-wizard"));
  ui_->action_update_collection->setIcon(IconLoader::Load("view-refresh"));
  ui_->action_full_collection_scan->setIcon(IconLoader::Load("view-refresh"));
  ui_->action_settings->setIcon(IconLoader::Load("configure"));
  ui_->action_import_data_from_last_fm->setIcon(IconLoader::Load("scrobble"));

  // Scrobble

  ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble-disabled"));
  ui_->action_love->setIcon(IconLoader::Load("love"));

  // File view connections
  connect(file_view_, SIGNAL(AddToPlaylist(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(file_view_, SIGNAL(PathChanged(QString)), SLOT(FilePathChanged(QString)));
#ifdef HAVE_GSTREAMER
  connect(file_view_, SIGNAL(CopyToCollection(QList<QUrl>)), SLOT(CopyFilesToCollection(QList<QUrl>)));
  connect(file_view_, SIGNAL(MoveToCollection(QList<QUrl>)), SLOT(MoveFilesToCollection(QList<QUrl>)));
  connect(file_view_, SIGNAL(EditTags(QList<QUrl>)), SLOT(EditFileTags(QList<QUrl>)));
#ifndef Q_OS_WIN
  connect(file_view_, SIGNAL(CopyToDevice(QList<QUrl>)), SLOT(CopyFilesToDevice(QList<QUrl>)));
#endif
#endif
  file_view_->SetTaskManager(app_->task_manager());

  // Action connections
  connect(ui_->action_next_track, SIGNAL(triggered()), app_->player(), SLOT(Next()));
  connect(ui_->action_previous_track, SIGNAL(triggered()), app_->player(), SLOT(Previous()));
  connect(ui_->action_play_pause, SIGNAL(triggered()), app_->player(), SLOT(PlayPause()));
  connect(ui_->action_stop, SIGNAL(triggered()), app_->player(), SLOT(Stop()));
  connect(ui_->action_quit, SIGNAL(triggered()), SLOT(Exit()));
  connect(ui_->action_stop_after_this_track, SIGNAL(triggered()), SLOT(StopAfterCurrent()));
  connect(ui_->action_mute, SIGNAL(triggered()), app_->player(), SLOT(Mute()));

  connect(ui_->action_clear_playlist, SIGNAL(triggered()), SLOT(PlaylistClearCurrent()));
  connect(ui_->action_remove_duplicates, SIGNAL(triggered()), app_->playlist_manager(), SLOT(RemoveDuplicatesCurrent()));
  connect(ui_->action_remove_unavailable, SIGNAL(triggered()), app_->playlist_manager(), SLOT(RemoveUnavailableCurrent()));
  connect(ui_->action_remove_from_playlist, SIGNAL(triggered()), SLOT(PlaylistRemoveCurrent()));
  connect(ui_->action_edit_track, SIGNAL(triggered()), SLOT(EditTracks()));
  connect(ui_->action_renumber_tracks, SIGNAL(triggered()), SLOT(RenumberTracks()));
  connect(ui_->action_selection_set_value, SIGNAL(triggered()), SLOT(SelectionSetValue()));
  connect(ui_->action_edit_value, SIGNAL(triggered()), SLOT(EditValue()));
#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
  connect(ui_->action_auto_complete_tags, SIGNAL(triggered()), SLOT(AutoCompleteTags()));
#endif
  connect(ui_->action_settings, SIGNAL(triggered()), SLOT(OpenSettingsDialog()));
  connect(ui_->action_import_data_from_last_fm, SIGNAL(triggered()), lastfm_import_dialog_, SLOT(show()));
  connect(ui_->action_toggle_show_sidebar, SIGNAL(toggled(bool)), SLOT(ToggleSidebar(bool)));
  connect(ui_->action_about_strawberry, SIGNAL(triggered()), SLOT(ShowAboutDialog()));
  connect(ui_->action_about_qt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
  connect(ui_->action_shuffle, SIGNAL(triggered()), app_->playlist_manager(), SLOT(ShuffleCurrent()));
  connect(ui_->action_open_file, SIGNAL(triggered()), SLOT(AddFile()));
  connect(ui_->action_open_cd, SIGNAL(triggered()), SLOT(AddCDTracks()));
  connect(ui_->action_add_file, SIGNAL(triggered()), SLOT(AddFile()));
  connect(ui_->action_add_folder, SIGNAL(triggered()), SLOT(AddFolder()));
  connect(ui_->action_add_stream, SIGNAL(triggered()), SLOT(AddStream()));
  connect(ui_->action_cover_manager, SIGNAL(triggered()), SLOT(ShowCoverManager()));
  connect(ui_->action_equalizer, SIGNAL(triggered()), SLOT(ShowEqualizer()));
#if defined(HAVE_GSTREAMER)
  connect(ui_->action_transcoder, SIGNAL(triggered()), SLOT(ShowTranscodeDialog()));
#else
  ui_->action_transcoder->setDisabled(true);
#endif
  connect(ui_->action_jump, SIGNAL(triggered()), ui_->playlist->view(), SLOT(JumpToCurrentlyPlayingTrack()));
  connect(ui_->action_update_collection, SIGNAL(triggered()), app_->collection(), SLOT(IncrementalScan()));
  connect(ui_->action_full_collection_scan, SIGNAL(triggered()), app_->collection(), SLOT(FullScan()));
  connect(ui_->action_abort_collection_scan, SIGNAL(triggered()), app_->collection(), SLOT(AbortScan()));
#if defined(HAVE_GSTREAMER)
  connect(ui_->action_add_files_to_transcoder, SIGNAL(triggered()), SLOT(AddFilesToTranscoder()));
  ui_->action_add_files_to_transcoder->setIcon(IconLoader::Load("tools-wizard"));
#else
  ui_->action_add_files_to_transcoder->setDisabled(true);
#endif

  connect(ui_->action_toggle_scrobbling, SIGNAL(triggered()), app_->scrobbler(), SLOT(ToggleScrobbling()));
  connect(ui_->action_love, SIGNAL(triggered()), SLOT(Love()));
  connect(app_->scrobbler(), SIGNAL(ErrorMessage(QString)), SLOT(ShowErrorDialog(QString)));

  // Playlist view actions
  ui_->action_next_playlist->setShortcuts(QList<QKeySequence>() << QKeySequence::fromString("Ctrl+Tab")<< QKeySequence::fromString("Ctrl+PgDown"));
  ui_->action_previous_playlist->setShortcuts(QList<QKeySequence>() << QKeySequence::fromString("Ctrl+Shift+Tab")<< QKeySequence::fromString("Ctrl+PgUp"));

  // Actions for switching tabs will be global to the entire window, so adding them here
  addAction(ui_->action_next_playlist);
  addAction(ui_->action_previous_playlist);

  // Give actions to buttons
  ui_->forward_button->setDefaultAction(ui_->action_next_track);
  ui_->back_button->setDefaultAction(ui_->action_previous_track);
  ui_->pause_play_button->setDefaultAction(ui_->action_play_pause);
  ui_->stop_button->setDefaultAction(ui_->action_stop);
  ui_->button_scrobble->setDefaultAction(ui_->action_toggle_scrobbling);
  ui_->button_love->setDefaultAction(ui_->action_love);

  ui_->playlist->SetActions(ui_->action_new_playlist, ui_->action_load_playlist, ui_->action_save_playlist, ui_->action_clear_playlist, ui_->action_next_playlist,    /* These two actions aren't associated */ ui_->action_previous_playlist /* to a button but to the main window */ );

  // Add the shuffle and repeat action groups to the menu
  ui_->action_shuffle_mode->setMenu(ui_->playlist_sequence->shuffle_menu());
  ui_->action_repeat_mode->setMenu(ui_->playlist_sequence->repeat_menu());

  // Stop actions
  QMenu *stop_menu = new QMenu(this);
  stop_menu->addAction(ui_->action_stop);
  stop_menu->addAction(ui_->action_stop_after_this_track);
  ui_->stop_button->setMenu(stop_menu);

  // Player connections
  connect(ui_->volume, SIGNAL(valueChanged(int)), app_->player(), SLOT(SetVolume(int)));

  connect(app_->player(), SIGNAL(EngineChanged(Engine::EngineType)), SLOT(EngineChanged(Engine::EngineType)));
  connect(app_->player(), SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  connect(app_->player(), SIGNAL(SongChangeRequestProcessed(QUrl,bool)), app_->playlist_manager(), SLOT(SongChangeRequestProcessed(QUrl,bool)));

  connect(app_->player(), SIGNAL(Paused()), SLOT(MediaPaused()));
  connect(app_->player(), SIGNAL(Playing()), SLOT(MediaPlaying()));
  connect(app_->player(), SIGNAL(Stopped()), SLOT(MediaStopped()));
  connect(app_->player(), SIGNAL(Seeked(qlonglong)), SLOT(Seeked(qlonglong)));
  connect(app_->player(), SIGNAL(TrackSkipped(PlaylistItemPtr)), SLOT(TrackSkipped(PlaylistItemPtr)));
  connect(app_->player(), SIGNAL(VolumeChanged(int)), SLOT(VolumeChanged(int)));

  connect(app_->player(), SIGNAL(Paused()), ui_->playlist, SLOT(ActivePaused()));
  connect(app_->player(), SIGNAL(Playing()), ui_->playlist, SLOT(ActivePlaying()));
  connect(app_->player(), SIGNAL(Stopped()), ui_->playlist, SLOT(ActiveStopped()));

  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), osd_, SLOT(SongChanged(Song)));
  connect(app_->player(), SIGNAL(Paused()), osd_, SLOT(Paused()));
  connect(app_->player(), SIGNAL(Resumed()), osd_, SLOT(Resumed()));
  connect(app_->player(), SIGNAL(Stopped()), osd_, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(PlaylistFinished()), osd_, SLOT(PlaylistFinished()));
  connect(app_->player(), SIGNAL(VolumeChanged(int)), osd_, SLOT(VolumeChanged(int)));
  connect(app_->player(), SIGNAL(VolumeChanged(int)), ui_->volume, SLOT(setValue(int)));
  connect(app_->player(), SIGNAL(ForceShowOSD(Song, bool)), SLOT(ForceShowOSD(Song, bool)));

  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), SLOT(SongChanged(Song)));
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), app_->player(), SLOT(CurrentMetadataChanged(Song)));
  connect(app_->playlist_manager(), SIGNAL(EditingFinished(QModelIndex)), SLOT(PlaylistEditFinished(QModelIndex)));
  connect(app_->playlist_manager(), SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  connect(app_->playlist_manager(), SIGNAL(SummaryTextChanged(QString)), ui_->playlist_summary, SLOT(setText(QString)));
  connect(app_->playlist_manager(), SIGNAL(PlayRequested(QModelIndex, Playlist::AutoScroll)), SLOT(PlayIndex(QModelIndex, Playlist::AutoScroll)));

  connect(ui_->playlist->view(), SIGNAL(doubleClicked(QModelIndex)), SLOT(PlaylistDoubleClick(QModelIndex)));
  connect(ui_->playlist->view(), SIGNAL(PlayItem(QModelIndex, Playlist::AutoScroll)), SLOT(PlayIndex(QModelIndex, Playlist::AutoScroll)));
  connect(ui_->playlist->view(), SIGNAL(PlayPause(Playlist::AutoScroll)), app_->player(), SLOT(PlayPause(Playlist::AutoScroll)));
  connect(ui_->playlist->view(), SIGNAL(RightClicked(QPoint, QModelIndex)), SLOT(PlaylistRightClick(QPoint, QModelIndex)));
  connect(ui_->playlist->view(), SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
  connect(ui_->playlist->view(), SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
  connect(ui_->playlist->view(), SIGNAL(BackgroundPropertyChanged()), SLOT(RefreshStyleSheet()));

  connect(ui_->track_slider, SIGNAL(ValueChangedSeconds(int)), app_->player(), SLOT(SeekTo(int)));
  connect(ui_->track_slider, SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
  connect(ui_->track_slider, SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
  connect(ui_->track_slider, SIGNAL(Previous()), app_->player(), SLOT(Previous()));
  connect(ui_->track_slider, SIGNAL(Next()), app_->player(), SLOT(Next()));

  // Collection connections
  connect(collection_view_->view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(collection_view_->view(), SIGNAL(ShowConfigDialog()), SLOT(ShowCollectionConfig()));
  connect(collection_view_->view(), SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  connect(app_->collection_model(), SIGNAL(TotalSongCountUpdated(int)), collection_view_->view(), SLOT(TotalSongCountUpdated(int)));
  connect(app_->collection_model(), SIGNAL(TotalArtistCountUpdated(int)), collection_view_->view(), SLOT(TotalArtistCountUpdated(int)));
  connect(app_->collection_model(), SIGNAL(TotalAlbumCountUpdated(int)), collection_view_->view(), SLOT(TotalAlbumCountUpdated(int)));
  connect(app_->collection_model(), SIGNAL(modelAboutToBeReset()), collection_view_->view(), SLOT(SaveFocus()));
  connect(app_->collection_model(), SIGNAL(modelReset()), collection_view_->view(), SLOT(RestoreFocus()));

  connect(app_->task_manager(), SIGNAL(PauseCollectionWatchers()), app_->collection(), SLOT(PauseWatcher()));
  connect(app_->task_manager(), SIGNAL(ResumeCollectionWatchers()), app_->collection(), SLOT(ResumeWatcher()));

  connect(app_->current_albumcover_loader(), SIGNAL(AlbumCoverLoaded(Song, AlbumCoverLoaderResult)), SLOT(AlbumCoverLoaded(Song, AlbumCoverLoaderResult)));
  connect(album_cover_choice_controller_->cover_from_file_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromFile()));
  connect(album_cover_choice_controller_->cover_to_file_action(), SIGNAL(triggered()), this, SLOT(SaveCoverToFile()));
  connect(album_cover_choice_controller_->cover_from_url_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromURL()));
  connect(album_cover_choice_controller_->search_for_cover_action(), SIGNAL(triggered()), this, SLOT(SearchForCover()));
  connect(album_cover_choice_controller_->unset_cover_action(), SIGNAL(triggered()), this, SLOT(UnsetCover()));
  connect(album_cover_choice_controller_->show_cover_action(), SIGNAL(triggered()), this, SLOT(ShowCover()));
  connect(album_cover_choice_controller_->search_cover_auto_action(), SIGNAL(triggered()), this, SLOT(SearchCoverAutomatically()));
  connect(album_cover_choice_controller_->search_cover_auto_action(), SIGNAL(toggled(bool)), SLOT(ToggleSearchCoverAuto(bool)));

#ifndef Q_OS_WIN
  // Devices connections
  connect(device_view_->view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
#endif

  // Collection filter widget
  QActionGroup *collection_view_group = new QActionGroup(this);

  collection_show_all_ = collection_view_group->addAction(tr("Show all songs"));
  collection_show_duplicates_ = collection_view_group->addAction(tr("Show only duplicates"));
  collection_show_untagged_ = collection_view_group->addAction(tr("Show only untagged"));

  collection_show_all_->setCheckable(true);
  collection_show_duplicates_->setCheckable(true);
  collection_show_untagged_->setCheckable(true);
  collection_show_all_->setChecked(true);

  connect(collection_view_group, SIGNAL(triggered(QAction*)), SLOT(ChangeCollectionQueryMode(QAction*)));

  QAction *collection_config_action = new QAction(IconLoader::Load("configure"), tr("Configure collection..."), this);
  connect(collection_config_action, SIGNAL(triggered()), SLOT(ShowCollectionConfig()));
  collection_view_->filter()->SetSettingsGroup(CollectionSettingsPage::kSettingsGroup);
  collection_view_->filter()->SetCollectionModel(app_->collection()->model());

  QAction *separator = new QAction(this);
  separator->setSeparator(true);

  collection_view_->filter()->AddMenuAction(collection_show_all_);
  collection_view_->filter()->AddMenuAction(collection_show_duplicates_);
  collection_view_->filter()->AddMenuAction(collection_show_untagged_);
  collection_view_->filter()->AddMenuAction(separator);
  collection_view_->filter()->AddMenuAction(collection_config_action);

#ifdef HAVE_SUBSONIC
  connect(subsonic_view_->view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
#endif

#ifdef HAVE_TIDAL
  connect(tidal_view_->artists_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->albums_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->songs_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->search_view(), SIGNAL(AddToPlaylist(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  if (TidalService *tidalservice = qobject_cast<TidalService*> (app_->internet_services()->ServiceBySource(Song::Source_Tidal)))
    connect(this, SIGNAL(AuthorizationUrlReceived(QUrl)), tidalservice, SLOT(AuthorizationUrlReceived(QUrl)));
#endif

#ifdef HAVE_QOBUZ
  connect(qobuz_view_->artists_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(qobuz_view_->albums_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(qobuz_view_->songs_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(qobuz_view_->search_view(), SIGNAL(AddToPlaylist(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
#endif

  // Playlist menu
  connect(playlist_menu_, SIGNAL(aboutToHide()), SLOT(PlaylistMenuHidden()));
  playlist_play_pause_ = playlist_menu_->addAction(tr("Play"), this, SLOT(PlaylistPlay()));
  playlist_menu_->addAction(ui_->action_stop);
  playlist_stop_after_ = playlist_menu_->addAction(IconLoader::Load("media-playback-stop"), tr("Stop after this track"), this, SLOT(PlaylistStopAfter()));
  playlist_queue_ = playlist_menu_->addAction(IconLoader::Load("go-next"), tr("Toggle queue status"), this, SLOT(PlaylistQueue()));
  playlist_queue_->setShortcut(QKeySequence("Ctrl+D"));
  ui_->playlist->addAction(playlist_queue_);
  playlist_queue_play_next_ = playlist_menu_->addAction(IconLoader::Load("go-next"), tr("Queue selected tracks to play next"), this, SLOT(PlaylistQueuePlayNext()));
  playlist_queue_play_next_->setShortcut(QKeySequence("Ctrl+Shift+D"));
  ui_->playlist->addAction(playlist_queue_play_next_);
  playlist_skip_ = playlist_menu_->addAction(IconLoader::Load("media-skip-forward"), tr("Toggle skip status"), this, SLOT(PlaylistSkip()));
  ui_->playlist->addAction(playlist_skip_);

  playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_remove_from_playlist);
  playlist_undoredo_ = playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_edit_track);
  playlist_menu_->addAction(ui_->action_edit_value);
  playlist_menu_->addAction(ui_->action_renumber_tracks);
  playlist_menu_->addAction(ui_->action_selection_set_value);
#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
  playlist_menu_->addAction(ui_->action_auto_complete_tags);
#endif
  playlist_rescan_songs_ = playlist_menu_->addAction(IconLoader::Load("view-refresh"), tr("Rescan song(s)..."), this, SLOT(RescanSongs()));
  playlist_menu_->addAction(playlist_rescan_songs_);
#ifdef HAVE_GSTREAMER
  playlist_menu_->addAction(ui_->action_add_files_to_transcoder);
#endif
  playlist_menu_->addSeparator();
  playlist_copy_url_ = playlist_menu_->addAction(IconLoader::Load("edit-copy"), tr("Copy URL(s)..."), this, SLOT(PlaylistCopyUrl()));
  playlist_show_in_collection_ = playlist_menu_->addAction(IconLoader::Load("edit-find"), tr("Show in collection..."), this, SLOT(ShowInCollection()));
  playlist_open_in_browser_ = playlist_menu_->addAction(IconLoader::Load("document-open-folder"), tr("Show in file browser..."), this, SLOT(PlaylistOpenInBrowser()));
  playlist_organize_ = playlist_menu_->addAction(IconLoader::Load("edit-copy"), tr("Organize files..."), this, SLOT(PlaylistMoveToCollection()));
  playlist_copy_to_collection_ = playlist_menu_->addAction(IconLoader::Load("edit-copy"), tr("Copy to collection..."), this, SLOT(PlaylistCopyToCollection()));
  playlist_move_to_collection_ = playlist_menu_->addAction(IconLoader::Load("go-jump"), tr("Move to collection..."), this, SLOT(PlaylistMoveToCollection()));
#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  playlist_copy_to_device_ = playlist_menu_->addAction(IconLoader::Load("device"), tr("Copy to device..."), this, SLOT(PlaylistCopyToDevice()));
#endif
  playlist_delete_ = playlist_menu_->addAction(IconLoader::Load("edit-delete"), tr("Delete from disk..."), this, SLOT(PlaylistDelete()));
  playlist_menu_->addSeparator();
  playlistitem_actions_separator_ = playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_clear_playlist);
  playlist_menu_->addAction(ui_->action_shuffle);
  playlist_menu_->addAction(ui_->action_remove_duplicates);
  playlist_menu_->addAction(ui_->action_remove_unavailable);

#ifdef Q_OS_MACOS
  ui_->action_shuffle->setShortcut(QKeySequence());
#endif

  // We have to add the actions on the playlist menu to this QWidget otherwise their shortcut keys don't work
  addActions(playlist_menu_->actions());

  connect(ui_->playlist, SIGNAL(UndoRedoActionsChanged(QAction*, QAction*)), SLOT(PlaylistUndoRedoChanged(QAction*, QAction*)));

#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  playlist_copy_to_device_->setDisabled(app_->device_manager()->connected_devices_model()->rowCount() == 0);
  connect(app_->device_manager()->connected_devices_model(), SIGNAL(IsEmptyChanged(bool)), playlist_copy_to_device_, SLOT(setDisabled(bool)));
#endif

  connect(app_->scrobbler(), SIGNAL(ScrobblingEnabledChanged(bool)), SLOT(ScrobblingEnabledChanged(bool)));
  connect(app_->scrobbler(), SIGNAL(ScrobbleButtonVisibilityChanged(bool)), SLOT(ScrobbleButtonVisibilityChanged(bool)));
  connect(app_->scrobbler(), SIGNAL(LoveButtonVisibilityChanged(bool)), SLOT(LoveButtonVisibilityChanged(bool)));

#ifdef Q_OS_MACOS
  mac::SetApplicationHandler(this);
#endif
  // Tray icon
  if (tray_icon_) {
    tray_icon_->SetupMenu(ui_->action_previous_track, ui_->action_play_pause, ui_->action_stop, ui_->action_stop_after_this_track, ui_->action_next_track, ui_->action_mute, ui_->action_love, ui_->action_quit);
    connect(tray_icon_, SIGNAL(PlayPause()), app_->player(), SLOT(PlayPause()));
    connect(tray_icon_, SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
    connect(tray_icon_, SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
    connect(tray_icon_, SIGNAL(NextTrack()), app_->player(), SLOT(Next()));
    connect(tray_icon_, SIGNAL(PreviousTrack()), app_->player(), SLOT(Previous()));
    connect(tray_icon_, SIGNAL(ShowHide()), SLOT(ToggleShowHide()));
    connect(tray_icon_, SIGNAL(ChangeVolume(int)), SLOT(VolumeWheelEvent(int)));
  }

  // Windows 7 thumbbar buttons
#ifdef Q_OS_WIN
  thumbbar_->SetActions(QList<QAction*>() << ui_->action_previous_track << ui_->action_play_pause << ui_->action_stop << ui_->action_next_track << nullptr << ui_->action_love);
#endif

#if defined(HAVE_SPARKLE) || defined(HAVE_QTSPARKLE)
  QAction *check_updates = ui_->menu_tools->addAction(tr("Check for updates..."));
  check_updates->setMenuRole(QAction::ApplicationSpecificRole);
  connect(check_updates, SIGNAL(triggered(bool)), SLOT(CheckForUpdates()));
#endif

#ifdef HAVE_GLOBALSHORTCUTS
  // Global shortcuts
  connect(global_shortcuts_, SIGNAL(Play()), app_->player(), SLOT(Play()));
  connect(global_shortcuts_, SIGNAL(Pause()), app_->player(), SLOT(Pause()));
  connect(global_shortcuts_, SIGNAL(PlayPause()), ui_->action_play_pause, SLOT(trigger()));
  connect(global_shortcuts_, SIGNAL(Stop()), ui_->action_stop, SLOT(trigger()));
  connect(global_shortcuts_, SIGNAL(StopAfter()), ui_->action_stop_after_this_track, SLOT(trigger()));
  connect(global_shortcuts_, SIGNAL(Next()), ui_->action_next_track, SLOT(trigger()));
  connect(global_shortcuts_, SIGNAL(Previous()), ui_->action_previous_track, SLOT(trigger()));
  connect(global_shortcuts_, SIGNAL(IncVolume()), app_->player(), SLOT(VolumeUp()));
  connect(global_shortcuts_, SIGNAL(DecVolume()), app_->player(), SLOT(VolumeDown()));
  connect(global_shortcuts_, SIGNAL(Mute()), app_->player(), SLOT(Mute()));
  connect(global_shortcuts_, SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
  connect(global_shortcuts_, SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
  connect(global_shortcuts_, SIGNAL(ShowHide()), SLOT(ToggleShowHide()));
  connect(global_shortcuts_, SIGNAL(ShowOSD()), app_->player(), SLOT(ShowOSD()));
  connect(global_shortcuts_, SIGNAL(TogglePrettyOSD()), app_->player(), SLOT(TogglePrettyOSD()));
  connect(global_shortcuts_, SIGNAL(ToggleScrobbling()), app_->scrobbler(), SLOT(ToggleScrobbling()));
  connect(global_shortcuts_, SIGNAL(Love()), app_->scrobbler(), SLOT(Love()));
#endif

  // Fancy tabs
  connect(ui_->tabs, SIGNAL(CurrentChanged(int)), SLOT(TabSwitched()));

  // Context
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), context_view_, SLOT(SongChanged(Song)));
  connect(app_->playlist_manager(), SIGNAL(SongMetadataChanged(Song)), context_view_, SLOT(SongChanged(Song)));
  connect(app_->player(), SIGNAL(PlaylistFinished()), context_view_, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Playing()), context_view_, SLOT(Playing()));
  connect(app_->player(), SIGNAL(Stopped()), context_view_, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Error()), context_view_, SLOT(Error()));
  connect(this, SIGNAL(AlbumCoverReady(Song, QImage)), context_view_, SLOT(AlbumCoverLoaded(Song, QImage)));
  connect(this, SIGNAL(SearchCoverInProgress()), context_view_->album_widget(), SLOT(SearchCoverInProgress()));
  connect(context_view_, SIGNAL(AlbumEnabledChanged()), SLOT(TabSwitched()));
  connect(context_view_->albums_widget(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));

  // Analyzer
  connect(ui_->analyzer, SIGNAL(WheelEvent(int)), SLOT(VolumeWheelEvent(int)));

  // Statusbar widgets
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
  ui_->playlist_summary->setMinimumWidth(QFontMetrics(font()).horizontalAdvance("WW selected of WW tracks - [ WW:WW ]"));
#else
  ui_->playlist_summary->setMinimumWidth(QFontMetrics(font()).width("WW selected of WW tracks - [ WW:WW ]"));
#endif
  ui_->status_bar_stack->setCurrentWidget(ui_->playlist_summary_page);
  connect(ui_->multi_loading_indicator, SIGNAL(TaskCountChange(int)), SLOT(TaskCountChanged(int)));

  ui_->track_slider->SetApplication(app);

#ifdef HAVE_MOODBAR
  // Moodbar connections
  connect(app_->moodbar_controller(), SIGNAL(CurrentMoodbarDataChanged(QByteArray)), ui_->track_slider->moodbar_style(), SLOT(SetMoodbarData(QByteArray)));
#endif

  // Playing widget
  qLog(Debug) << "Creating playing widget";
  ui_->widget_playing->set_ideal_height(ui_->status_bar->sizeHint().height() + ui_->player_controls->sizeHint().height());
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), ui_->widget_playing, SLOT(SongChanged(Song)));
  connect(app_->player(), SIGNAL(PlaylistFinished()), ui_->widget_playing, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Playing()), ui_->widget_playing, SLOT(Playing()));
  connect(app_->player(), SIGNAL(Stopped()), ui_->widget_playing, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Error()), ui_->widget_playing, SLOT(Error()));
  connect(ui_->widget_playing, SIGNAL(ShowAboveStatusBarChanged(bool)), SLOT(PlayingWidgetPositionChanged(bool)));
  connect(this, SIGNAL(AlbumCoverReady(Song, QImage)), ui_->widget_playing, SLOT(AlbumCoverLoaded(Song, QImage)));
  connect(this, SIGNAL(SearchCoverInProgress()), ui_->widget_playing, SLOT(SearchCoverInProgress()));

  connect(ui_->action_console, SIGNAL(triggered()), SLOT(ShowConsole()));
  PlayingWidgetPositionChanged(ui_->widget_playing->show_above_status_bar());

  // Load theme
  // This is tricky: we need to save the default/system palette now,
  // before loading user preferred theme (which will override it), to be able to restore it later
  const_cast<QPalette&>(Appearance::kDefaultPalette) = QApplication::palette();
  app_->appearance()->LoadUserTheme();
  StyleSheetLoader *css_loader = new StyleSheetLoader(this);
  css_loader->SetStyleSheet(this, ":/style/strawberry.css");

  // Load playlists
  app_->playlist_manager()->Init(app_->collection_backend(), app_->playlist_backend(), ui_->playlist_sequence, ui_->playlist);

  queue_view_->SetPlaylistManager(app_->playlist_manager());

  // This connection must be done after the playlists have been initialized.
  connect(this, SIGNAL(StopAfterToggled(bool)), osd_, SLOT(StopAfterToggle(bool)));

  // We need to connect these global shortcuts here after the playlist have been initialized
#ifdef HAVE_GLOBALSHORTCUTS
  connect(global_shortcuts_, SIGNAL(CycleShuffleMode()), app_->playlist_manager()->sequence(), SLOT(CycleShuffleMode()));
  connect(global_shortcuts_, SIGNAL(CycleRepeatMode()), app_->playlist_manager()->sequence(), SLOT(CycleRepeatMode()));
#endif
  connect(app_->playlist_manager()->sequence(), SIGNAL(RepeatModeChanged(PlaylistSequence::RepeatMode)), osd_, SLOT(RepeatModeChanged(PlaylistSequence::RepeatMode)));
  connect(app_->playlist_manager()->sequence(), SIGNAL(ShuffleModeChanged(PlaylistSequence::ShuffleMode)), osd_, SLOT(ShuffleModeChanged(PlaylistSequence::ShuffleMode)));

  // Smart playlists
  connect(smartplaylists_view_, SIGNAL(AddToPlaylist(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));

  ScrobbleButtonVisibilityChanged(app_->scrobbler()->ScrobbleButton());
  LoveButtonVisibilityChanged(app_->scrobbler()->LoveButton());
  ScrobblingEnabledChanged(app_->scrobbler()->IsEnabled());

  // Last.fm ImportData
  connect(app_->lastfm_import(), SIGNAL(Finished()), lastfm_import_dialog_, SLOT(Finished()));
  connect(app_->lastfm_import(), SIGNAL(FinishedWithError(QString)), lastfm_import_dialog_, SLOT(FinishedWithError(QString)));
  connect(app_->lastfm_import(), SIGNAL(UpdateTotal(int, int)), lastfm_import_dialog_, SLOT(UpdateTotal(int, int)));
  connect(app_->lastfm_import(), SIGNAL(UpdateProgress(int, int)), lastfm_import_dialog_, SLOT(UpdateProgress(int, int)));

  // Load settings
  qLog(Debug) << "Loading settings";
  settings_.beginGroup(kSettingsGroup);

  // Set last used geometry to position window on the correct monitor
  // Set window state only if the window was last maximized
  if (settings_.contains("geometry")) {
    restoreGeometry(settings_.value("geometry").toByteArray());
  }

  if (!settings_.contains("splitter_state") || !ui_->splitter->restoreState(settings_.value("splitter_state").toByteArray())) {
    ui_->splitter->setSizes(QList<int>() << 20 << (width() - 20));
  }

  ui_->tabs->setCurrentIndex(settings_.value("current_tab", 1).toInt());
  FancyTabWidget::Mode default_mode = FancyTabWidget::Mode_LargeSidebar;
  int tab_mode_int = settings_.value("tab_mode", default_mode).toInt();
  FancyTabWidget::Mode tab_mode = FancyTabWidget::Mode(tab_mode_int);
  if (tab_mode == FancyTabWidget::Mode_None) tab_mode = default_mode;
  ui_->tabs->SetMode(tab_mode);

  TabSwitched();

  file_view_->SetPath(settings_.value("file_path", QDir::homePath()).toString());

  // Users often collapse one side of the splitter by mistake and don't know how to restore it. This must be set after the state is restored above.
  ui_->splitter->setChildrenCollapsible(false);

  ReloadSettings();

  // Reload pretty OSD to avoid issues with fonts
  osd_->ReloadPrettyOSDSettings();

  // Reload playlist settings, for BG and glowing
  ui_->playlist->view()->ReloadSettings();

#ifdef Q_OS_MACOS // Always show the mainwindow on startup for macOS
  show();
#else
  QSettings s;
  s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  BehaviourSettingsPage::StartupBehaviour behaviour = BehaviourSettingsPage::StartupBehaviour(s.value("startupbehaviour", BehaviourSettingsPage::Startup_Remember).toInt());
  s.endGroup();
  switch (behaviour) {
    case BehaviourSettingsPage::Startup_Show:
      show();
      break;
    case BehaviourSettingsPage::Startup_ShowMaximized:
      setWindowState(windowState() | Qt::WindowMaximized);
      show();
      break;
    case BehaviourSettingsPage::Startup_ShowMinimized:
      setWindowState(windowState() | Qt::WindowMinimized);
      show();
      break;
    case BehaviourSettingsPage::Startup_Hide:
      if (QSystemTrayIcon::isSystemTrayAvailable() && tray_icon_ && tray_icon_->IsVisible()) {
        hide();
        break;
      }
      // fallthrough
    case BehaviourSettingsPage::Startup_Remember:
    default: {

      was_maximized_ = settings_.value("maximized", true).toBool();
      if (was_maximized_) setWindowState(windowState() | Qt::WindowMaximized);

      was_minimized_ = settings_.value("minimized", false).toBool();
      if (was_minimized_) setWindowState(windowState() | Qt::WindowMinimized);

      if (!QSystemTrayIcon::isSystemTrayAvailable() || !tray_icon_ || !tray_icon_->IsVisible()) {
        hidden_ = false;
        settings_.setValue("hidden", false);
        show();
      }
      else {
        hidden_ = settings_.value("hidden", false).toBool();
        setVisible(!hidden_);
      }
      break;
    }
  }
#endif

  bool show_sidebar = settings_.value("show_sidebar", true).toBool();
  ui_->sidebar_layout->setVisible(show_sidebar);
  ui_->action_toggle_show_sidebar->setChecked(show_sidebar);

  QShortcut *close_window_shortcut = new QShortcut(this);
  close_window_shortcut->setKey(Qt::CTRL | Qt::Key_W);
  connect(close_window_shortcut, SIGNAL(activated()), SLOT(SetHiddenInTray()));

  CheckFullRescanRevisions();

  CommandlineOptionsReceived(options);

  if (!options.contains_play_options()) {
    LoadPlaybackStatus();
  }
  if (app_->scrobbler()->IsEnabled() && !app_->scrobbler()->IsOffline()) {
    app_->scrobbler()->Submit();
  }

#ifdef HAVE_QTSPARKLE
  QUrl sparkle_url;
#if defined(Q_OS_MACOS)
  sparkle_url.setUrl("https://www.strawberrymusicplayer.org/sparkle-macos");
#elif defined(Q_OS_WIN)
  sparkle_url.setUrl("https://www.strawberrymusicplayer.org/sparkle-windows");
#endif
  if (!sparkle_url.isEmpty()) {
    qLog(Debug) << "Creating Qt Sparkle updater";
    qtsparkle::Updater *updater = new qtsparkle::Updater(sparkle_url, this);
    updater->SetNetworkAccessManager(new NetworkAccessManager(this));
    updater->SetVersion(STRAWBERRY_VERSION_PACKAGE);
    connect(check_updates, SIGNAL(triggered()), updater, SLOT(CheckNow()));
  }
#endif

#ifdef Q_OS_LINUX
  if (!Utilities::GetEnv("SNAP").isEmpty() && !Utilities::GetEnv("SNAP_NAME").isEmpty()) {
    s.beginGroup(kSettingsGroup);
    if (!s.value("ignore_snap", false).toBool()) {
      SnapDialog *snap_dialog = new SnapDialog();
      snap_dialog->setAttribute(Qt::WA_DeleteOnClose);
      snap_dialog->show();
    }
    s.endGroup();
  }
#endif

  qLog(Debug) << "Started" << QThread::currentThread();
  initialized_ = true;

}

MainWindow::~MainWindow() {
  delete ui_;
}

void MainWindow::ReloadSettings() {

  QSettings s;

#ifndef Q_OS_MACOS
  s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  bool showtrayicon = s.value("showtrayicon", QSystemTrayIcon::isSystemTrayAvailable()).toBool();
  s.endGroup();
  if (tray_icon_) tray_icon_->SetVisible(showtrayicon);
  if ((!showtrayicon || !QSystemTrayIcon::isSystemTrayAvailable()) && !isVisible()) show();
#endif

  s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  keep_running_ = s.value("keeprunning", false).toBool();
  playing_widget_ = s.value("playing_widget", true).toBool();
  bool trayicon_progress = s.value("trayicon_progress", false).toBool();
  if (playing_widget_ != ui_->widget_playing->IsEnabled()) TabSwitched();
  doubleclick_addmode_ = BehaviourSettingsPage::AddBehaviour(s.value("doubleclick_addmode", BehaviourSettingsPage::AddBehaviour_Append).toInt());
  doubleclick_playmode_ = BehaviourSettingsPage::PlayBehaviour(s.value("doubleclick_playmode", BehaviourSettingsPage::PlayBehaviour_Never).toInt());
  doubleclick_playlist_addmode_ = BehaviourSettingsPage::PlaylistAddBehaviour(s.value("doubleclick_playlist_addmode", BehaviourSettingsPage::PlayBehaviour_Never).toInt());
  menu_playmode_ = BehaviourSettingsPage::PlayBehaviour(s.value("menu_playmode", BehaviourSettingsPage::PlayBehaviour_Never).toInt());
  s.endGroup();

  s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  int iconsize = s.value(AppearanceSettingsPage::kIconSizePlayControlButtons, 32).toInt();
  s.endGroup();

  if (tray_icon_) tray_icon_->SetTrayiconProgress(trayicon_progress);

  ui_->back_button->setIconSize(QSize(iconsize, iconsize));
  ui_->pause_play_button->setIconSize(QSize(iconsize, iconsize));
  ui_->stop_button->setIconSize(QSize(iconsize, iconsize));
  ui_->forward_button->setIconSize(QSize(iconsize, iconsize));
  ui_->button_love->setIconSize(QSize(iconsize, iconsize));

  s.beginGroup(BackendSettingsPage::kSettingsGroup);
  bool volume_control = s.value("volume_control", true).toBool();
  s.endGroup();
  if (volume_control != ui_->volume->isEnabled()) {
    ui_->volume->SetEnabled(volume_control);
    if (volume_control) {
      if (!ui_->action_mute->isVisible()) ui_->action_mute->setVisible(true);
      if (tray_icon_ && !tray_icon_->MuteEnabled()) tray_icon_->SetMuteEnabled(true);
    }
    else {
      if (ui_->action_mute->isVisible()) ui_->action_mute->setVisible(false);
      if (tray_icon_ && tray_icon_->MuteEnabled()) tray_icon_->SetMuteEnabled(false);
    }
  }

  s.beginGroup(PlaylistSettingsPage::kSettingsGroup);
  delete_files_ = s.value("delete_files", false).toBool();
  s.endGroup();

  osd_->ReloadSettings();

  album_cover_choice_controller_->search_cover_auto_action()->setChecked(settings_.value("search_for_cover_auto", true).toBool());

#ifdef HAVE_SUBSONIC
  s.beginGroup(SubsonicSettingsPage::kSettingsGroup);
  bool enable_subsonic = s.value("enabled", false).toBool();
  s.endGroup();
  if (enable_subsonic)
    ui_->tabs->EnableTab(subsonic_view_);
  else
    ui_->tabs->DisableTab(subsonic_view_);
  app_->scrobbler()->Service<SubsonicScrobbler>()->ReloadSettings();
#endif

#ifdef HAVE_TIDAL
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  bool enable_tidal = s.value("enabled", false).toBool();
  s.endGroup();
  if (enable_tidal)
    ui_->tabs->EnableTab(tidal_view_);
  else
    ui_->tabs->DisableTab(tidal_view_);
#endif

#ifdef HAVE_QOBUZ
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);
  bool enable_qobuz = s.value("enabled", false).toBool();
  s.endGroup();
  if (enable_qobuz)
    ui_->tabs->EnableTab(qobuz_view_);
  else
    ui_->tabs->DisableTab(qobuz_view_);
#endif

  ui_->tabs->ReloadSettings();

}

void MainWindow::ReloadAllSettings() {

  ReloadSettings();

  // Other settings
  app_->ReloadSettings();
  app_->collection()->ReloadSettings();
  app_->player()->ReloadSettings();
  collection_view_->ReloadSettings();
  ui_->playlist->view()->ReloadSettings();
  app_->playlist_manager()->playlist_container()->ReloadSettings();
  app_->album_cover_loader()->ReloadSettings();
  album_cover_choice_controller_->ReloadSettings();
  if (cover_manager_.get()) cover_manager_->ReloadSettings();
  context_view_->ReloadSettings();
  file_view_->ReloadSettings();
  queue_view_->ReloadSettings();
  playlist_list_->ReloadSettings();
  smartplaylists_view_->ReloadSettings();
  app_->cover_providers()->ReloadSettings();
  app_->lyrics_providers()->ReloadSettings();
#ifdef HAVE_SUBSONIC
  subsonic_view_->ReloadSettings();
#endif
#ifdef HAVE_TIDAL
  tidal_view_->ReloadSettings();
#endif
#ifdef HAVE_QOBUZ
  qobuz_view_->ReloadSettings();
#endif

}

void MainWindow::RefreshStyleSheet() {
  QString contents(styleSheet());
  setStyleSheet("");
  setStyleSheet(contents);
}

void MainWindow::SaveSettings() {

  SaveGeometry();
  SavePlaybackStatus();
  ui_->tabs->SaveSettings(kSettingsGroup);
  ui_->playlist->view()->SaveSettings();
  app_->scrobbler()->WriteCache();

  settings_.setValue("show_sidebar", ui_->action_toggle_show_sidebar->isChecked());
  settings_.setValue("search_for_cover_auto", album_cover_choice_controller_->search_cover_auto_action()->isChecked());

}

void MainWindow::Exit() {

  ++exit_count_;

  SaveSettings();

  // Make sure Settings dialog is destroyed first.
  settings_dialog_.reset();

  if (exit_count_ > 1) {
    qApp->quit();
  }
  else {
    if (app_->player()->engine()->is_fadeout_enabled()) {
      // To shut down the application when fadeout will be finished
      connect(app_->player()->engine(), SIGNAL(FadeoutFinishedSignal()), this, SLOT(DoExit()));
      if (app_->player()->GetState() == Engine::Playing) {
        app_->player()->Stop();
        hide();
        if (tray_icon_) tray_icon_->SetVisible(false);
        return; // Don't quit the application now: wait for the fadeout finished signal
      }
    }
    DoExit();
  }

}

void MainWindow::DoExit() {

  connect(app_, SIGNAL(ExitFinished()), this, SLOT(ExitFinished()));
  app_->Exit();

}

void MainWindow::ExitFinished() {

  qApp->quit();

}

void MainWindow::EngineChanged(Engine::EngineType enginetype) {

  ui_->action_equalizer->setEnabled(enginetype == Engine::EngineType::GStreamer);
#ifdef Q_OS_WIN
  ui_->action_open_cd->setEnabled(false);
  ui_->action_open_cd->setVisible(false);
#else
  ui_->action_open_cd->setEnabled(enginetype == Engine::EngineType::GStreamer);
#endif

}

void MainWindow::MediaStopped() {

  setWindowTitle("Strawberry Music Player");

  ui_->action_stop->setEnabled(false);
  ui_->action_stop_after_this_track->setEnabled(false);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-playback-start"));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

  ui_->action_love->setEnabled(false);
  ui_->button_love->setEnabled(false);
  if (tray_icon_) tray_icon_->LoveStateChanged(false);

  track_position_timer_->stop();
  track_slider_timer_->stop();
  ui_->track_slider->SetStopped();
  if (tray_icon_) {
    tray_icon_->SetProgress(0);
    tray_icon_->SetStopped();
  }

  song_playing_ = Song();
  song_ = Song();
  image_original_ = QImage();

  app_->scrobbler()->ClearPlaying();

}

void MainWindow::MediaPaused() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-playback-start"));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

  track_position_timer_->stop();
  track_slider_timer_->stop();

  if (tray_icon_) {
    tray_icon_->SetPaused();
  }

}

void MainWindow::MediaPlaying() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-playback-pause"));
  ui_->action_play_pause->setText(tr("Pause"));

  bool enable_play_pause(false);
  bool can_seek(false);

  PlaylistItemPtr item(app_->player()->GetCurrentItem());
  if (item) {
    enable_play_pause = !(item->options() & PlaylistItem::PauseDisabled);
    can_seek = !(item->options() & PlaylistItem::SeekDisabled);
  }
  ui_->action_play_pause->setEnabled(enable_play_pause);
  ui_->track_slider->SetCanSeek(can_seek);
  if (tray_icon_) tray_icon_->SetPlaying(enable_play_pause);

  track_position_timer_->start();
  track_slider_timer_->start();
  UpdateTrackPosition();

}

void MainWindow::SendNowPlaying() {

  // Send now playing to scrobble services
  Playlist *playlist = app_->playlist_manager()->active();
  if (app_->scrobbler()->IsEnabled() && playlist && playlist->current_item() && playlist->current_item()->Metadata().is_metadata_good()) {
    app_->scrobbler()->UpdateNowPlaying(playlist->current_item()->Metadata());
    ui_->action_love->setEnabled(true);
    ui_->button_love->setEnabled(true);
    if (tray_icon_) tray_icon_->LoveStateChanged(true);
  }

}

void MainWindow::VolumeChanged(const int volume) {
  ui_->action_mute->setChecked(!volume);
  if (tray_icon_) tray_icon_->MuteButtonStateChanged(!volume);
}

void MainWindow::SongChanged(const Song &song) {

  qLog(Debug) << "Song changed to" << song.artist() << song.album() << song.title();

  song_playing_ = song;
  song_ = song;
  setWindowTitle(song.PrettyTitleWithArtist());
  if (tray_icon_) tray_icon_->SetProgress(0);

  SendNowPlaying();

}

void MainWindow::TrackSkipped(PlaylistItemPtr item) {

  // If it was a collection item then we have to increment its skipped count in the database.

  if (item && item->IsLocalCollectionItem() && item->Metadata().id() != -1) {

    Song song = item->Metadata();
    const qint64 position = app_->player()->engine()->position_nanosec();
    const qint64 length = app_->player()->engine()->length_nanosec();
    const float percentage = (length == 0 ? 1 : float(position) / length);

    const qint64 seconds_left = (length - position) / kNsecPerSec;
    const qint64 seconds_total = length / kNsecPerSec;

    if (((0.05 * seconds_total > 60 && percentage < 0.98) || percentage < 0.95) && seconds_left > 5) {  // Never count the skip if under 5 seconds left
      app_->collection_backend()->IncrementSkipCountAsync(song.id(), percentage);
    }
  }

}

void MainWindow::TabSwitched() {

  if (playing_widget_ && ui_->action_toggle_show_sidebar->isChecked() && (ui_->tabs->currentIndex() != ui_->tabs->IndexOfTab(context_view_) || !context_view_->album_enabled())) {
    ui_->widget_playing->SetEnabled();
  }
  else {
    ui_->widget_playing->SetDisabled();
  }

}

void MainWindow::ToggleSidebar(const bool checked) {

  ui_->sidebar_layout->setVisible(checked);
  TabSwitched();
  settings_.setValue("show_sidebar", checked);

}

void MainWindow::ToggleSearchCoverAuto(const bool checked) {
  settings_.setValue("search_for_cover_auto", checked);
}

void MainWindow::SaveGeometry() {

  if (!initialized_) return;

  settings_.setValue("maximized", isMaximized());
  settings_.setValue("minimized", isMinimized());
  settings_.setValue("hidden", hidden_);
  settings_.setValue("geometry", saveGeometry());
  settings_.setValue("splitter_state", ui_->splitter->saveState());

}

void MainWindow::SavePlaybackStatus() {

  QSettings s;

  s.beginGroup(Player::kSettingsGroup);
  s.setValue("playback_state", app_->player()->GetState());
  if (app_->player()->GetState() == Engine::Playing || app_->player()->GetState() == Engine::Paused) {
    s.setValue("playback_playlist", app_->playlist_manager()->active()->id());
    s.setValue("playback_position", app_->player()->engine()->position_nanosec() / kNsecPerSec);
  }
  else {
    s.setValue("playback_playlist", -1);
    s.setValue("playback_position", 0);
  }

  s.endGroup();

}

void MainWindow::LoadPlaybackStatus() {

  QSettings s;

  s.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  bool resume_playback = s.value("resumeplayback", false).toBool();
  s.endGroup();

  s.beginGroup(Player::kSettingsGroup);
  Engine::State playback_state = static_cast<Engine::State> (s.value("playback_state", Engine::Empty).toInt());
  s.endGroup();

  if (resume_playback && playback_state != Engine::Empty && playback_state != Engine::Idle) {
    connect(app_->playlist_manager(), SIGNAL(AllPlaylistsLoaded()), SLOT(ResumePlayback()));
  }

}

void MainWindow::ResumePlayback() {

  qLog(Debug) << "Resuming playback";

  disconnect(app_->playlist_manager(), SIGNAL(AllPlaylistsLoaded()), this, SLOT(ResumePlayback()));

  QSettings s;
  s.beginGroup(Player::kSettingsGroup);
  Engine::State playback_state = static_cast<Engine::State> (s.value("playback_state", Engine::Empty).toInt());
  int playback_playlist = s.value("playback_playlist", -1).toInt();
  int playback_position = s.value("playback_position", 0).toInt();
  s.endGroup();

  if (playback_playlist == app_->playlist_manager()->current()->id()) {
    // Set active to current to resume playback on correct playlist.
    app_->playlist_manager()->SetActiveToCurrent();
    if (playback_state == Engine::Paused) {
      NewClosure(app_->player(), SIGNAL(Playing()), app_->player(), SLOT(PlayPause()));
    }
    // Seek after we got song length.
    NewClosure(track_position_timer_, SIGNAL(timeout()), this, SLOT(ResumePlaybackSeek(int)), playback_position);
    app_->player()->Play();
  }

  // Reset saved playback status so we don't resume again from the same position.
  s.beginGroup(Player::kSettingsGroup);
  s.setValue("playback_state", Engine::Empty);
  s.setValue("playback_playlist", -1);
  s.setValue("playback_position", 0);
  s.endGroup();

}

void MainWindow::ResumePlaybackSeek(const int playback_position) {

  if (app_->player()->engine()->length_nanosec() > 0) {
    app_->player()->SeekTo(playback_position);
  }

}

void MainWindow::PlayIndex(const QModelIndex &idx, Playlist::AutoScroll autoscroll) {

  if (!idx.isValid()) return;

  int row = idx.row();
  if (idx.model() == app_->playlist_manager()->current()->proxy()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    row = app_->playlist_manager()->current()->proxy()->mapToSource(idx).row();
  }

  app_->playlist_manager()->SetActiveToCurrent();
  app_->player()->PlayAt(row, Engine::Manual, autoscroll, true);

}

void MainWindow::PlaylistDoubleClick(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  QModelIndex source_idx = idx;
  if (idx.model() == app_->playlist_manager()->current()->proxy()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    source_idx = app_->playlist_manager()->current()->proxy()->mapToSource(idx);
  }

  switch (doubleclick_playlist_addmode_) {
    case BehaviourSettingsPage::PlaylistAddBehaviour_Play:
      app_->playlist_manager()->SetActiveToCurrent();
      app_->player()->PlayAt(source_idx.row(), Engine::Manual, Playlist::AutoScroll_Never, true, true);
      break;

    case BehaviourSettingsPage::PlaylistAddBehaviour_Enqueue:
      app_->playlist_manager()->current()->queue()->ToggleTracks(QModelIndexList() << source_idx);
      if (app_->player()->GetState() != Engine::Playing) {
        app_->playlist_manager()->SetActiveToCurrent();
        app_->player()->PlayAt(app_->playlist_manager()->current()->queue()->TakeNext(), Engine::Manual, Playlist::AutoScroll_Never, true);
      }
      break;
  }

}

void MainWindow::VolumeWheelEvent(const int delta) {
  ui_->volume->setValue(ui_->volume->value() + delta / 30);
}

void MainWindow::ToggleShowHide() {

  if (hidden_) {
    show();
    SetHiddenInTray(false);
  }
  else if (isActiveWindow()) {
    hide();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    SetHiddenInTray(true);
  }
  else if (isMinimized()) {
    hide();
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    SetHiddenInTray(false);
  }
  else if (!isVisible()) {
    show();
    activateWindow();
  }
  else {
    // Window is not hidden but does not have focus; bring it to front.
    activateWindow();
    raise();
  }

}

void MainWindow::StopAfterCurrent() {
  app_->playlist_manager()->current()->StopAfter(app_->playlist_manager()->current()->current_row());
  emit StopAfterToggled(app_->playlist_manager()->active()->stop_after_current());
}

void MainWindow::showEvent(QShowEvent *e) {

  hidden_ = false;

  QMainWindow::showEvent(e);

}

void MainWindow::closeEvent(QCloseEvent *e) {

#ifdef Q_OS_MACOS
  Exit();
#else
  if (!hidden_ && keep_running_ && e->spontaneous() && QSystemTrayIcon::isSystemTrayAvailable()) {
    SetHiddenInTray(true);
  }
  else {
    Exit();
  }
#endif

  QMainWindow::closeEvent(e);

}

void MainWindow::SetHiddenInTray(const bool hidden) {

  hidden_ = hidden;
  settings_.setValue("hidden", hidden_);

  // Some window managers don't remember maximized state between calls to hide() and show(), so we have to remember it ourself.
  if (hidden) {
    was_maximized_ = isMaximized();
    was_minimized_ = isMinimized();
    hide();
  }
  else {
    if (was_minimized_) { showMinimized(); }
    else if (was_maximized_) showMaximized();
    else show();
  }

}

void MainWindow::FilePathChanged(const QString &path) {
  settings_.setValue("file_path", path);
}

void MainWindow::Seeked(const qlonglong microseconds) {

  const int position = microseconds / kUsecPerSec;
  const int length = app_->player()->GetCurrentItem()->Metadata().length_nanosec() / kNsecPerSec;
  if (tray_icon_) tray_icon_->SetProgress(double(position) / length * 100);

}

void MainWindow::UpdateTrackPosition() {

  PlaylistItemPtr item(app_->player()->GetCurrentItem());
  if (!item) return;

  const int length = (item->Metadata().length_nanosec() / kNsecPerSec);
  if (length <= 0) return;
  const int position = std::floor(float(app_->player()->engine()->position_nanosec()) / kNsecPerSec + 0.5);

  // Update the tray icon every 10 seconds
  if (tray_icon_ && position % 10 == 0) tray_icon_->SetProgress(double(position) / length * 100);

  // Send Scrobble
  if (app_->scrobbler()->IsEnabled() && item->Metadata().is_metadata_good()) {
    Playlist *playlist = app_->playlist_manager()->active();
    if (playlist && !playlist->scrobbled()) {
      const int scrobble_point = (playlist->scrobble_point_nanosec() / kNsecPerSec);
      if (position >= scrobble_point) {
        app_->scrobbler()->Scrobble(item->Metadata(), scrobble_point);
        playlist->set_scrobbled(true);
      }
    }
  }

}

void MainWindow::UpdateTrackSliderPosition() {

  PlaylistItemPtr item(app_->player()->GetCurrentItem());

  const int slider_position = std::floor(float(app_->player()->engine()->position_nanosec()) / kNsecPerMsec);
  const int slider_length = app_->player()->engine()->length_nanosec() / kNsecPerMsec;

  // Update the slider
  ui_->track_slider->SetValue(slider_position, slider_length);

}

void MainWindow::ApplyAddBehaviour(const BehaviourSettingsPage::AddBehaviour b, MimeData *mimedata) const {

  switch (b) {
      case BehaviourSettingsPage::AddBehaviour_Append:
      mimedata->clear_first_ = false;
      mimedata->enqueue_now_ = false;
      break;

    case BehaviourSettingsPage::AddBehaviour_Enqueue:
      mimedata->clear_first_ = false;
      mimedata->enqueue_now_ = true;
      break;

    case BehaviourSettingsPage::AddBehaviour_Load:
      mimedata->clear_first_ = true;
      mimedata->enqueue_now_ = false;
      break;

    case BehaviourSettingsPage::AddBehaviour_OpenInNew:
      mimedata->open_in_new_playlist_ = true;
      break;
  }
}

void MainWindow::ApplyPlayBehaviour(const BehaviourSettingsPage::PlayBehaviour b, MimeData *mimedata) const {

  switch (b) {
    case BehaviourSettingsPage::PlayBehaviour_Always:
      mimedata->play_now_ = true;
      break;

    case BehaviourSettingsPage::PlayBehaviour_Never:
      mimedata->play_now_ = false;
      break;

    case BehaviourSettingsPage::PlayBehaviour_IfStopped:
      mimedata->play_now_ = !(app_->player()->GetState() == Engine::Playing);
      break;
  }
}

void MainWindow::AddToPlaylist(QMimeData *q_mimedata) {

  if (!q_mimedata) return;

  if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
    // Should we replace the flags with the user's preference?
    if (mimedata->override_user_settings_) {
      // Do nothing
    }
    else if (mimedata->from_doubleclick_) {
      ApplyAddBehaviour(doubleclick_addmode_, mimedata);
      ApplyPlayBehaviour(doubleclick_playmode_, mimedata);
    }
    else {
      ApplyPlayBehaviour(menu_playmode_, mimedata);
    }

    // Should we create a new playlist for the songs?
    if (mimedata->open_in_new_playlist_) {
      app_->playlist_manager()->New(mimedata->get_name_for_new_playlist());
    }
  }
  app_->playlist_manager()->current()->dropMimeData(q_mimedata, Qt::CopyAction, -1, 0, QModelIndex());
  delete q_mimedata;

}

void MainWindow::AddToPlaylist(QAction *action) {

  const int destination = action->data().toInt();
  PlaylistItemList items;
  SongList songs;

  // Get the selected playlist items
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    items << item;
    songs << item->Metadata();
  }

  // We're creating a new playlist
  if (destination == -1) {
    // Save the current playlist to reactivate it
    const int current_id = app_->playlist_manager()->current_id();
    // Get the name from selection
    app_->playlist_manager()->New(app_->playlist_manager()->GetNameForNewPlaylist(songs));
    if (app_->playlist_manager()->current()->id() != current_id) {
      // I'm sure the new playlist was created and is selected, so I can just insert items
      app_->playlist_manager()->current()->InsertItems(items);
      // Set back the current playlist
      app_->playlist_manager()->SetCurrentPlaylist(current_id);
    }
  }
  else {
    // We're inserting in a existing playlist
    app_->playlist_manager()->playlist(destination)->InsertItems(items);
  }

}

void MainWindow::PlaylistMenuHidden() {

  playlist_queue_->setVisible(true);
  playlist_queue_play_next_->setVisible(true);
  playlist_skip_->setVisible(true);

}

void MainWindow::PlaylistRightClick(const QPoint &global_pos, const QModelIndex &index) {

  QModelIndex source_index = index;
  if (index.model() == app_->playlist_manager()->current()->proxy()) {
    source_index = app_->playlist_manager()->current()->proxy()->mapToSource(index);
  }

  playlist_menu_index_ = source_index;

  // Is this song currently playing?
  if (app_->playlist_manager()->current()->current_row() == source_index.row() && app_->player()->GetState() == Engine::Playing) {
    playlist_play_pause_->setText(tr("Pause"));
    playlist_play_pause_->setIcon(IconLoader::Load("media-playback-pause"));
  }
  else {
    playlist_play_pause_->setText(tr("Play"));
    playlist_play_pause_->setIcon(IconLoader::Load("media-playback-start"));
  }

  // Are we allowed to pause?
  if (source_index.isValid()) {
    playlist_play_pause_->setEnabled(app_->playlist_manager()->current()->current_row() != source_index.row() || !(app_->playlist_manager()->current()->item_at(source_index.row())->options() & PlaylistItem::PauseDisabled));
  }
  else {
    playlist_play_pause_->setEnabled(false);
  }

  playlist_stop_after_->setEnabled(source_index.isValid());

  // Are any of the selected songs editable or queued?
  QModelIndexList selection = ui_->playlist->view()->selectionModel()->selectedRows();
  bool cue_selected = false;
  int selected = ui_->playlist->view()->selectionModel()->selectedRows().count();
  int editable = 0;
  int in_queue = 0;
  int not_in_queue = 0;
  int in_skipped = 0;
  int not_in_skipped = 0;
  int local_songs = 0;
  int collection_songs = 0;

  for (const QModelIndex &idx : selection) {

    const QModelIndex src_idx = app_->playlist_manager()->current()->proxy()->mapToSource(idx);
    if (!src_idx.isValid()) continue;

    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(src_idx.row());
    if (!item) continue;

    if (item->Metadata().url().isLocalFile()) ++local_songs;
    if (item->Metadata().source() == Song::Source_Collection) ++collection_songs;

    if (item->Metadata().has_cue()) {
      cue_selected = true;
    }
    else if (item->Metadata().IsEditable()) {
      ++editable;
    }

    if (src_idx.data(Playlist::Role_QueuePosition).toInt() == -1) ++not_in_queue;
    else ++in_queue;

    if (item->GetShouldSkip()) ++in_skipped;
    else ++not_in_skipped;

  }

  // this is available when we have one or many files and at least one of those is not CUE related
  ui_->action_edit_track->setEnabled(editable > 0);
  ui_->action_edit_track->setVisible(editable > 0);
#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
  ui_->action_auto_complete_tags->setEnabled(editable > 0);
  ui_->action_auto_complete_tags->setVisible(editable > 0);
#endif

  playlist_rescan_songs_->setEnabled(editable > 0);
  playlist_rescan_songs_->setVisible(editable > 0);

#ifdef HAVE_GSTREAMER
  ui_->action_add_files_to_transcoder->setEnabled(editable);
  ui_->action_add_files_to_transcoder->setVisible(editable);
#endif

  // the rest of the read / write actions work only when there are no CUEs involved
  if (cue_selected) editable = 0;

  playlist_open_in_browser_->setVisible(selected > 0 && local_songs == selected);

  bool track_column = (index.column() == Playlist::Column_Track);
  ui_->action_renumber_tracks->setVisible(editable >= 2 && track_column);
  ui_->action_selection_set_value->setVisible(editable >= 2 && !track_column);
  ui_->action_edit_value->setVisible(editable > 0);
  ui_->action_remove_from_playlist->setEnabled(selected > 0);
  ui_->action_remove_from_playlist->setVisible(selected > 0);

  playlist_show_in_collection_->setVisible(false);
  playlist_copy_to_collection_->setVisible(false);
  playlist_move_to_collection_->setVisible(false);
#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  playlist_copy_to_device_->setVisible(false);
#endif
  playlist_organize_->setVisible(false);
  playlist_delete_->setVisible(false);

  playlist_copy_url_->setVisible(selected > 0);

  if (selected < 1) {
    playlist_queue_->setVisible(false);
    playlist_queue_play_next_->setVisible(false);
    playlist_skip_->setVisible(false);
  }
  else {
    playlist_queue_->setVisible(true);
    playlist_queue_play_next_->setVisible(true);
    playlist_skip_->setVisible(true);
    if (in_queue == 1 && not_in_queue == 0) playlist_queue_->setText(tr("Dequeue track"));
    else if (in_queue > 1 && not_in_queue == 0) playlist_queue_->setText(tr("Dequeue selected tracks"));
    else if (in_queue == 0 && not_in_queue == 1) playlist_queue_->setText(tr("Queue track"));
    else if (in_queue == 0 && not_in_queue > 1) playlist_queue_->setText(tr("Queue selected tracks"));
    else playlist_queue_->setText(tr("Toggle queue status"));

    if (selected > 1)
      playlist_queue_play_next_->setText(tr("Queue selected tracks to play next"));
    else
      playlist_queue_play_next_->setText(tr("Queue to play next"));

    if (in_skipped == 1 && not_in_skipped == 0) playlist_skip_->setText(tr("Unskip track"));
    else if (in_skipped > 1 && not_in_skipped == 0) playlist_skip_->setText(tr("Unskip selected tracks"));
    else if (in_skipped == 0 && not_in_skipped == 1) playlist_skip_->setText(tr("Skip track"));
    else if (in_skipped == 0 && not_in_skipped > 1) playlist_skip_->setText(tr("Skip selected tracks"));
    else playlist_skip_->setText(tr("Toggle skip status"));
  }

  if (not_in_queue == 0) playlist_queue_->setIcon(IconLoader::Load("go-previous"));
  else playlist_queue_->setIcon(IconLoader::Load("go-next"));

  if (in_skipped < selected) playlist_skip_->setIcon(IconLoader::Load("media-skip-forward"));
  else playlist_skip_->setIcon(IconLoader::Load("media-playback-start"));


  if (!index.isValid()) {
    ui_->action_selection_set_value->setVisible(false);
    ui_->action_edit_value->setVisible(false);
  }
  else {

    Playlist::Column column = static_cast<Playlist::Column>(index.column());
    bool column_is_editable = Playlist::column_is_editable(column) && editable > 0;

    ui_->action_selection_set_value->setVisible(ui_->action_selection_set_value->isVisible() && column_is_editable);
    ui_->action_edit_value->setVisible(ui_->action_edit_value->isVisible() && column_is_editable);

    QString column_name = Playlist::column_name(column);
    QString column_value = app_->playlist_manager()->current()->data(source_index).toString();
    if (column_value.length() > 25) column_value = column_value.left(25) + "...";

    ui_->action_selection_set_value->setText(tr("Set %1 to \"%2\"...").arg(column_name.toLower()).arg(column_value));
    ui_->action_edit_value->setText(tr("Edit tag \"%1\"...").arg(column_name));

    // Is it a collection item?
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (item && item->IsLocalCollectionItem() && item->Metadata().id() != -1) {
      playlist_organize_->setVisible(editable > 0);
      playlist_show_in_collection_->setVisible(editable > 0);
      playlist_open_in_browser_->setVisible(true);
    }
    else {
      playlist_copy_to_collection_->setVisible(editable > 0);
      playlist_move_to_collection_->setVisible(editable > 0);
    }

#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
    playlist_copy_to_device_->setVisible(editable > 0);
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    playlist_delete_->setVisible(delete_files_ && editable > 0);
#endif

    // Remove old item actions, if any.
    for (QAction *action : playlistitem_actions_) {
      playlist_menu_->removeAction(action);
    }

    // Get the new item actions, and add them
    playlistitem_actions_ = item->actions();
    playlistitem_actions_separator_->setVisible(!playlistitem_actions_.isEmpty());
    playlist_menu_->insertActions(playlistitem_actions_separator_,playlistitem_actions_);
  }

  //if it isn't the first time we right click, we need to remove the menu previously created
  if (playlist_add_to_another_ != nullptr) {
    playlist_menu_->removeAction(playlist_add_to_another_);
    delete playlist_add_to_another_;
    playlist_add_to_another_ = nullptr;
  }

  // Create the playlist submenu if songs are selected.
  if (selected > 0) {
    QMenu *add_to_another_menu = new QMenu(tr("Add to another playlist"), this);
    add_to_another_menu->setIcon(IconLoader::Load("list-add"));

    for (const PlaylistBackend::Playlist &playlist : app_->playlist_backend()->GetAllOpenPlaylists()) {
      // don't add the current playlist
      if (playlist.id != app_->playlist_manager()->current()->id()) {
        QAction *existing_playlist = new QAction(this);
        existing_playlist->setText(playlist.name);
        existing_playlist->setData(playlist.id);
        add_to_another_menu->addAction(existing_playlist);
      }
    }

    add_to_another_menu->addSeparator();
    // add to a new playlist
    QAction *new_playlist = new QAction(this);
    new_playlist->setText(tr("New playlist"));
    new_playlist->setData(-1);  // fake id
    add_to_another_menu->addAction(new_playlist);
    playlist_add_to_another_ = playlist_menu_->insertMenu(ui_->action_remove_from_playlist, add_to_another_menu);

    connect(add_to_another_menu, SIGNAL(triggered(QAction*)), SLOT(AddToPlaylist(QAction*)));

  }

  playlist_menu_->popup(global_pos);

}

void MainWindow::PlaylistPlay() {

  if (app_->playlist_manager()->current()->current_row() == playlist_menu_index_.row()) {
    app_->player()->PlayPause(Playlist::AutoScroll_Never);
  }
  else {
    PlayIndex(playlist_menu_index_, Playlist::AutoScroll_Never);
  }

}

void MainWindow::PlaylistStopAfter() {
  app_->playlist_manager()->current()->StopAfter(playlist_menu_index_.row());
}

void MainWindow::RescanSongs() {

  SongList songs;

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(source_index.row()));
    if (!item) continue;
    if (item->IsLocalCollectionItem()) {
      songs << item->Metadata();
    }
    else if (item->Metadata().source() == Song::Source_LocalFile) {
      item->Reload();
    }
  }

  if (songs.isEmpty()) return;

  app_->collection()->Rescan(songs);

}

void MainWindow::EditTracks() {

  SongList songs;
  PlaylistItemList items;

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(source_index.row()));
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (song.IsEditable()) {
      songs << song;
      items << item;
    }
  }

  if (items.isEmpty()) return;

  edit_tag_dialog_->SetSongs(songs, items);
  edit_tag_dialog_->show();
  edit_tag_dialog_->raise();

}

void MainWindow::EditTagDialogAccepted() {

  for (PlaylistItemPtr item : edit_tag_dialog_->playlist_items()) {
    item->Reload();
  }

  // FIXME: This is really lame but we don't know what rows have changed.
  ui_->playlist->view()->update();

  app_->playlist_manager()->current()->Save();

}

void MainWindow::RenumberTracks() {

  QModelIndexList indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  int track = 1;

  // Get the index list in order
  std::stable_sort(indexes.begin(), indexes.end());

  // if first selected song has a track number set, start from that offset
  if (!indexes.isEmpty()) {
    const Song first_song = app_->playlist_manager()->current()->item_at(indexes[0].row())->OriginalMetadata();
    if (first_song.track() > 0) track = first_song.track();
  }

  for (const QModelIndex &proxy_index : indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (song.IsEditable()) {
      song.set_track(track);
      TagReaderReply *reply = TagReaderClient::Instance()->SaveFile(song.url().toLocalFile(), song);
      NewClosure(reply, SIGNAL(Finished(bool)), this, SLOT(SongSaveComplete(TagReaderReply*, QPersistentModelIndex)),reply, QPersistentModelIndex(source_index));
    }
    ++track;
  }

}

void MainWindow::SongSaveComplete(TagReaderReply *reply, const QPersistentModelIndex &idx) {

  if (reply->is_successful() && idx.isValid()) {
    app_->playlist_manager()->current()->ReloadItems(QList<int>()<< idx.row());
  }
  reply->deleteLater();

}

void MainWindow::SelectionSetValue() {

  Playlist::Column column = static_cast<Playlist::Column>(playlist_menu_index_.column());
  QVariant column_value = app_->playlist_manager()->current()->data(playlist_menu_index_);

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (!song.is_valid() || !song.url().isLocalFile()) continue;
    if (Playlist::set_column_value(song, column, column_value)) {
      TagReaderReply *reply = TagReaderClient::Instance()->SaveFile(song.url().toLocalFile(), song);
      NewClosure(reply, SIGNAL(Finished(bool)), this, SLOT(SongSaveComplete(TagReaderReply*, QPersistentModelIndex)), reply, QPersistentModelIndex(source_index));
    }
  }

}

void MainWindow::EditValue() {

  QModelIndex current = ui_->playlist->view()->currentIndex();
  if (!current.isValid()) return;

  // Edit the last column that was right-clicked on.  If nothing's ever been right clicked then look for the first editable column.
  int column = playlist_menu_index_.column();
  if (column == -1) {
    for (int i = 0; i < ui_->playlist->view()->model()->columnCount(); ++i) {
      if (ui_->playlist->view()->isColumnHidden(i)) continue;
      if (!Playlist::column_is_editable(Playlist::Column(i))) continue;
      column = i;
      break;
    }
  }

  ui_->playlist->view()->edit(current.sibling(current.row(), column));

}

void MainWindow::AddFile() {

  // Last used directory
  QString directory = settings_.value("add_media_path", QDir::currentPath()).toString();

  PlaylistParser parser(app_->collection_backend());

  // Show dialog
  QStringList file_names = QFileDialog::getOpenFileNames(this, tr("Add file"), directory, QString("%1 (%2);;%3;;%4").arg(tr("Music"), FileView::kFileFilter, parser.filters(), tr(kAllFilesFilterSpec)));

  if (file_names.isEmpty()) return;

  // Save last used directory
  settings_.setValue("add_media_path", file_names[0]);

  // Convert to URLs
  QList<QUrl> urls;
  for (const QString &path : file_names) {
    urls << QUrl::fromLocalFile(QFileInfo(path).canonicalFilePath());
  }

  MimeData *mimedata = new MimeData;
  mimedata->setUrls(urls);
  AddToPlaylist(mimedata);

}

void MainWindow::AddFolder() {

  // Last used directory
  QString directory = settings_.value("add_folder_path", QDir::currentPath()).toString();

  // Show dialog
  directory = QFileDialog::getExistingDirectory(this, tr("Add folder"), directory);
  if (directory.isEmpty()) return;

  // Save last used directory
  settings_.setValue("add_folder_path", directory);

  // Add media
  MimeData *mimedata = new MimeData;
  mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(QFileInfo(directory).canonicalFilePath()));
  AddToPlaylist(mimedata);

}

void MainWindow::AddCDTracks() {

  MimeData *mimedata = new MimeData;
  // We are putting empty data, but we specify cdda mimetype to indicate that we want to load audio cd tracks
  mimedata->open_in_new_playlist_ = true;
  mimedata->setData(Playlist::kCddaMimeType, QByteArray());
  AddToPlaylist(mimedata);

}

void MainWindow::AddStream() {
  add_stream_dialog_->show();
  add_stream_dialog_->raise();
}

void MainWindow::AddStreamAccepted() {

  MimeData *mimedata = new MimeData;
  mimedata->setUrls(QList<QUrl>() << add_stream_dialog_->url());
  AddToPlaylist(mimedata);

}

void MainWindow::ShowInCollection() {

  // Show the first valid selected track artist/album in CollectionView

  SongList songs;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (item && item->IsLocalCollectionItem()) {
      songs << item->OriginalMetadata();
      break;
    }
  }
  QString search;
  if (!songs.isEmpty()) {
    search = "artist:" + songs.first().artist() + " album:" + songs.first().album();
  }
  collection_view_->filter()->ShowInCollection(search);

}

void MainWindow::PlaylistRemoveCurrent() {
  ui_->playlist->view()->RemoveSelected();
}

void MainWindow::PlaylistClearCurrent() {

  if (app_->playlist_manager()->current()->rowCount() > Playlist::kUndoItemLimit) {
    QMessageBox messagebox(QMessageBox::Warning, tr("Clear playlist"), tr("Playlist has %1 songs, too large to undo, are you sure you want to clear the playlist?").arg(app_->playlist_manager()->current()->rowCount()), QMessageBox::Ok|QMessageBox::Cancel);
    messagebox.setTextFormat(Qt::RichText);
    int result = messagebox.exec();
    switch (result) {
    case QMessageBox::Ok:
      break;
    case QMessageBox::Cancel:
    default:
      return;
    }
  }

  app_->playlist_manager()->ClearCurrent();

}

void MainWindow::PlaylistEditFinished(const QModelIndex &idx) {
  if (idx == playlist_menu_index_) SelectionSetValue();
}

void MainWindow::CommandlineOptionsReceived(const quint32 instanceId, const QByteArray &string_options) {

  Q_UNUSED(instanceId);

  CommandlineOptions options;
  options.Load(string_options);

  if (options.is_empty()) {
    raise();
    show();
    activateWindow();
    hidden_ = false;
  }
  else
    CommandlineOptionsReceived(options);

}

void MainWindow::CommandlineOptionsReceived(const CommandlineOptions &options) {

  switch (options.player_action()) {
    case CommandlineOptions::Player_Play:
      if (options.urls().empty()) {
        app_->player()->Play();
      }
      break;
    case CommandlineOptions::Player_PlayPause:
      app_->player()->PlayPause(Playlist::AutoScroll_Maybe);
      break;
    case CommandlineOptions::Player_Pause:
      app_->player()->Pause();
      break;
    case CommandlineOptions::Player_Stop:
      app_->player()->Stop();
      break;
    case CommandlineOptions::Player_StopAfterCurrent:
      app_->player()->StopAfterCurrent();
      break;
    case CommandlineOptions::Player_Previous:
      app_->player()->Previous();
      break;
    case CommandlineOptions::Player_Next:
      app_->player()->Next();
      break;
    case CommandlineOptions::Player_PlayPlaylist:
      if (options.playlist_name().isEmpty()) {
        qLog(Error) << "ERROR: playlist name missing";
      }
      else {
        app_->player()->PlayPlaylist(options.playlist_name());
      }
      break;
    case CommandlineOptions::Player_RestartOrPrevious:
      app_->player()->RestartOrPrevious();
      break;

    case CommandlineOptions::Player_None:
      break;
  }

  if (!options.urls().empty()) {

#ifdef HAVE_TIDAL
    for (const QUrl &url : options.urls()) {
      if (url.scheme() == "tidal" && url.host() == "login") {
        emit AuthorizationUrlReceived(url);
        return;
      }
    }
#endif
    MimeData *mimedata = new MimeData;
    mimedata->setUrls(options.urls());
    // Behaviour depends on command line options, so set it here
    mimedata->override_user_settings_ = true;

    if (options.player_action() == CommandlineOptions::Player_Play) mimedata->play_now_ = true;
    else ApplyPlayBehaviour(doubleclick_playmode_, mimedata);

    switch (options.url_list_action()) {
      case CommandlineOptions::UrlList_Load:
        mimedata->clear_first_ = true;
        break;
      case CommandlineOptions::UrlList_Append:
        // Nothing to do
        break;
      case CommandlineOptions::UrlList_None:
        ApplyAddBehaviour(doubleclick_addmode_, mimedata);
        break;
      case CommandlineOptions::UrlList_CreateNew:
        mimedata->name_for_new_playlist_ = options.playlist_name();
        ApplyAddBehaviour(BehaviourSettingsPage::AddBehaviour_OpenInNew, mimedata);
        break;
    }

    AddToPlaylist(mimedata);
  }

  if (options.set_volume() != -1) app_->player()->SetVolume(options.set_volume());

  if (options.volume_modifier() != 0) {
    app_->player()->SetVolume(app_->player()->GetVolume() +options.volume_modifier());
  }

  if (options.seek_to() != -1) {
    app_->player()->SeekTo(options.seek_to());
  }
  else if (options.seek_by() != 0) {
    app_->player()->SeekTo(app_->player()->engine()->position_nanosec() / kNsecPerSec + options.seek_by());
  }

  if (options.play_track_at() != -1) app_->player()->PlayAt(options.play_track_at(), Engine::Manual, Playlist::AutoScroll_Maybe, true);

  if (options.show_osd()) app_->player()->ShowOSD();

  if (options.toggle_pretty_osd()) app_->player()->TogglePrettyOSD();

}

void MainWindow::ForceShowOSD(const Song &song, const bool toggle) {

  Q_UNUSED(song);

  if (toggle) {
    osd_->SetPrettyOSDToggleMode(toggle);
  }
  osd_->ReshowCurrentSong();

}

void MainWindow::Activate() {
  show();
}

bool MainWindow::LoadUrl(const QString &url) {

  if (!QFile::exists(url)) return false;

  MimeData *mimedata = new MimeData;
  mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(url));
  AddToPlaylist(mimedata);

  return true;

}

void MainWindow::CheckForUpdates() {
#if defined(Q_OS_MACOS)
  mac::CheckForUpdates();
#endif
}

void MainWindow::PlaylistUndoRedoChanged(QAction *undo, QAction *redo) {

  playlist_menu_->insertAction(playlist_undoredo_, undo);
  playlist_menu_->insertAction(playlist_undoredo_, redo);
}

void MainWindow::AddFilesToTranscoder() {

#ifdef HAVE_GSTREAMER

  QStringList filenames;

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(source_index.row()));
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (!song.is_valid() || !song.url().isLocalFile()) continue;
    filenames << song.url().toLocalFile();
  }

  if (filenames.isEmpty()) return;

  transcode_dialog_->SetFilenames(filenames);

  ShowTranscodeDialog();

#endif

}

void MainWindow::ShowCollectionConfig() {
  settings_dialog_->OpenAtPage(SettingsDialog::Page_Collection);
}

void MainWindow::TaskCountChanged(const int count) {

  if (count == 0) {
    ui_->status_bar_stack->setCurrentWidget(ui_->playlist_summary_page);
  }
  else {
    ui_->status_bar_stack->setCurrentWidget(ui_->multi_loading_indicator);
  }

}

void MainWindow::PlayingWidgetPositionChanged(const bool above_status_bar) {

  if (above_status_bar) ui_->status_bar->setParent(ui_->centralWidget);
  else ui_->status_bar->setParent(ui_->player_controls_container);

  ui_->status_bar->parentWidget()->layout()->addWidget(ui_->status_bar);
  ui_->status_bar->show();

}

void MainWindow::CopyFilesToCollection(const QList<QUrl> &urls) {

  organize_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organize_dialog_->SetUrls(urls);
  organize_dialog_->SetCopy(true);
  organize_dialog_->show();
  organize_dialog_->raise();

}

void MainWindow::MoveFilesToCollection(const QList<QUrl> &urls) {

  organize_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organize_dialog_->SetUrls(urls);
  organize_dialog_->SetCopy(false);
  organize_dialog_->show();
  organize_dialog_->raise();

}

void MainWindow::CopyFilesToDevice(const QList<QUrl> &urls) {

#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  if (organize_dialog_->SetUrls(urls)) {
    organize_dialog_->show();
    organize_dialog_->raise();
  }
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }
#else
  Q_UNUSED(urls);
#endif

}

void MainWindow::EditFileTags(const QList<QUrl> &urls) {

  SongList songs;
  for (const QUrl &url : urls) {
    Song song;
    song.set_url(url);
    song.set_valid(true);
    song.set_filetype(Song::FileType_MPEG);
    songs << song;
  }

  edit_tag_dialog_->SetSongs(songs);
  edit_tag_dialog_->show();
  edit_tag_dialog_->raise();

}

void MainWindow::PlaylistCopyToCollection() {
  PlaylistOrganizeSelected(true);
}

void MainWindow::PlaylistMoveToCollection() {
  PlaylistOrganizeSelected(false);
}

void MainWindow::PlaylistOrganizeSelected(const bool copy) {

  SongList songs;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (!song.is_valid() || !song.url().isLocalFile()) continue;
    songs << song;
  }
  if (songs.isEmpty()) return;

  organize_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organize_dialog_->SetSongs(songs);
  organize_dialog_->SetCopy(copy);
  organize_dialog_->show();
  organize_dialog_->raise();

}

void MainWindow::PlaylistOpenInBrowser() {

  QList<QUrl> urls;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    urls << QUrl(source_index.sibling(source_index.row(), Playlist::Column_Filename).data().toString());
  }

  Utilities::OpenInFileBrowser(urls);

}

void MainWindow::PlaylistCopyUrl() {

  QList<QUrl> urls;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    urls << item->StreamUrl();
  }

  if (urls.count() > 0) {
    QMimeData mime_data;
    mime_data.setUrls(urls);
    QApplication::clipboard()->setText(mime_data.text());
  }

}

void MainWindow::PlaylistQueue() {

  QModelIndexList indexes;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    indexes << app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->queue()->ToggleTracks(indexes);

}

void MainWindow::PlaylistQueuePlayNext() {

  QModelIndexList indexes;
  for (const QModelIndex& proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    indexes << app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->queue()->InsertFirst(indexes);

}

void MainWindow::PlaylistSkip() {

  QModelIndexList indexes;

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    indexes << app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->SkipTracks(indexes);

}

void MainWindow::PlaylistCopyToDevice() {

#ifndef Q_OS_WIN

  SongList songs;

  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (!song.is_valid() || !song.url().isLocalFile()) continue;
    songs << song;
  }
  if (songs.isEmpty()) return;

  organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  if (organize_dialog_->SetSongs(songs)) {
    organize_dialog_->show();
    organize_dialog_->raise();
  }
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }

#endif

}

void MainWindow::ChangeCollectionQueryMode(QAction *action) {

  if (action == collection_show_duplicates_) {
    collection_view_->filter()->SetQueryMode(QueryOptions::QueryMode_Duplicates);
  }
  else if (action == collection_show_untagged_) {
    collection_view_->filter()->SetQueryMode(QueryOptions::QueryMode_Untagged);
  }
  else {
    collection_view_->filter()->SetQueryMode(QueryOptions::QueryMode_All);
  }

}

void MainWindow::ShowCoverManager() {

  cover_manager_->show();
  cover_manager_->raise();

}

void MainWindow::ShowEqualizer() {

  equalizer_->show();
  equalizer_->raise();

}

SettingsDialog *MainWindow::CreateSettingsDialog() {

  SettingsDialog *settings_dialog = new SettingsDialog(app_, osd_, this);
#ifdef HAVE_GLOBALSHORTCUTS
  settings_dialog->SetGlobalShortcutManager(global_shortcuts_);
#endif

  // Settings
  connect(settings_dialog, SIGNAL(ReloadSettings()), SLOT(ReloadAllSettings()));

  // Allows custom notification preview
  connect(settings_dialog, SIGNAL(NotificationPreview(OSDBase::Behaviour, QString, QString)), SLOT(HandleNotificationPreview(OSDBase::Behaviour, QString, QString)));
  return settings_dialog;

}

void MainWindow::OpenSettingsDialog() {

  settings_dialog_->show();
  settings_dialog_->raise();

}

void MainWindow::OpenSettingsDialogAtPage(SettingsDialog::Page page) {
  settings_dialog_->OpenAtPage(page);
}

EditTagDialog *MainWindow::CreateEditTagDialog() {

  EditTagDialog *edit_tag_dialog = new EditTagDialog(app_);
  connect(edit_tag_dialog, SIGNAL(accepted()), SLOT(EditTagDialogAccepted()));
  connect(edit_tag_dialog, SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  return edit_tag_dialog;

}

void MainWindow::ShowAboutDialog() {

  about_dialog_->show();
  about_dialog_->raise();

}

void MainWindow::ShowTranscodeDialog() {

#ifdef HAVE_GSTREAMER
  transcode_dialog_->show();
  transcode_dialog_->raise();
#endif

}

void MainWindow::ShowErrorDialog(const QString &message) {
  error_dialog_->ShowMessage(message);
}

void MainWindow::CheckFullRescanRevisions() {

  int from = app_->database()->startup_schema_version();
  int to = app_->database()->current_schema_version();

  // if we're restoring DB from scratch or nothing has changed, do nothing
  if (from == 0 || from == to) {
    return;
  }

  // Collect all reasons
  QSet<QString> reasons;
  for (int i = from ; i <= to ; ++i) {
    QString reason = app_->collection()->full_rescan_reason(i);
    if (!reason.isEmpty()) {
      reasons.insert(reason);
    }
  }

  // if we have any...
  if (!reasons.isEmpty()) {
    QString message = tr("The version of Strawberry you've just updated to requires a full collection rescan because of the new features listed below:") + "<ul>";
    for(const QString &reason : reasons) {
      message += ("<li>" + reason + "</li>");
    }
    message += "</ul>" + tr("Would you like to run a full rescan right now?");
    if (QMessageBox::question(this, tr("Collection rescan notice"), message, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
      app_->collection()->FullScan();
    }
  }
}

void MainWindow::PlaylistViewSelectionModelChanged() {

  connect(ui_->playlist->view()->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), SLOT(PlaylistCurrentChanged(QModelIndex)));

}

void MainWindow::PlaylistCurrentChanged(const QModelIndex &proxy_current) {

  const QModelIndex source_current = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_current);

  // If the user moves the current index using the keyboard and then presses
  // F2, we don't want that editing the last column that was right clicked on.
  if (source_current != playlist_menu_index_) playlist_menu_index_ = QModelIndex();

}

void MainWindow::Raise() {

  show();
  activateWindow();
  hidden_ = false;

}

#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#else
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result) {
#endif

  if (exit_count_ == 0 && message) {
    MSG *msg = static_cast<MSG*>(message);
    thumbbar_->HandleWinEvent(msg);
  }
  return QMainWindow::nativeEvent(eventType, message, result);

}
#endif  // Q_OS_WIN

void MainWindow::AutoCompleteTags() {

#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)

  autocomplete_tag_items_.clear();

  // Create the tag fetching stuff if it hasn't been already
  if (!tag_fetcher_) {
    tag_fetcher_.reset(new TagFetcher);
    track_selection_dialog_.reset(new TrackSelectionDialog);
    track_selection_dialog_->set_save_on_close(true);

    connect(tag_fetcher_.get(), SIGNAL(ResultAvailable(Song, SongList)), track_selection_dialog_.get(), SLOT(FetchTagFinished(Song, SongList)), Qt::QueuedConnection);
    connect(tag_fetcher_.get(), SIGNAL(Progress(Song, QString)), track_selection_dialog_.get(), SLOT(FetchTagProgress(Song, QString)));
    connect(track_selection_dialog_.get(), SIGNAL(accepted()), SLOT(AutoCompleteTagsAccepted()));
    connect(track_selection_dialog_.get(), SIGNAL(finished(int)), tag_fetcher_.get(), SLOT(Cancel()));
    connect(track_selection_dialog_.get(), SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  }

  // Get the selected songs and start fetching tags for them
  SongList songs;
  for (const QModelIndex &proxy_index : ui_->playlist->view()->selectionModel()->selectedRows()) {
    const QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(source_index.row()));
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (song.IsEditable()) {
      songs << song;
      autocomplete_tag_items_ << item;
    }
  }

  if (songs.isEmpty()) return;

  track_selection_dialog_->Init(songs);
  tag_fetcher_->StartFetch(songs);
  track_selection_dialog_->show();
  track_selection_dialog_->raise();

#endif

}

void MainWindow::AutoCompleteTagsAccepted() {

  for (PlaylistItemPtr item : autocomplete_tag_items_) {
    item->Reload();
  }
  autocomplete_tag_items_.clear();

  // This is really lame but we don't know what rows have changed
  ui_->playlist->view()->update();

}

void MainWindow::HandleNotificationPreview(OSDBase::Behaviour type, QString line1, QString line2) {

  if (!app_->playlist_manager()->current()->GetAllSongs().isEmpty()) {
    // Show a preview notification for the first song in the current playlist
    osd_->ShowPreview(type, line1, line2, app_->playlist_manager()->current()->GetAllSongs().first());
  }
  else {
    qLog(Debug) << "The current playlist is empty, showing a fake song";
    // Create a fake song
    Song fake(Song::Source_LocalFile);
    fake.Init("Title", "Artist", "Album", 123);
    fake.set_genre("Classical");
    fake.set_composer("Anonymous");
    fake.set_performer("Anonymous");
    fake.set_track(1);
    fake.set_disc(1);
    fake.set_year(2011);

    osd_->ShowPreview(type, line1, line2, fake);
  }

}

void MainWindow::ShowConsole() {

  console_->show();
  console_->raise();

}

void MainWindow::keyPressEvent(QKeyEvent *e) {

  if (e->key() == Qt::Key_Space) {
    app_->player()->PlayPause(Playlist::AutoScroll_Never);
    e->accept();
  }
  else if (e->key() == Qt::Key_Left) {
    ui_->track_slider->Seek(-1);
    e->accept();
  }
  else if (e->key() == Qt::Key_Right) {
    ui_->track_slider->Seek(1);
    e->accept();
  }
  else {
    QMainWindow::keyPressEvent(e);
  }

}

void MainWindow::LoadCoverFromFile() {
  album_cover_choice_controller_->LoadCoverFromFile(&song_);
}

void MainWindow::LoadCoverFromURL() {
  album_cover_choice_controller_->LoadCoverFromURL(&song_);
}

void MainWindow::SearchForCover() {
  album_cover_choice_controller_->SearchForCover(&song_);
}

void MainWindow::SaveCoverToFile() {
  album_cover_choice_controller_->SaveCoverToFileManual(song_, image_original_);
}

void MainWindow::UnsetCover() {
  album_cover_choice_controller_->UnsetCover(&song_);
}

void MainWindow::ShowCover() {
  album_cover_choice_controller_->ShowCover(song_, image_original_);
}

void MainWindow::SearchCoverAutomatically() {

  GetCoverAutomatically();

}

void MainWindow::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  if (song != song_playing_) return;

  song_ = song;
  image_original_ = result.image_original;

  emit AlbumCoverReady(song, result.image_original);

  GetCoverAutomatically();

}

void MainWindow::GetCoverAutomatically() {

  // Search for cover automatically?
  bool search =
                album_cover_choice_controller_->search_cover_auto_action()->isChecked() &&
                !song_.has_manually_unset_cover() &&
                !song_.art_automatic_is_valid() &&
                !song_.art_manual_is_valid() &&
                !song_.effective_albumartist().isEmpty() &&
                !song_.effective_album().isEmpty();

  if (search) {
    emit SearchCoverInProgress();
    album_cover_choice_controller_->SearchCoverAutomatically(song_);
  }

}

void MainWindow::ScrobblingEnabledChanged(const bool value) {
  if (app_->scrobbler()->ScrobbleButton()) SetToggleScrobblingIcon(value);
}

void MainWindow::ScrobbleButtonVisibilityChanged(const bool value) {

  ui_->button_scrobble->setVisible(value);
  ui_->action_toggle_scrobbling->setVisible(value);
  if (value) SetToggleScrobblingIcon(app_->scrobbler()->IsEnabled());

}

void MainWindow::LoveButtonVisibilityChanged(const bool value) {

  if (value)
    ui_->widget_love->show();
  else
    ui_->widget_love->hide();

  if (tray_icon_) tray_icon_->LoveVisibilityChanged(value);

}

void MainWindow::SetToggleScrobblingIcon(const bool value) {

  if (value) {
    if (app_->playlist_manager()->active() && app_->playlist_manager()->active()->scrobbled())
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble", 22));
    else
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble", 22)); // TODO: Create a faint version of the icon
  }
  else {
    ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble-disabled", 22));
  }

}

void MainWindow::Love() {

  app_->scrobbler()->Love();
  ui_->button_love->setEnabled(false);
  ui_->action_love->setEnabled(false);
  if (tray_icon_) tray_icon_->LoveStateChanged(false);

}

void MainWindow::PlaylistDelete() {

  if (!delete_files_) return;

  SongList selected_songs;
  QStringList files;
  bool is_current_item = false;
  for (const QModelIndex &proxy_idx : ui_->playlist->view()->selectionModel()->selectedRows()) {
    QModelIndex source_idx = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_idx);
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_idx.row());
    if (!item || !item->Metadata().url().isLocalFile()) continue;
    selected_songs << item->Metadata();
    files << item->Metadata().url().toLocalFile();
    if (item == app_->player()->GetCurrentItem()) is_current_item = true;
  }
  if (selected_songs.isEmpty()) return;

  if (DeleteConfirmationDialog::warning(files) != QDialogButtonBox::Yes) return;

  if (app_->player()->GetState() == Engine::Playing && app_->playlist_manager()->current()->rowCount() == selected_songs.count()) {
    app_->player()->Stop();
  }

  ui_->playlist->view()->RemoveSelected();

  if (app_->player()->GetState() == Engine::Playing && is_current_item) {
    app_->player()->Next();
  }

  std::shared_ptr<MusicStorage> storage(new FilesystemMusicStorage("/"));
  DeleteFiles *delete_files = new DeleteFiles(app_->task_manager(), storage, true);
  connect(delete_files, SIGNAL(Finished(SongList)), SLOT(DeleteFinished(SongList)));
  delete_files->Start(selected_songs);

}
