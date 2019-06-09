/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#include <memory>
#include <functional>
#include <cmath>

#include <QMainWindow>
#include <QApplication>
#include <QObject>
#include <QWidget>
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
#include <QtAlgorithms>
#include <QKeySequence>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QShortcut>
#include <QMessageBox>
#include <QtEvents>
#include <QSettings>
#include <QtDebug>

#include "core/logging.h"
#include "core/closure.h"

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
#include "windows7thumbbar.h"
#include "application.h"
#include "database.h"
#include "player.h"
#include "appearance.h"
#include "engine/enginetype.h"
#include "engine/enginebase.h"
#include "dialogs/errordialog.h"
#include "dialogs/about.h"
#include "dialogs/console.h"
#include "dialogs/trackselectiondialog.h"
#include "dialogs/edittagdialog.h"
#include "organise/organisedialog.h"
#include "widgets/fancytabwidget.h"
#include "widgets/playingwidget.h"
#include "widgets/volumeslider.h"
#include "widgets/fileview.h"
#include "widgets/multiloadingindicator.h"
#include "widgets/osd.h"
#include "widgets/trackslider.h"
#include "context/contextview.h"
#include "collection/collectionview.h"
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
#include "covermanager/albumcoverloader.h"
#include "covermanager/currentartloader.h"
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
#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "settings/tidalsettingspage.h"
#endif

#include "internet/internetservices.h"
#include "internet/internetservice.h"
#include "internet/internettabsview.h"

#include "scrobbler/audioscrobbler.h"

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

using std::bind;
using std::floor;
using std::stable_sort;

const char *MainWindow::kSettingsGroup = "MainWindow";
const char *MainWindow::kAllFilesFilterSpec = QT_TR_NOOP("All Files (*)");

namespace {
const int kTrackSliderUpdateTimeMs = 200;
const int kTrackPositionUpdateTimeMs = 1000;
}

MainWindow::MainWindow(Application *app, SystemTrayIcon *tray_icon, OSD *osd, const CommandlineOptions &options, QWidget *parent) :
      QMainWindow(parent),
      ui_(new Ui_MainWindow),
      thumbbar_(new Windows7ThumbBar(this)),
      app_(app),
      tray_icon_(tray_icon),
      osd_(osd),
      edit_tag_dialog_(std::bind(&MainWindow::CreateEditTagDialog, this)),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
#ifdef HAVE_GLOBALSHORTCUTS
      global_shortcuts_(new GlobalShortcuts(this)),
#endif
      context_view_(new ContextView(this)),
      collection_view_(new CollectionViewContainer(this)),
      file_view_(new FileView(this)),
#ifndef Q_OS_WIN
      device_view_container_(new DeviceViewContainer(this)),
      device_view_(device_view_container_->view()),
#endif
      playlist_list_(new PlaylistListContainer(this)),
      queue_view_(new QueueView(this)),
      settings_dialog_(std::bind(&MainWindow::CreateSettingsDialog, this)),
      cover_manager_([=]() {
        AlbumCoverManager *cover_manager = new AlbumCoverManager(app, app->collection_backend());
        cover_manager->Init();

        // Cover manager connections
        connect(cover_manager, SIGNAL(AddToPlaylist(QMimeData*)), this, SLOT(AddToPlaylist(QMimeData*)));
        return cover_manager;
      }),

      //organise_dialog_(new OrganiseDialog(app_->task_manager())),
      equalizer_(new Equalizer),
      organise_dialog_([=]() {
        OrganiseDialog *dialog = new OrganiseDialog(app->task_manager(), app->collection_backend());
        dialog->SetDestinationModel(app->collection()->model()->directory_model());
        return dialog;
      }),
#ifdef HAVE_TIDAL
      tidal_view_(new InternetTabsView(app_, app->internet_services()->ServiceBySource(Song::Source_Tidal), app_->tidal_search(), TidalSettingsPage::kSettingsGroup, SettingsDialog::Page_Tidal, this)),
#endif
      playlist_menu_(new QMenu(this)),
      playlist_add_to_another_(nullptr),
      playlistitem_actions_separator_(nullptr),
      collection_sort_model_(new QSortFilterProxyModel(this)),
      track_position_timer_(new QTimer(this)),
      track_slider_timer_(new QTimer(this)),
      initialised_(false),
      was_maximized_(true),
      playing_widget_(true),
      doubleclick_addmode_(BehaviourSettingsPage::AddBehaviour_Append),
      doubleclick_playmode_(BehaviourSettingsPage::PlayBehaviour_Never),
      menu_playmode_(BehaviourSettingsPage::PlayBehaviour_Never)
      {

  qLog(Debug) << "Starting";

  connect(app, SIGNAL(ErrorAdded(QString)), SLOT(ShowErrorDialog(QString)));
  connect(app, SIGNAL(SettingsDialogRequested(SettingsDialog::Page)), SLOT(OpenSettingsDialogAtPage(SettingsDialog::Page)));

  // Initialise the UI
  ui_->setupUi(this);
#ifdef Q_OS_MACOS
  ui_->menu_help->menuAction()->setVisible(false);
#endif

  connect(app_->current_art_loader(), SIGNAL(ArtLoaded(Song, QString, QImage)), SLOT(AlbumArtLoaded(Song, QString, QImage)));
  album_cover_choice_controller_->SetApplication(app);
  connect(album_cover_choice_controller_->cover_from_file_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromFile()));
  connect(album_cover_choice_controller_->cover_to_file_action(), SIGNAL(triggered()), this, SLOT(SaveCoverToFile()));
  connect(album_cover_choice_controller_->cover_from_url_action(), SIGNAL(triggered()), this, SLOT(LoadCoverFromURL()));
  connect(album_cover_choice_controller_->search_for_cover_action(), SIGNAL(triggered()), this, SLOT(SearchForCover()));
  connect(album_cover_choice_controller_->unset_cover_action(), SIGNAL(triggered()), this, SLOT(UnsetCover()));
  connect(album_cover_choice_controller_->show_cover_action(), SIGNAL(triggered()), this, SLOT(ShowCover()));
  connect(album_cover_choice_controller_->search_cover_auto_action(), SIGNAL(triggered()), this, SLOT(SearchCoverAutomatically()));

  ui_->multi_loading_indicator->SetTaskManager(app_->task_manager());
  context_view_->Init(app_, collection_view_->view(), album_cover_choice_controller_);
  ui_->widget_playing->SetApplication(app_, album_cover_choice_controller_);

  // Initialise the search widget
  StyleHelper::setBaseColor(palette().color(QPalette::Highlight).darker());

  // Add tabs to the fancy tab widget
  ui_->tabs->AddTab(context_view_, "context", IconLoader::Load("strawberry"), tr("Context"));
  ui_->tabs->AddTab(collection_view_, "collection", IconLoader::Load("vinyl"), tr("Collection"));
  ui_->tabs->AddTab(file_view_, "files", IconLoader::Load("document-open"), tr("Files"));
  ui_->tabs->AddTab(playlist_list_, "playlists", IconLoader::Load("view-media-playlist"), tr("Playlists"));
  ui_->tabs->AddTab(queue_view_, "queue", IconLoader::Load("footsteps"), tr("Queue"));
#ifndef Q_OS_WIN
  ui_->tabs->AddTab(device_view_, "devices", IconLoader::Load("device"), tr("Devices"));
#endif
#ifdef HAVE_TIDAL
  ui_->tabs->AddTab(tidal_view_, "tidal", IconLoader::Load("tidal"), tr("Tidal"));
#endif

  // Add the playing widget to the fancy tab widget
  ui_->tabs->addBottomWidget(ui_->widget_playing);
  //ui_->tabs->SetBackgroundPixmap(QPixmap(":/pictures/strawberry-background.png"));
  ui_->tabs->Load(kSettingsGroup);

  track_position_timer_->setInterval(kTrackPositionUpdateTimeMs);
  connect(track_position_timer_, SIGNAL(timeout()), SLOT(UpdateTrackPosition()));
  track_slider_timer_->setInterval(kTrackSliderUpdateTimeMs);
  connect(track_slider_timer_, SIGNAL(timeout()), SLOT(UpdateTrackSliderPosition()));

  // Start initialising the player
  qLog(Debug) << "Initialising player";
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

  ui_->playlist->view()->SetApplication(app_);

  collection_view_->view()->setModel(collection_sort_model_);
  collection_view_->view()->SetApplication(app_);
#ifndef Q_OS_WIN
  device_view_->SetApplication(app_);
#endif
  playlist_list_->SetApplication(app_);

  organise_dialog_->SetDestinationModel(app_->collection()->model()->directory_model());

  // Icons
  qLog(Debug) << "Creating UI";

  // Help menu

  ui_->action_about_strawberry->setIcon(IconLoader::Load("strawberry"));
  ui_->action_about_qt->setIcon(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"));

  // Music menu

  ui_->action_open_file->setIcon(IconLoader::Load("document-open"));
  ui_->action_open_cd->setIcon(IconLoader::Load("cd"));
  ui_->action_previous_track->setIcon(IconLoader::Load("media-rewind"));
  ui_->action_play_pause->setIcon(IconLoader::Load("media-play"));
  ui_->action_stop->setIcon(IconLoader::Load("media-stop"));
  ui_->action_stop_after_this_track->setIcon(IconLoader::Load("media-stop"));
  ui_->action_next_track->setIcon(IconLoader::Load("media-forward"));
  ui_->action_quit->setIcon(IconLoader::Load("application-exit"));

  // Playlist

  ui_->action_add_file->setIcon(IconLoader::Load("document-open"));
  ui_->action_add_folder->setIcon(IconLoader::Load("document-open-folder"));
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

  //ui_->action_remove_from_playlist->setIcon(IconLoader::Load("list-remove"));

  // Configure

  ui_->action_cover_manager->setIcon(IconLoader::Load("document-download"));
  ui_->action_edit_track->setIcon(IconLoader::Load("edit-rename"));
  ui_->action_equalizer->setIcon(IconLoader::Load("equalizer"));
  ui_->action_transcoder->setIcon(IconLoader::Load("tools-wizard"));
  ui_->action_update_collection->setIcon(IconLoader::Load("view-refresh"));
  ui_->action_full_collection_scan->setIcon(IconLoader::Load("view-refresh"));
  ui_->action_settings->setIcon(IconLoader::Load("configure"));

  // Scrobble

  ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble-disabled", 22));

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

  connect(ui_->action_clear_playlist, SIGNAL(triggered()), app_->playlist_manager(), SLOT(ClearCurrent()));
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
  connect(ui_->action_about_strawberry, SIGNAL(triggered()), SLOT(ShowAboutDialog()));
  connect(ui_->action_about_qt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
  connect(ui_->action_shuffle, SIGNAL(triggered()), app_->playlist_manager(), SLOT(ShuffleCurrent()));
  connect(ui_->action_open_file, SIGNAL(triggered()), SLOT(AddFile()));
  connect(ui_->action_open_cd, SIGNAL(triggered()), SLOT(AddCDTracks()));
  connect(ui_->action_add_file, SIGNAL(triggered()), SLOT(AddFile()));
  connect(ui_->action_add_folder, SIGNAL(triggered()), SLOT(AddFolder()));
  connect(ui_->action_cover_manager, SIGNAL(triggered()), SLOT(ShowCoverManager()));
  connect(ui_->action_equalizer, SIGNAL(triggered()), equalizer_.get(), SLOT(show()));
#if defined(HAVE_GSTREAMER)
  connect(ui_->action_transcoder, SIGNAL(triggered()), SLOT(ShowTranscodeDialog()));
#else
  ui_->action_transcoder->setDisabled(true);
#endif
  connect(ui_->action_jump, SIGNAL(triggered()), ui_->playlist->view(), SLOT(JumpToCurrentlyPlayingTrack()));
  connect(ui_->action_update_collection, SIGNAL(triggered()), app_->collection(), SLOT(IncrementalScan()));
  connect(ui_->action_full_collection_scan, SIGNAL(triggered()), app_->collection(), SLOT(FullScan()));
#if defined(HAVE_GSTREAMER)
  connect(ui_->action_add_files_to_transcoder, SIGNAL(triggered()), SLOT(AddFilesToTranscoder()));
#else
  ui_->action_add_files_to_transcoder->setDisabled(true);
#endif

  connect(ui_->action_toggle_scrobbling, SIGNAL(triggered()), app_->scrobbler(), SLOT(ToggleScrobbling()));
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
  connect(this, SIGNAL(IntroPointReached()), app_->player(), SLOT(IntroPointReached()));
  connect(app_->player(), SIGNAL(VolumeChanged(int)), SLOT(VolumeChanged(int)));

  connect(app_->player(), SIGNAL(Paused()), ui_->playlist, SLOT(ActivePaused()));
  connect(app_->player(), SIGNAL(Playing()), ui_->playlist, SLOT(ActivePlaying()));
  connect(app_->player(), SIGNAL(Stopped()), ui_->playlist, SLOT(ActiveStopped()));

  connect(app_->player(), SIGNAL(Paused()), osd_, SLOT(Paused()));
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
  connect(app_->playlist_manager(), SIGNAL(PlayRequested(QModelIndex)), SLOT(PlayIndex(QModelIndex)));

  connect(ui_->playlist->view(), SIGNAL(doubleClicked(QModelIndex)), SLOT(PlaylistDoubleClick(QModelIndex)));
  connect(ui_->playlist->view(), SIGNAL(PlayItem(QModelIndex)), SLOT(PlayIndex(QModelIndex)));
  connect(ui_->playlist->view(), SIGNAL(PlayPause()), app_->player(), SLOT(PlayPause()));
  connect(ui_->playlist->view(), SIGNAL(RightClicked(QPoint,QModelIndex)), SLOT(PlaylistRightClick(QPoint,QModelIndex)));
  connect(ui_->playlist->view(), SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
  connect(ui_->playlist->view(), SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
  connect(ui_->playlist->view(), SIGNAL(BackgroundPropertyChanged()), SLOT(RefreshStyleSheet()));

  connect(ui_->track_slider, SIGNAL(ValueChangedSeconds(int)), app_->player(), SLOT(SeekTo(int)));
  connect(ui_->track_slider, SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
  connect(ui_->track_slider, SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
  connect(ui_->track_slider, SIGNAL(Previous()), app_->player(), SLOT(Previous()));
  connect(ui_->track_slider, SIGNAL(Next()), app_->player(), SLOT(Next()));

  // Context connections

  connect(context_view_->albums(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));

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

#ifndef Q_OS_WIN
  // Devices connections
  connect(device_view_, SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
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
  collection_view_->filter()->SetSettingsGroup(kSettingsGroup);
  collection_view_->filter()->SetCollectionModel(app_->collection()->model());

  QAction *separator = new QAction(this);
  separator->setSeparator(true);

  collection_view_->filter()->AddMenuAction(collection_show_all_);
  collection_view_->filter()->AddMenuAction(collection_show_duplicates_);
  collection_view_->filter()->AddMenuAction(collection_show_untagged_);
  collection_view_->filter()->AddMenuAction(separator);
  collection_view_->filter()->AddMenuAction(collection_config_action);

#ifdef HAVE_TIDAL
  connect(tidal_view_->artists_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->albums_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->songs_collection_view(), SIGNAL(AddToPlaylistSignal(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  connect(tidal_view_->search_view(), SIGNAL(AddToPlaylist(QMimeData*)), SLOT(AddToPlaylist(QMimeData*)));
  TidalService *tidalservice = qobject_cast<TidalService*> (app_->internet_services()->ServiceBySource(Song::Source_Tidal));
  if (tidalservice)
    connect(this, SIGNAL(AuthorisationUrlReceived(const QUrl&)), tidalservice, SLOT(AuthorisationUrlReceived(const QUrl&)));

#endif

  // Playlist menu
  playlist_play_pause_ = playlist_menu_->addAction(tr("Play"), this, SLOT(PlaylistPlay()));
  playlist_menu_->addAction(ui_->action_stop);
  playlist_stop_after_ = playlist_menu_->addAction(IconLoader::Load("media-stop"), tr("Stop after this track"), this, SLOT(PlaylistStopAfter()));
  playlist_queue_ = playlist_menu_->addAction(IconLoader::Load("go-next"), tr("Toggle queue status"), this, SLOT(PlaylistQueue()));
  playlist_queue_->setVisible(false);
  playlist_queue_->setShortcut(QKeySequence("Ctrl+D"));
  ui_->playlist->addAction(playlist_queue_);
  playlist_queue_play_next_ = playlist_menu_->addAction(IconLoader::Load("go-next"), tr("Queue selected tracks to play next"), this, SLOT(PlaylistQueuePlayNext()));
  playlist_queue_play_next_->setShortcut(QKeySequence("Ctrl+Shift+D"));
  playlist_queue_play_next_->setVisible(false);
  ui_->playlist->addAction(playlist_queue_play_next_);
  playlist_skip_ = playlist_menu_->addAction(IconLoader::Load("media-forward"), tr("Toggle skip status"), this, SLOT(PlaylistSkip()));
  playlist_skip_->setVisible(false);
  ui_->playlist->addAction(playlist_skip_);

  playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_remove_from_playlist);
  playlist_undoredo_ = playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_edit_track);
  playlist_menu_->addAction(ui_->action_edit_value);
  playlist_menu_->addAction(ui_->action_renumber_tracks);
  playlist_menu_->addAction(ui_->action_selection_set_value);
  playlist_menu_->addAction(ui_->action_auto_complete_tags);
#ifdef HAVE_GSTREAMER
  playlist_menu_->addAction(ui_->action_add_files_to_transcoder);
#endif
  playlist_menu_->addSeparator();
#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  playlist_copy_to_device_ = playlist_menu_->addAction(IconLoader::Load("device"), tr("Copy to device..."), this, SLOT(PlaylistCopyToDevice()));
#endif
  playlist_copy_to_collection_ = playlist_menu_->addAction(IconLoader::Load("edit-copy"), tr("Copy to collection..."), this, SLOT(PlaylistCopyToCollection()));
  playlist_move_to_collection_ = playlist_menu_->addAction(IconLoader::Load("go-jump"), tr("Move to collection..."), this, SLOT(PlaylistMoveToCollection()));
  playlist_organise_ = playlist_menu_->addAction(IconLoader::Load("edit-copy"), tr("Organise files..."), this, SLOT(PlaylistMoveToCollection()));
  playlist_open_in_browser_ = playlist_menu_->addAction(IconLoader::Load("document-open-folder"), tr("Show in file browser..."), this, SLOT(PlaylistOpenInBrowser()));
  playlist_open_in_browser_->setVisible(false);
  playlist_show_in_collection_ = playlist_menu_->addAction(IconLoader::Load("edit-find"), tr("Show in collection..."), this, SLOT(ShowInCollection()));
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

#ifdef Q_OS_MACOS
  mac::SetApplicationHandler(this);
#endif
  // Tray icon
  if (tray_icon_) {
    tray_icon_->SetupMenu(ui_->action_previous_track, ui_->action_play_pause, ui_->action_stop, ui_->action_stop_after_this_track, ui_->action_next_track, ui_->action_mute, ui_->action_quit);
    connect(tray_icon_, SIGNAL(PlayPause()), app_->player(), SLOT(PlayPause()));
    connect(tray_icon_, SIGNAL(SeekForward()), app_->player(), SLOT(SeekForward()));
    connect(tray_icon_, SIGNAL(SeekBackward()), app_->player(), SLOT(SeekBackward()));
    connect(tray_icon_, SIGNAL(NextTrack()), app_->player(), SLOT(Next()));
    connect(tray_icon_, SIGNAL(PreviousTrack()), app_->player(), SLOT(Previous()));
    connect(tray_icon_, SIGNAL(ShowHide()), SLOT(ToggleShowHide()));
    connect(tray_icon_, SIGNAL(ChangeVolume(int)), SLOT(VolumeWheelEvent(int)));
  }

  // Windows 7 thumbbar buttons
  thumbbar_->SetActions(QList<QAction*>() << ui_->action_previous_track << ui_->action_play_pause << ui_->action_stop << ui_->action_next_track << nullptr); // spacer

#if (defined(Q_OS_MACOS) && defined(HAVE_SPARKLE))
  // Add check for updates item to application menu.
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
#endif

  // Fancy tabs
  connect(ui_->tabs, SIGNAL(CurrentChanged(int)), SLOT(TabSwitched()));

  // Context
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), context_view_, SLOT(SongChanged(Song)));
  connect(app_->player(), SIGNAL(PlaylistFinished()), context_view_, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Playing()), context_view_, SLOT(Playing()));
  connect(app_->player(), SIGNAL(Stopped()), context_view_, SLOT(Stopped()));
  connect(app_->player(), SIGNAL(Error()), context_view_, SLOT(Error()));

  // Analyzer
  connect(ui_->analyzer, SIGNAL(WheelEvent(int)), SLOT(VolumeWheelEvent(int)));

  // Statusbar widgets
  ui_->playlist_summary->setMinimumWidth(QFontMetrics(font()).width("WW selected of WW tracks - [ WW:WW ]"));
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

  connect(ui_->action_console, SIGNAL(triggered()), SLOT(ShowConsole()));
  PlayingWidgetPositionChanged(ui_->widget_playing->show_above_status_bar());

  // Load theme
  // This is tricky: we need to save the default/system palette now,
  // before loading user preferred theme (which will overide it), to be able to restore it later
  const_cast<QPalette&>(Appearance::kDefaultPalette) = QApplication::palette();
  app_->appearance()->LoadUserTheme();
  StyleSheetLoader *css_loader = new StyleSheetLoader(this);
  css_loader->SetStyleSheet(this, ":/style/strawberry.css");
  RefreshStyleSheet();

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

  ScrobbleButtonVisibilityChanged(app_->scrobbler()->ScrobbleButton());
  ScrobblingEnabledChanged(app_->scrobbler()->IsEnabled());

  // Load settings
  qLog(Debug) << "Loading settings";
  settings_.beginGroup(kSettingsGroup);

  // Set last used geometry to position window on the correct monitor
  // Set window state only if the window was last maximized
  was_maximized_ = settings_.value("maximized", true).toBool();

  if (was_maximized_) setWindowState(windowState() | Qt::WindowMaximized);
  else restoreGeometry(settings_.value("geometry").toByteArray());

  if (!ui_->splitter->restoreState(settings_.value("splitter_state").toByteArray())) {
    ui_->splitter->setSizes(QList<int>() << 250 << width() - 250);
  }

  ui_->tabs->setCurrentIndex(settings_.value("current_tab", 1).toInt());
  FancyTabWidget::Mode default_mode = FancyTabWidget::Mode_LargeSidebar;
  int tab_mode_int = settings_.value("tab_mode", default_mode).toInt();
  FancyTabWidget::Mode tab_mode = FancyTabWidget::Mode(tab_mode_int);
  if (tab_mode == FancyTabWidget::Mode_None) tab_mode = default_mode;
  ui_->tabs->SetMode(tab_mode);

  file_view_->SetPath(settings_.value("file_path", QDir::homePath()).toString());

  TabSwitched();

  // Users often collapse one side of the splitter by mistake and don't know how to restore it. This must be set after the state is restored above.
  ui_->splitter->setChildrenCollapsible(false);

  ReloadSettings();

  // Reload pretty OSD to avoid issues with fonts
  osd_->ReloadPrettyOSDSettings();

  // Reload playlist settings, for BG and glowing
  ui_->playlist->view()->ReloadSettings();

#ifdef Q_OS_MACOS // Always show mainwindow on startup if on macos
  show();
#else
  QSettings settings;
  settings.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  StartupBehaviour behaviour = StartupBehaviour(settings.value("startupbehaviour", Startup_Remember).toInt());
  settings.endGroup();
  bool hidden = settings_.value("hidden", false).toBool();
  if (hidden && (!QSystemTrayIcon::isSystemTrayAvailable() || !tray_icon_ || !tray_icon_->IsVisible())) {
    hidden = false;
    settings_.setValue("hidden", false);
    show();
  }
  else {
    switch (behaviour) {
      case Startup_AlwaysHide:
        hide();
        break;
      case Startup_AlwaysShow:
        show();
        break;
      case Startup_Remember:
        setVisible(!hidden);
        break;
    }
  }
#endif

  QShortcut *close_window_shortcut = new QShortcut(this);
  close_window_shortcut->setKey(Qt::CTRL + Qt::Key_W);
  connect(close_window_shortcut, SIGNAL(activated()), SLOT(SetHiddenInTray()));

  CheckFullRescanRevisions();

  CommandlineOptionsReceived(options);

  if (!options.contains_play_options()) {
    LoadPlaybackStatus();
  }

  RefreshStyleSheet();

  qLog(Debug) << "Started";
  initialised_ = true;

  app_->scrobbler()->ConnectError();
  if (app_->scrobbler()->IsEnabled() && !app_->scrobbler()->IsOffline()) app_->scrobbler()->Submit();

}

MainWindow::~MainWindow() {
  delete ui_;
}

void MainWindow::ReloadSettings() {

  QSettings settings;

#ifndef Q_OS_MACOS
  settings.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  bool showtrayicon = settings.value("showtrayicon", QSystemTrayIcon::isSystemTrayAvailable()).toBool();
  settings.endGroup();
  if (tray_icon_) tray_icon_->SetVisible(showtrayicon);
  if ((!showtrayicon || !QSystemTrayIcon::isSystemTrayAvailable()) && !isVisible()) show();
#endif

  settings.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  playing_widget_ = settings.value("playing_widget", true).toBool();
  if (playing_widget_ != ui_->widget_playing->IsEnabled()) TabSwitched();
  doubleclick_addmode_ = BehaviourSettingsPage::AddBehaviour(settings.value("doubleclick_addmode", BehaviourSettingsPage::AddBehaviour_Append).toInt());
  doubleclick_playmode_ = BehaviourSettingsPage::PlayBehaviour(settings.value("doubleclick_playmode", BehaviourSettingsPage::PlayBehaviour_IfStopped).toInt());
  doubleclick_playlist_addmode_ = BehaviourSettingsPage::PlaylistAddBehaviour(settings.value("doubleclick_playlist_addmode", BehaviourSettingsPage::PlaylistAddBehaviour_Play).toInt());
  menu_playmode_ = BehaviourSettingsPage::PlayBehaviour(settings.value("menu_playmode", BehaviourSettingsPage::PlayBehaviour_IfStopped).toInt());
  settings.endGroup();

  settings.beginGroup(kSettingsGroup);
  album_cover_choice_controller_->search_cover_auto_action()->setChecked(settings.value("search_for_cover_auto", true).toBool());
  settings.endGroup();

  settings.beginGroup(BackendSettingsPage::kSettingsGroup);
  bool volume_control = settings.value("volume_control", true).toBool();
  settings.endGroup();
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

#ifdef HAVE_TIDAL
  settings.beginGroup(TidalSettingsPage::kSettingsGroup);
  bool enable_tidal = settings.value("enabled", false).toBool();
  settings.endGroup();
  if (enable_tidal)
    ui_->tabs->EnableTab(tidal_view_);
  else
    ui_->tabs->DisableTab(tidal_view_);
#endif

}

void MainWindow::ReloadAllSettings() {

  ReloadSettings();

  // Other settings
  app_->ReloadSettings();
  app_->collection()->ReloadSettings();
  app_->player()->ReloadSettings();
  osd_->ReloadSettings();
  collection_view_->ReloadSettings();
  ui_->playlist->view()->ReloadSettings();
  album_cover_choice_controller_->ReloadSettings();
  if (cover_manager_.get()) cover_manager_->ReloadSettings();
#ifdef HAVE_TIDAL
  tidal_view_->ReloadSettings();
#endif

}

void MainWindow::RefreshStyleSheet() {
  QString contents(styleSheet());
  setStyleSheet("");
  setStyleSheet(contents);
}

void MainWindow::EngineChanged(Engine::EngineType enginetype) {

  ui_->action_equalizer->setEnabled(enginetype == Engine::EngineType::GStreamer || enginetype == Engine::EngineType::Xine);
  ui_->action_open_cd->setEnabled(enginetype == Engine::EngineType::GStreamer);

}

void MainWindow::MediaStopped() {

  setWindowTitle("Strawberry Music Player");

  ui_->action_stop->setEnabled(false);
  ui_->action_stop_after_this_track->setEnabled(false);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-play"));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

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

}

void MainWindow::MediaPaused() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-play"));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

  track_position_timer_->stop();
  track_slider_timer_->stop();

  if (tray_icon_) tray_icon_->SetPaused();

}

void MainWindow::MediaPlaying() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load("media-pause"));
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

  // Send now playing to scrobble services
  Playlist *playlist = app_->playlist_manager()->active();
  if (app_->scrobbler()->IsEnabled() && playlist && !playlist->nowplaying() && item->Metadata().is_metadata_good() && item->Metadata().length_nanosec() > 0) {
    app_->scrobbler()->UpdateNowPlaying(item->Metadata());
    playlist->set_nowplaying(true);
  }

}

void MainWindow::VolumeChanged(int volume) {
  ui_->action_mute->setChecked(!volume);
  if (tray_icon_) tray_icon_->MuteButtonStateChanged(!volume);
}

void MainWindow::SongChanged(const Song &song) {

  song_playing_ = song;
  song_ = song;
  setWindowTitle(song.PrettyTitleWithArtist());
  if (tray_icon_) tray_icon_->SetProgress(0);

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

  if (playing_widget_ && ui_->tabs->tabBar()->tabData(ui_->tabs->currentIndex()).toString().toLower() != "context") {
    ui_->widget_playing->SetEnabled();
  }
  else {
    ui_->widget_playing->SetDisabled();
  }

}

void MainWindow::SaveGeometry() {

  if (!initialised_) return;

  was_maximized_ = isMaximized();
  settings_.setValue("maximized", was_maximized_);
  if (was_maximized_) settings_.remove("geometry");
  else settings_.setValue("geometry", saveGeometry());
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
    app_->player()->Play();
    app_->player()->SeekTo(playback_position);
  }

  // Reset saved playback status so we don't resume again from the same position.
  s.beginGroup(Player::kSettingsGroup);
  s.setValue("playback_state", Engine::Empty);
  s.setValue("playback_playlist", -1);
  s.setValue("playback_position", 0);
  s.endGroup();

}

void MainWindow::PlayIndex(const QModelIndex &index) {

  if (!index.isValid()) return;

  int row = index.row();
  if (index.model() == app_->playlist_manager()->current()->proxy()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    row = app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
  }

  app_->playlist_manager()->SetActiveToCurrent();
  app_->player()->PlayAt(row, Engine::Manual, true);
}

void MainWindow::PlaylistDoubleClick(const QModelIndex &index) {
  if (!index.isValid()) return;

  int row = index.row();
  if (index.model() == app_->playlist_manager()->current()->proxy()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    row = app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
  }

  QModelIndexList dummyIndexList;

  switch (doubleclick_playlist_addmode_) {
    case BehaviourSettingsPage::PlaylistAddBehaviour_Play:
      app_->playlist_manager()->SetActiveToCurrent();
      app_->player()->PlayAt(row, Engine::Manual, true);
      break;

    case BehaviourSettingsPage::PlaylistAddBehaviour_Enqueue:
      dummyIndexList.append(index);
      app_->playlist_manager()->current()->queue()->ToggleTracks(dummyIndexList);
      if (app_->player()->GetState() != Engine::Playing) {
        app_->player()->PlayAt(app_->playlist_manager()->current()->queue()->TakeNext(), Engine::Manual, true);
      }
      break;
  }
}

void MainWindow::VolumeWheelEvent(int delta) {
  ui_->volume->setValue(ui_->volume->value() + delta / 30);
}

void MainWindow::ToggleShowHide() {

  if (settings_.value("hidden").toBool()) {
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

void MainWindow::closeEvent(QCloseEvent *event) {

  QSettings settings;
  settings.beginGroup(BehaviourSettingsPage::kSettingsGroup);
  bool keep_running = settings.value("keeprunning", false).toBool();
  settings.endGroup();

  if (keep_running && event->spontaneous() && QSystemTrayIcon::isSystemTrayAvailable()) {
    event->ignore();
    SetHiddenInTray(true);
  }
  else {
    Exit();
    QApplication::quit();
  }
}

void MainWindow::SetHiddenInTray(bool hidden) {

  settings_.setValue("hidden", hidden);

  // Some window managers don't remember maximized state between calls to hide() and show(), so we have to remember it ourself.
  if (hidden) {
    //was_maximized_ = isMaximized();
    hide();
  }
  else {
    if (was_maximized_) showMaximized();
    else show();
  }
}

void MainWindow::FilePathChanged(const QString &path) {
  settings_.setValue("file_path", path);
}

void MainWindow::Seeked(qlonglong microseconds) {

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
    if (playlist && playlist->nowplaying() && !playlist->scrobbled()) {
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
  const int slider_length =app_->player()->engine()->length_nanosec() / kNsecPerMsec;

  // Update the slider
  ui_->track_slider->SetValue(slider_position, slider_length);

}

void MainWindow::ApplyAddBehaviour(BehaviourSettingsPage::AddBehaviour b, MimeData *data) const {

  switch (b) {
      case BehaviourSettingsPage::AddBehaviour_Append:
      data->clear_first_ = false;
      data->enqueue_now_ = false;
      break;

    case BehaviourSettingsPage::AddBehaviour_Enqueue:
      data->clear_first_ = false;
      data->enqueue_now_ = true;
      break;

    case BehaviourSettingsPage::AddBehaviour_Load:
      data->clear_first_ = true;
      data->enqueue_now_ = false;
      break;

    case BehaviourSettingsPage::AddBehaviour_OpenInNew:
      data->open_in_new_playlist_ = true;
      break;
  }
}

void MainWindow::ApplyPlayBehaviour(BehaviourSettingsPage::PlayBehaviour b, MimeData *data) const {

  switch (b) {
    case BehaviourSettingsPage::PlayBehaviour_Always:
      data->play_now_ = true;
      break;

    case BehaviourSettingsPage::PlayBehaviour_Never:
      data->play_now_ = false;
      break;

    case BehaviourSettingsPage::PlayBehaviour_IfStopped:
      data->play_now_ = !(app_->player()->GetState() == Engine::Playing);
      break;
  }
}

void MainWindow::AddToPlaylist(QMimeData *data) {

  if (!data) return;

  if (MimeData *mime_data = qobject_cast<MimeData*>(data)) {
    // Should we replace the flags with the user's preference?
    if (mime_data->override_user_settings_) {
      // Do nothing
    }
    else if (mime_data->from_doubleclick_) {
      ApplyAddBehaviour(doubleclick_addmode_, mime_data);
      ApplyPlayBehaviour(doubleclick_playmode_, mime_data);
    }
    else {
      ApplyPlayBehaviour(menu_playmode_, mime_data);
    }

    // Should we create a new playlist for the songs?
    if (mime_data->open_in_new_playlist_) {
      app_->playlist_manager()->New(mime_data->get_name_for_new_playlist());
    }
  }
  app_->playlist_manager()->current()->dropMimeData(data, Qt::CopyAction, -1, 0, QModelIndex());
  delete data;

}

void MainWindow::AddToPlaylist(QAction *action) {

  int destination = action->data().toInt();
  PlaylistItemList items;

  // get the selected playlist items
  for (const QModelIndex &index : ui_->playlist->view()->selectionModel()->selection().indexes()) {
    if (index.column() != 0) continue;
    int row = app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
    items << app_->playlist_manager()->current()->item_at(row);
  }

  SongList songs;
  for (const PlaylistItemPtr item : items) {
    songs << item->Metadata();
  }

  // we're creating a new playlist
  if (destination == -1) {
    // save the current playlist to reactivate it
    int current_id = app_->playlist_manager()->current_id();
    // get the name from selection
    app_->playlist_manager()->New(app_->playlist_manager()->GetNameForNewPlaylist(songs));
    if (app_->playlist_manager()->current()->id() != current_id) {
      //I'm sure the new playlist was created and is selected, so I can just insert items
      app_->playlist_manager()->current()->InsertItems(items);
      // set back the current playlist
      app_->playlist_manager()->SetCurrentPlaylist(current_id);
    }
  }
  else {
    // we're inserting in a existing playlist
    app_->playlist_manager()->playlist(destination)->InsertItems(items);
  }

}

void MainWindow::PlaylistRightClick(const QPoint &global_pos, const QModelIndex &index) {

  QModelIndex source_index = app_->playlist_manager()->current()->proxy()->mapToSource(index);

  playlist_menu_index_ = source_index;

  // Is this song currently playing?
  if (app_->playlist_manager()->current()->current_row() == source_index.row() && app_->player()->GetState() == Engine::Playing) {
    playlist_play_pause_->setText(tr("Pause"));
    playlist_play_pause_->setIcon(IconLoader::Load("media-pause"));
  }
  else {
    playlist_play_pause_->setText(tr("Play"));
    playlist_play_pause_->setIcon(IconLoader::Load("media-play"));
  }

  // Are we allowed to pause?
  if (index.isValid()) {
    playlist_play_pause_->setEnabled(app_->playlist_manager()->current()->current_row() != source_index.row() || !(app_->playlist_manager()->current()->item_at(source_index.row())->options() & PlaylistItem::PauseDisabled));
  }
  else {
    playlist_play_pause_->setEnabled(false);
  }

  playlist_stop_after_->setEnabled(index.isValid());

  // Are any of the selected songs editable or queued?
  QModelIndexList selection = ui_->playlist->view()->selectionModel()->selection().indexes();
  bool cue_selected = false;
  int all = 0;
  int selected = 0;
  int editable = 0;
  int in_queue = 0;
  int not_in_queue = 0;
  int in_skipped = 0;
  int not_in_skipped = 0;

  for (const QModelIndex &index : selection) {

    all++;

    if (index.column() != 0) continue;

    selected++;

    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(index.row());

    if (item->Metadata().has_cue()) {
      cue_selected = true;
    }
    else if (item->Metadata().IsEditable()) {
      editable++;
    }

    if (index.data(Playlist::Role_QueuePosition).toInt() == -1) not_in_queue++;
    else in_queue++;

    if (item->GetShouldSkip()) in_skipped++;
    else not_in_skipped++;
  }

  // this is available when we have one or many files and at least one of those is not CUE related
  ui_->action_edit_track->setEnabled(editable);
  ui_->action_edit_track->setVisible(editable);
#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
  ui_->action_auto_complete_tags->setEnabled(editable);
  ui_->action_auto_complete_tags->setVisible(editable);
#else
  ui_->action_auto_complete_tags->setEnabled(false);
  ui_->action_auto_complete_tags->setVisible(false);
#endif
  // the rest of the read / write actions work only when there are no CUEs involved
  if (cue_selected) editable = 0;

  if (selected > 0) playlist_open_in_browser_->setVisible(true);

  bool track_column = (index.column() == Playlist::Column_Track);
  ui_->action_renumber_tracks->setVisible(editable >= 2 && track_column);
  ui_->action_selection_set_value->setVisible(editable >= 2 && !track_column);
  ui_->action_edit_value->setVisible(editable);
  ui_->action_remove_from_playlist->setEnabled(!selection.isEmpty());

  playlist_show_in_collection_->setVisible(false);
  playlist_copy_to_collection_->setVisible(false);
  playlist_move_to_collection_->setVisible(false);
#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  playlist_copy_to_device_->setVisible(false);
#endif
  playlist_organise_->setVisible(false);
  playlist_open_in_browser_->setVisible(false);

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

  if (in_skipped < selected) playlist_skip_->setIcon(IconLoader::Load("media-forward"));
  else playlist_skip_->setIcon(IconLoader::Load("media-play"));


  if (!index.isValid()) {
    ui_->action_selection_set_value->setVisible(false);
    ui_->action_edit_value->setVisible(false);
  }
  else {


    Playlist::Column column = (Playlist::Column)index.column();
    bool column_is_editable = Playlist::column_is_editable(column) && editable;

    ui_->action_selection_set_value->setVisible(ui_->action_selection_set_value->isVisible() && column_is_editable);
    ui_->action_edit_value->setVisible(ui_->action_edit_value->isVisible() && column_is_editable);

    QString column_name = Playlist::column_name(column);
    QString column_value =app_->playlist_manager()->current()->data(source_index).toString();
    if (column_value.length() > 25) column_value = column_value.left(25) + "...";

    ui_->action_selection_set_value->setText(tr("Set %1 to \"%2\"...").arg(column_name.toLower()).arg(column_value));
    ui_->action_edit_value->setText(tr("Edit tag \"%1\"...").arg(column_name));

    // Is it a collection item?
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (item->IsLocalCollectionItem() && item->Metadata().id() != -1) {
      playlist_organise_->setVisible(editable);
      playlist_show_in_collection_->setVisible(editable);
      playlist_open_in_browser_->setVisible(true);
    }
    else {
      playlist_copy_to_collection_->setVisible(editable);
      playlist_move_to_collection_->setVisible(editable);
    }

#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
    playlist_copy_to_device_->setVisible(editable);
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
  }

  // create the playlist submenu
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

  playlist_menu_->popup(global_pos);
}

void MainWindow::PlaylistPlay() {
  if (app_->playlist_manager()->current()->current_row() ==playlist_menu_index_.row()) {
    app_->player()->PlayPause();
  }
  else {
    PlayIndex(playlist_menu_index_);
  }
}

void MainWindow::PlaylistStopAfter() {
  app_->playlist_manager()->current()->StopAfter(playlist_menu_index_.row());
}

void MainWindow::EditTracks() {

  SongList songs;
  PlaylistItemList items;

  for (const QModelIndex &index : ui_->playlist->view()->selectionModel()->selection().indexes()) {
    if (index.column() != 0) continue;
    int row =app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(row));
    Song song = item->Metadata();

    if (song.IsEditable()) {
      songs << song;
      items << item;
    }
  }

  edit_tag_dialog_->SetSongs(songs, items);
  edit_tag_dialog_->show();

}

void MainWindow::EditTagDialogAccepted() {

  for (const PlaylistItemPtr item : edit_tag_dialog_->playlist_items()) {
    item->Reload();
  }

  // This is really lame but we don't know what rows have changed
  ui_->playlist->view()->update();

  app_->playlist_manager()->current()->Save();

}

void MainWindow::RenumberTracks() {

  QModelIndexList indexes =ui_->playlist->view()->selectionModel()->selection().indexes();
  int track = 1;

  // Get the index list in order
  std::stable_sort(indexes.begin(), indexes.end());

  // if first selected song has a track number set, start from that offset
  if (!indexes.isEmpty()) {
    const Song first_song = app_->playlist_manager()->current()->item_at(indexes[0].row())->Metadata();

    if (first_song.track() > 0) track = first_song.track();
  }

  for (const QModelIndex &index : indexes) {
    if (index.column() != 0) continue;

    const QModelIndex source_index =app_->playlist_manager()->current()->proxy()->mapToSource(index);
    int row = source_index.row();
    Song song = app_->playlist_manager()->current()->item_at(row)->Metadata();

    if (song.IsEditable()) {
      song.set_track(track);

      TagReaderReply *reply = TagReaderClient::Instance()->SaveFile(song.url().toLocalFile(), song);

      NewClosure(reply, SIGNAL(Finished(bool)), this, SLOT(SongSaveComplete(TagReaderReply*, QPersistentModelIndex)),reply, QPersistentModelIndex(source_index));
    }
    track++;
  }

}

void MainWindow::SongSaveComplete(TagReaderReply *reply, const QPersistentModelIndex &index) {
  if (reply->is_successful() && index.isValid()) {
    app_->playlist_manager()->current()->ReloadItems(QList<int>()<< index.row());
  }
  reply->deleteLater();
}

void MainWindow::SelectionSetValue() {

  Playlist::Column column = (Playlist::Column)playlist_menu_index_.column();
  QVariant column_value = app_->playlist_manager()->current()->data(playlist_menu_index_);

  QModelIndexList indexes =ui_->playlist->view()->selectionModel()->selection().indexes();

  for (const QModelIndex &index : indexes) {
    if (index.column() != 0) continue;

    const QModelIndex source_index =app_->playlist_manager()->current()->proxy()->mapToSource(index);
    int row = source_index.row();
    Song song = app_->playlist_manager()->current()->item_at(row)->Metadata();

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
  QString directory =settings_.value("add_media_path", QDir::currentPath()).toString();

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

  MimeData *data = new MimeData;
  data->setUrls(urls);
  AddToPlaylist(data);

}

void MainWindow::AddFolder() {

  // Last used directory
  QString directory =settings_.value("add_folder_path", QDir::currentPath()).toString();

  // Show dialog
  directory =QFileDialog::getExistingDirectory(this, tr("Add folder"), directory);
  if (directory.isEmpty()) return;

  // Save last used directory
  settings_.setValue("add_folder_path", directory);

  // Add media
  MimeData *data = new MimeData;
  data->setUrls(QList<QUrl>() << QUrl::fromLocalFile(QFileInfo(directory).canonicalFilePath()));
  AddToPlaylist(data);

}

void MainWindow::AddCDTracks() {
  MimeData *data = new MimeData;
  // We are putting empty data, but we specify cdda mimetype to indicate that we want to load audio cd tracks
  data->open_in_new_playlist_ = true;
  data->setData(Playlist::kCddaMimeType, QByteArray());
  AddToPlaylist(data);
}

void MainWindow::ShowInCollection() {

  // Show the first valid selected track artist/album in CollectionView
  QModelIndexList proxy_indexes =ui_->playlist->view()->selectionModel()->selectedRows();
  SongList songs;

  for (const QModelIndex &proxy_index : proxy_indexes) {
    QModelIndex index =app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    if (app_->playlist_manager()->current()->item_at(index.row())->IsLocalCollectionItem()) {
      songs << app_->playlist_manager()->current()->item_at(index.row())->Metadata();
      break;
    }
  }
  QString search;
  if (!songs.isEmpty()) {
    search ="artist:" + songs.first().artist() + " album:" + songs.first().album();
  }
  collection_view_->filter()->ShowInCollection(search);
}

void MainWindow::PlaylistRemoveCurrent() {
  ui_->playlist->view()->RemoveSelected(false);
}

void MainWindow::PlaylistEditFinished(const QModelIndex &index) {
  if (index == playlist_menu_index_) SelectionSetValue();
}

void MainWindow::CommandlineOptionsReceived(const quint32 instanceId, const QByteArray &string_options) {

  Q_UNUSED(instanceId);

  CommandlineOptions options;
  options.Load(string_options);

  if (options.is_empty()) {
    raise();
    show();
    activateWindow();
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
      app_->player()->PlayPause();
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
    case CommandlineOptions::Player_RestartOrPrevious:
      app_->player()->RestartOrPrevious();
      break;

    case CommandlineOptions::Player_None:
      break;
  }

  if (!options.urls().empty()) {

#ifdef HAVE_TIDAL
    for (const QUrl url : options.urls()) {
      if (url.scheme() == "tidal" && url.host() == "login") {
        emit AuthorisationUrlReceived(url);
        return;
      }
    }
#endif
    MimeData *data = new MimeData;
    data->setUrls(options.urls());
    // Behaviour depends on command line options, so set it here
    data->override_user_settings_ = true;

    if (options.player_action() == CommandlineOptions::Player_Play) data->play_now_ = true;
    else ApplyPlayBehaviour(doubleclick_playmode_, data);

    switch (options.url_list_action()) {
      case CommandlineOptions::UrlList_Load:
        data->clear_first_ = true;
        break;
      case CommandlineOptions::UrlList_Append:
        // Nothing to do
        break;
      case CommandlineOptions::UrlList_None:
        ApplyAddBehaviour(doubleclick_addmode_, data);
        break;
      case CommandlineOptions::UrlList_CreateNew:
        data->name_for_new_playlist_ = options.playlist_name();
        ApplyAddBehaviour(BehaviourSettingsPage::AddBehaviour_OpenInNew, data);
        break;
    }

    AddToPlaylist(data);
  }

  if (options.set_volume() != -1) app_->player()->SetVolume(options.set_volume());

  if (options.volume_modifier() != 0) {
    app_->player()->SetVolume(app_->player()->GetVolume() +options.volume_modifier());
  }

  if (options.seek_to() != -1) {
    app_->player()->SeekTo(options.seek_to());
  }
  else if (options.seek_by() != 0) {
    app_->player()->SeekTo(app_->player()->engine()->position_nanosec() /kNsecPerSec +options.seek_by());
  }

  if (options.play_track_at() != -1) app_->player()->PlayAt(options.play_track_at(), Engine::Manual, true);

  if (options.show_osd()) app_->player()->ShowOSD();

  if (options.toggle_pretty_osd()) app_->player()->TogglePrettyOSD();
}

void MainWindow::ForceShowOSD(const Song &song, const bool toggle) {
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

  MimeData *data = new MimeData;
  data->setUrls(QList<QUrl>() << QUrl::fromLocalFile(url));
  AddToPlaylist(data);

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

#ifdef HAVE_GSTREAMER
void MainWindow::AddFilesToTranscoder() {

  QStringList filenames;

  for (const QModelIndex &index : ui_->playlist->view()->selectionModel()->selection().indexes()) {
    if (index.column() != 0) continue;
    int row =app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(row));
    Song song = item->Metadata();
    filenames << song.url().toLocalFile();
  }

  transcode_dialog_->SetFilenames(filenames);

  ShowTranscodeDialog();
}
#endif

void MainWindow::ShowCollectionConfig() {
  //EnsureSettingsDialogCreated();
  settings_dialog_->OpenAtPage(SettingsDialog::Page_Collection);
}

void MainWindow::TaskCountChanged(int count) {
  if (count == 0) {
    ui_->status_bar_stack->setCurrentWidget(ui_->playlist_summary_page);
  }
  else {
    ui_->status_bar_stack->setCurrentWidget(ui_->multi_loading_indicator);
  }
}

void MainWindow::PlayingWidgetPositionChanged(bool above_status_bar) {

  if (above_status_bar) ui_->status_bar->setParent(ui_->centralWidget);
  else ui_->status_bar->setParent(ui_->player_controls_container);

  ui_->status_bar->parentWidget()->layout()->addWidget(ui_->status_bar);
  ui_->status_bar->show();
}

void MainWindow::CopyFilesToCollection(const QList<QUrl> &urls) {
  organise_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organise_dialog_->SetUrls(urls);
  organise_dialog_->SetCopy(true);
  organise_dialog_->show();
}

void MainWindow::MoveFilesToCollection(const QList<QUrl> &urls) {
  organise_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organise_dialog_->SetUrls(urls);
  organise_dialog_->SetCopy(false);
  organise_dialog_->show();
}

void MainWindow::CopyFilesToDevice(const QList<QUrl> &urls) {
#if defined(HAVE_GSTREAMER) && !defined(Q_OS_WIN)
  organise_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organise_dialog_->SetCopy(true);
  if (organise_dialog_->SetUrls(urls))
    organise_dialog_->show();
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }
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
}

void MainWindow::PlaylistCopyToCollection() {
  PlaylistOrganiseSelected(true);
}

void MainWindow::PlaylistMoveToCollection() {
  PlaylistOrganiseSelected(false);
}

void MainWindow::PlaylistOrganiseSelected(bool copy) {

  QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  SongList songs;

  for (const QModelIndex &proxy_index : proxy_indexes) {
    QModelIndex index =app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    songs << app_->playlist_manager()->current()->item_at(index.row())->Metadata();
  }

  organise_dialog_->SetDestinationModel(app_->collection_model()->directory_model());
  organise_dialog_->SetSongs(songs);
  organise_dialog_->SetCopy(copy);
  organise_dialog_->show();
}

void MainWindow::PlaylistOpenInBrowser() {

  QList<QUrl> urls;
  QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();

  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    urls << QUrl(index.sibling(index.row(), Playlist::Column_Filename).data().toString());
  }

  Utilities::OpenInFileBrowser(urls);
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
  QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  SongList songs;

  for (const QModelIndex &proxy_index : proxy_indexes) {
    QModelIndex index = app_->playlist_manager()->current()->proxy()->mapToSource(proxy_index);
    songs << app_->playlist_manager()->current()->item_at(index.row())->Metadata();
  }

  organise_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organise_dialog_->SetCopy(true);
  if (organise_dialog_->SetSongs(songs))
    organise_dialog_->show();
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

}

SettingsDialog *MainWindow::CreateSettingsDialog() {

  SettingsDialog *settings_dialog = new SettingsDialog(app_);
#ifdef HAVE_GLOBALSHORTCUTS
  settings_dialog->SetGlobalShortcutManager(global_shortcuts_);
#endif

  // Settings
  connect(settings_dialog, SIGNAL(ReloadSettings()), SLOT(ReloadAllSettings()));

  // Allows custom notification preview
  connect(settings_dialog, SIGNAL(NotificationPreview(OSD::Behaviour, QString, QString)), SLOT(HandleNotificationPreview(OSD::Behaviour, QString, QString)));
  return settings_dialog;

}

void MainWindow::OpenSettingsDialog() {

  settings_dialog_->show();

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

}

#ifdef HAVE_GSTREAMER
void MainWindow::ShowTranscodeDialog() {

  transcode_dialog_->show();

}
#endif

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
  for (int i = from; i <= to; i++) {
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

  connect(ui_->playlist->view()->selectionModel(),SIGNAL(currentChanged(QModelIndex, QModelIndex)), SLOT(PlaylistCurrentChanged(QModelIndex)));
}

void MainWindow::PlaylistCurrentChanged(const QModelIndex &proxy_current) {
  const QModelIndex source_current =app_->playlist_manager()->current()->proxy()->mapToSource(proxy_current);

  // If the user moves the current index using the keyboard and then presses
  // F2, we don't want that editing the last column that was right clicked on.
  if (source_current != playlist_menu_index_) playlist_menu_index_ = QModelIndex();
}

void MainWindow::Raise() {
  show();
  activateWindow();
}

#ifdef Q_OS_WIN32
bool MainWindow::winEvent(MSG *msg, long*) {
  thumbbar_->HandleWinEvent(msg);
  return false;
}
#endif  // Q_OS_WIN32

void MainWindow::Exit() {

  SaveGeometry();
  SavePlaybackStatus();
  ui_->tabs->SaveSettings(kSettingsGroup);
  ui_->playlist->view()->SaveGeometry();
  ui_->playlist->view()->SaveSettings();
  app_->scrobbler()->WriteCache();

  if (app_->player()->engine()->is_fadeout_enabled()) {
    // To shut down the application when fadeout will be finished
    connect(app_->player()->engine(), SIGNAL(FadeoutFinishedSignal()), qApp, SLOT(quit()));
    if (app_->player()->GetState() == Engine::Playing) {
      app_->player()->Stop();
      hide();
      if (tray_icon_) tray_icon_->SetVisible(false);
      return; // Don't quit the application now: wait for the fadeout finished signal
    }
  }

  qApp->quit();

}

#if defined(HAVE_GSTREAMER) && defined(HAVE_CHROMAPRINT)
void MainWindow::AutoCompleteTags() {

  // Create the tag fetching stuff if it hasn't been already
  if (!tag_fetcher_) {
    tag_fetcher_.reset(new TagFetcher);
    track_selection_dialog_.reset(new TrackSelectionDialog);
    track_selection_dialog_->set_save_on_close(true);

    connect(tag_fetcher_.get(), SIGNAL(ResultAvailable(Song, SongList)), track_selection_dialog_.get(), SLOT(FetchTagFinished(Song, SongList)), Qt::QueuedConnection);
    connect(tag_fetcher_.get(), SIGNAL(Progress(Song, QString)), track_selection_dialog_.get(), SLOT(FetchTagProgress(Song,QString)));
    connect(track_selection_dialog_.get(), SIGNAL(accepted()), SLOT(AutoCompleteTagsAccepted()));
    connect(track_selection_dialog_.get(), SIGNAL(finished(int)), tag_fetcher_.get(), SLOT(Cancel()));
    connect(track_selection_dialog_.get(), SIGNAL(Error(QString)), SLOT(ShowErrorDialog(QString)));
  }

  // Get the selected songs and start fetching tags for them
  SongList songs;
  autocomplete_tag_items_.clear();
  for (const QModelIndex &index : ui_->playlist->view()->selectionModel()->selection().indexes()) {
    if (index.column() != 0) continue;
    int row = app_->playlist_manager()->current()->proxy()->mapToSource(index).row();
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(row));
    Song song = item->Metadata();

    if (song.IsEditable()) {
      songs << song;
      autocomplete_tag_items_ << item;
    }
  }

  track_selection_dialog_->Init(songs);
  tag_fetcher_->StartFetch(songs);

  track_selection_dialog_->show();
}

void MainWindow::AutoCompleteTagsAccepted() {

  for (const PlaylistItemPtr item : autocomplete_tag_items_) {
    item->Reload();
  }

  // This is really lame but we don't know what rows have changed
  ui_->playlist->view()->update();
}
#endif

void MainWindow::HandleNotificationPreview(OSD::Behaviour type, QString line1, QString line2) {

  if (!app_->playlist_manager()->current()->GetAllSongs().isEmpty()) {
    // Show a preview notification for the first song in the current playlist
    osd_->ShowPreview(type, line1, line2, app_->playlist_manager()->current()->GetAllSongs().first());
  }
  else {
    qLog(Debug) << "The current playlist is empty, showing a fake song";
    // Create a fake song
    Song fake;
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
  Console *console = new Console(app_, this);
  console->show();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Space) {
    app_->player()->PlayPause();
    event->accept();
  }
  else if (event->key() == Qt::Key_Left) {
    ui_->track_slider->Seek(-1);
    event->accept();
  }
  else if (event->key() == Qt::Key_Right) {
    ui_->track_slider->Seek(1);
    event->accept();
  }
  else {
    QMainWindow::keyPressEvent(event);
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

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("search_for_cover_auto", album_cover_choice_controller_->search_cover_auto_action()->isChecked());
  s.endGroup();
  GetCoverAutomatically();

}

void MainWindow::AlbumArtLoaded(const Song &song, const QString&, const QImage &image) {

  if (song.effective_albumartist() != song_playing_.effective_albumartist() || song.effective_album() != song_playing_.effective_album() || song.title() != song_playing_.title()) return;

  song_ = song;
  image_original_ = image;
  GetCoverAutomatically();

}

void MainWindow::GetCoverAutomatically() {

  // Search for cover automatically?
  bool search =
               album_cover_choice_controller_->search_cover_auto_action()->isChecked() &&
               !song_.has_manually_unset_cover() &&
               song_.art_automatic().isEmpty() &&
               song_.art_manual().isEmpty() &&
               !song_.effective_albumartist().isEmpty() &&
               !song_.effective_album().isEmpty();

  if (search) album_cover_choice_controller_->SearchCoverAutomatically(song_);

}

void MainWindow::ScrobblingEnabledChanged(bool value) {
  if (app_->scrobbler()->ScrobbleButton()) SetToggleScrobblingIcon(value);
}

void MainWindow::ScrobbleButtonVisibilityChanged(bool value) {
  ui_->button_scrobble->setVisible(value);
  ui_->action_toggle_scrobbling->setVisible(value);
  if (value) SetToggleScrobblingIcon(app_->scrobbler()->IsEnabled());

}

void MainWindow::SetToggleScrobblingIcon(bool value) {

  if (value) {
    if (app_->playlist_manager()->active()->scrobbled())
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble", 22));
    else 
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble", 22)); // TODO: Create a faint version of the icon
  }
  else {
    ui_->action_toggle_scrobbling->setIcon(IconLoader::Load("scrobble-disabled", 22));
  }

}
