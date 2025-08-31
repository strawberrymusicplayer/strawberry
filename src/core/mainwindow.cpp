/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cmath>
#include <algorithm>
#include <utility>
#include <functional>
#include <chrono>
#include <memory>

#include <QMainWindow>
#include <QApplication>
#include <QObject>
#include <QWidget>
#include <QScreen>
#include <QMetaObject>
#include <QThread>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileDialog>
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
#include <QErrorMessage>
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
#include <QCheckBox>
#include <QClipboard>
#include <QShowEvent>
#include <QCloseEvent>
#include <QKeyEvent>
#ifdef HAVE_DBUS
#  include <QDBusConnection>
#  include <QDBusMessage>
#endif

#include "core/logging.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "constants/filefilterconstants.h"
#include "constants/timeconstants.h"
#include "constants/mainwindowsettings.h"
#include "includes/shared_ptr.h"
#include "core/commandlineoptions.h"
#include "core/mimedata.h"
#include "core/iconloader.h"
#include "core/taskmanager.h"
#include "core/song.h"
#include "core/stylehelper.h"
#include "core/stylesheetloader.h"
#include "application.h"
#include "core/database.h"
#include "core/filesystemmusicstorage.h"
#include "core/deletefiles.h"
#include "core/settings.h"
#include "core/player.h"
#include "utilities/envutils.h"
#include "utilities/filemanagerutils.h"
#include "utilities/screenutils.h"
#include "engine/enginebase.h"
#include "dialogs/errordialog.h"
#include "dialogs/about.h"
#include "dialogs/console.h"
#include "dialogs/addstreamdialog.h"
#include "dialogs/deleteconfirmationdialog.h"
#include "dialogs/lastfmimportdialog.h"
#include "dialogs/snapdialog.h"
#include "dialogs/edittagdialog.h"
#include "dialogs/trackselectiondialog.h"
#include "organize/organizedialog.h"
#include "widgets/fancytabwidget.h"
#include "widgets/playingwidget.h"
#include "widgets/volumeslider.h"
#include "widgets/multiloadingindicator.h"
#include "widgets/trackslider.h"
#include "fileview/fileview.h"
#include "osd/osdbase.h"
#include "context/contextview.h"
#include "collection/collectionlibrary.h"
#include "collection/collectionbackend.h"
#include "collection/collectiondirectorymodel.h"
#include "collection/collectionviewcontainer.h"
#include "collection/collectionfilterwidget.h"
#include "collection/collectionfilter.h"
#include "collection/collectionmodel.h"
#include "collection/collectionview.h"
#include "playlist/playlist.h"
#include "playlist/playlistbackend.h"
#include "playlist/playlistcontainer.h"
#include "playlist/playlistlistcontainer.h"
#include "playlist/playlistmanager.h"
#include "playlist/playlistsequence.h"
#include "playlist/playlistview.h"
#include "playlist/playlistfilter.h"
#include "queue/queue.h"
#include "queue/queueview.h"
#include "playlistparsers/playlistparser.h"
#include "analyzer/analyzercontainer.h"
#include "equalizer/equalizer.h"
#ifdef HAVE_GLOBALSHORTCUTS
#  include "globalshortcuts/globalshortcutsmanager.h"
#endif
#include "covermanager/albumcovermanager.h"
#include "covermanager/albumcoverchoicecontroller.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/currentalbumcoverloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/albumcoverimageresult.h"
#include "lyrics/lyricsproviders.h"
#include "device/devicemanager.h"
#include "device/devicestatefiltermodel.h"
#include "device/deviceview.h"
#include "device/deviceviewcontainer.h"
#include "transcoder/transcodedialog.h"
#include "settings/settingsdialog.h"
#include "constants/behavioursettings.h"
#include "constants/appearancesettings.h"
#include "constants/backendsettings.h"
#include "constants/collectionsettings.h"
#include "constants/playlistsettings.h"
#ifdef HAVE_SUBSONIC
#  include "constants/subsonicsettings.h"
#endif
#ifdef HAVE_TIDAL
#  include "tidal/tidalservice.h"
#  include "constants/tidalsettings.h"
#endif
#ifdef HAVE_SPOTIFY
#  include "spotify/spotifyservice.h"
#  include "constants/spotifysettings.h"
#endif
#ifdef HAVE_QOBUZ
#  include "constants/qobuzsettings.h"
#endif

#include "streaming/streamingservices.h"
#include "streaming/streamingservice.h"
#include "streaming/streamingsongsview.h"
#include "streaming/streamingtabsview.h"
#include "streaming/streamingcollectionview.h"
#include "streaming/streamingsearchview.h"

#include "radios/radioservices.h"
#include "radios/radioviewcontainer.h"

#include "scrobbler/audioscrobbler.h"
#include "scrobbler/lastfmimport.h"

#ifdef HAVE_MUSICBRAINZ
#  include "musicbrainz/tagfetcher.h"
#endif

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarcontroller.h"
#  include "moodbar/moodbarloader.h"
#  include "moodbar/moodbarproxystyle.h"
#endif

#include "smartplaylists/smartplaylistsviewcontainer.h"

#include "organize/organizeerrordialog.h"

#ifdef Q_OS_WIN32
#  include "core/windows7thumbbar.h"
#endif

#ifdef Q_OS_MACOS
#  include "core/mac_startup.h"
#  include "systemtrayicon/macsystemtrayicon.h"
#  include "utilities/macosutils.h"
#else
#  include "systemtrayicon/qtsystemtrayicon.h"
#endif

#ifdef HAVE_SPARKLE
  #include "core/sparkleupdater.h"
#endif

#ifdef HAVE_QTSPARKLE
#  include <qtsparkle-qt6/Updater>
#endif  // HAVE_QTSPARKLE

#ifdef HAVE_DISCORD_RPC
  #include "discord/richpresence.h"
#endif

using std::make_unique;
using std::make_shared;
using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

namespace {
const int kTrackSliderUpdateTimeMs = 200;
const int kTrackPositionUpdateTimeMs = 1000;
}  // namespace

#ifdef HAVE_QTSPARKLE
namespace {

#  if defined(__APPLE__)
#    if defined(__x86_64__)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-macos-x86_64";
#    elif defined(__aarch64__)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-macos-arm64";
#    else
#      error "Unsupported macOS arch for QtSparkle"
#    endif

#  elif defined(__MINGW32__)
#    if defined(__x86_64__)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-windows-mingw-x64";
#    elif defined(__i686__)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-windows-mingw-x86";
#    else
#      error "Unsupported MinGW arch for QtSparkle"
#    endif

#  elif defined(_MSC_VER)
#    if defined(_WIN64)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-windows-msvc-x64";
#    elif defined(_WIN32)
constexpr char QTSPARKLE_URL[] = "https://www.strawberrymusicplayer.org/sparkle-windows-msvc-x86";
#    else
#      error "Unsupported MSVC arch for QtSparkle"
#    endif

#  else
#  error "Unsupported OS for QtSparkle"
#  endif // OS

}  // namespace
#endif  // HAVE_QTSPARKLE

MainWindow::MainWindow(Application *app,
                       SharedPtr<SystemTrayIcon> systemtrayicon, OSDBase *osd,
#ifdef HAVE_DISCORD_RPC
                       discord::RichPresence *discord_rich_presence,
#endif
                       const CommandlineOptions &options,
                       QWidget *parent)
    : QMainWindow(parent),
      ui_(new Ui_MainWindow),
#ifdef Q_OS_WIN32
      thumbbar_(new Windows7ThumbBar(this)),
#endif
      app_(app),
      systemtrayicon_(systemtrayicon),
      osd_(osd),
#ifdef HAVE_DISCORD_RPC
      discord_rich_presence_(discord_rich_presence),
#endif
      console_([app, this]() {
        Console *console = new Console(app->database());
        QObject::connect(console, &Console::Error, this, &MainWindow::ShowErrorDialog);
        return console;
      }),
      edit_tag_dialog_(std::bind(&MainWindow::CreateEditTagDialog, this)),
      album_cover_choice_controller_(new AlbumCoverChoiceController(this)),
#ifdef HAVE_GLOBALSHORTCUTS
      globalshortcuts_manager_(new GlobalShortcutsManager(this)),
#endif
      context_view_(new ContextView(this)),
      collection_view_(new CollectionViewContainer(this)),
      file_view_(new FileView(this)),
      device_view_(new DeviceViewContainer(this)),
      playlist_list_(new PlaylistListContainer(this)),
      queue_view_(new QueueView(this)),
      settings_dialog_(std::bind(&MainWindow::CreateSettingsDialog, this)),
      cover_manager_([this, app]() {
        AlbumCoverManager *cover_manager = new AlbumCoverManager(app->network(), app->collection_backend(), app->tagreader_client(), app->albumcover_loader(), app->current_albumcover_loader(), app->cover_providers(), app->streaming_services(), this);
        cover_manager->Init();

        // Cover manager connections
        QObject::connect(cover_manager, &AlbumCoverManager::Error, this, &MainWindow::ShowErrorDialog);
        QObject::connect(cover_manager, &AlbumCoverManager::AddToPlaylist, this, &MainWindow::AddToPlaylist);
        return cover_manager;
      }),
      equalizer_(new Equalizer),
      organize_dialog_([this, app]() {
        OrganizeDialog *dialog = new OrganizeDialog(app->task_manager(), app->tagreader_client(), app->collection_backend(), this);
        dialog->SetDestinationModel(app->collection()->model()->directory_model());
        return dialog;
      }),
      transcode_dialog_([this]() {
        TranscodeDialog *dialog = new TranscodeDialog(this);
        return dialog;
      }),
      add_stream_dialog_([this]() {
        AddStreamDialog *add_stream_dialog = new AddStreamDialog;
        QObject::connect(add_stream_dialog, &AddStreamDialog::accepted, this, &MainWindow::AddStreamAccepted);
        return add_stream_dialog;
      }),
      smartplaylists_view_(new SmartPlaylistsViewContainer(app->player(),
                                                           app->playlist_manager(),
                                                           app->collection_backend(),
#ifdef HAVE_MOODBAR
                                                           app->moodbar_loader(),
#endif
                                                           app->current_albumcover_loader(),
                                                           this)),
#ifdef HAVE_SUBSONIC
      subsonic_view_(new StreamingSongsView(app->streaming_services()->ServiceBySource(Song::Source::Subsonic), QLatin1String(SubsonicSettings::kSettingsGroup), this)),
#endif
#ifdef HAVE_TIDAL
      tidal_view_(new StreamingTabsView(app->streaming_services()->ServiceBySource(Song::Source::Tidal), app->albumcover_loader(), QLatin1String(TidalSettings::kSettingsGroup), this)),
#endif
#ifdef HAVE_SPOTIFY
      spotify_view_(new StreamingTabsView(app->streaming_services()->ServiceBySource(Song::Source::Spotify), app->albumcover_loader(), QLatin1String(SpotifySettings::kSettingsGroup), this)),
#endif
#ifdef HAVE_QOBUZ
      qobuz_view_(new StreamingTabsView(app->streaming_services()->ServiceBySource(Song::Source::Qobuz), app->albumcover_loader(), QLatin1String(QobuzSettings::kSettingsGroup), this)),
#endif
      radio_view_(new RadioViewContainer(this)),
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
      playlist_copy_to_device_(nullptr),
      playlist_delete_(nullptr),
      playlist_queue_(nullptr),
      playlist_queue_play_next_(nullptr),
      playlist_skip_(nullptr),
      playlist_add_to_another_(nullptr),
      playlistitem_actions_separator_(nullptr),
      playlist_rescan_songs_(nullptr),
      track_position_timer_(new QTimer(this)),
      track_slider_timer_(new QTimer(this)),
      keep_running_(false),
      playing_widget_(true),
#ifdef HAVE_DBUS
      taskbar_progress_(false),
#endif
      doubleclick_addmode_(BehaviourSettings::AddBehaviour::Append),
      doubleclick_playmode_(BehaviourSettings::PlayBehaviour::Never),
      doubleclick_playlist_addmode_(BehaviourSettings::PlaylistAddBehaviour::Play),
      menu_playmode_(BehaviourSettings::PlayBehaviour::Never),
      initialized_(false),
      was_maximized_(true),
      was_minimized_(false),
      exit_(false),
      exit_count_(0),
      playlists_loaded_(false),
      delete_files_(false) {

  qLog(Debug) << "Starting";

  // Initialize the UI
  ui_->setupUi(this);

  if (QGuiApplication::platformName() != "wayland"_L1) {
    setWindowIcon(IconLoader::Load(u"strawberry"_s));
  }

  systemtrayicon_->SetDevicePixelRatioF(devicePixelRatioF());

  QObject::connect(&*app->database(), &Database::Error, this, &MainWindow::ShowErrorDialog);

  album_cover_choice_controller_->Init(app->network(), app->tagreader_client(), app->collection()->backend(), app->albumcover_loader(), app->current_albumcover_loader(), app->cover_providers(), app->streaming_services());

  ui_->multi_loading_indicator->SetTaskManager(app_->task_manager());
  context_view_->Init(collection_view_->view(), album_cover_choice_controller_, app_->lyrics_providers());
  ui_->widget_playing->Init(album_cover_choice_controller_);

  // Initialize the search widget
  StyleHelper::setBaseColor(palette().color(QPalette::Highlight).darker());

  // Add tabs to the fancy tab widget
  ui_->tabs->AddTab(context_view_, u"context"_s, IconLoader::Load(u"strawberry"_s, true, 0, 32), tr("Context"));
  ui_->tabs->AddTab(collection_view_, u"collection"_s, IconLoader::Load(u"library-music"_s, true, 0, 32), tr("Collection"));
  ui_->tabs->AddTab(queue_view_, u"queue"_s, IconLoader::Load(u"footsteps"_s, true, 0, 32), tr("Queue"));
  ui_->tabs->AddTab(playlist_list_, u"playlists"_s, IconLoader::Load(u"view-media-playlist"_s, true, 0, 32), tr("Playlists"));
  ui_->tabs->AddTab(smartplaylists_view_, u"smartplaylists"_s, IconLoader::Load(u"view-media-playlist"_s, true, 0, 32), tr("Smart playlists"));
  ui_->tabs->AddTab(file_view_, u"files"_s, IconLoader::Load(u"document-open"_s, true, 0, 32), tr("Files"));
  ui_->tabs->AddTab(radio_view_, u"radios"_s, IconLoader::Load(u"radio"_s, true, 0, 32), tr("Radios"));
  ui_->tabs->AddTab(device_view_, u"devices"_s, IconLoader::Load(u"device"_s, true, 0, 32), tr("Devices"));
#ifdef HAVE_SUBSONIC
  ui_->tabs->AddTab(subsonic_view_, u"subsonic"_s, IconLoader::Load(u"subsonic"_s, true, 0, 32), tr("Subsonic"));
#endif
#ifdef HAVE_TIDAL
  ui_->tabs->AddTab(tidal_view_, u"tidal"_s, IconLoader::Load(u"tidal"_s, true, 0, 32), tr("Tidal"));
#endif
#ifdef HAVE_SPOTIFY
  ui_->tabs->AddTab(spotify_view_, u"spotify"_s, IconLoader::Load(u"spotify"_s, true, 0, 32), tr("Spotify"));
#endif
#ifdef HAVE_QOBUZ
  ui_->tabs->AddTab(qobuz_view_, u"qobuz"_s, IconLoader::Load(u"qobuz"_s, true, 0, 32), tr("Qobuz"));
#endif

  // Add the playing widget to the fancy tab widget
  ui_->tabs->AddBottomWidget(ui_->widget_playing);
  ui_->tabs->SetBackgroundPixmap(QPixmap(u":/pictures/sidebar-background.png"_s));
  ui_->tabs->LoadSettings(QLatin1String(MainWindowSettings::kSettingsGroup));

  track_position_timer_->setInterval(kTrackPositionUpdateTimeMs);
  QObject::connect(track_position_timer_, &QTimer::timeout, this, &MainWindow::UpdateTrackPosition);
  track_slider_timer_->setInterval(kTrackSliderUpdateTimeMs);
  QObject::connect(track_slider_timer_, &QTimer::timeout, this, &MainWindow::UpdateTrackSliderPosition);

  // Start initializing the player
  qLog(Debug) << "Initializing player";
  app_->player()->SetAnalyzer(ui_->analyzer);
  app_->player()->SetEqualizer(equalizer_);
  app_->player()->Init();

  const uint volume = app_->player()->GetVolume();
  ui_->volume->SetValue(volume);
  VolumeChanged(volume);

  QObject::connect(ui_->playlist, &PlaylistContainer::ViewSelectionModelChanged, this, &MainWindow::PlaylistViewSelectionModelChanged);

  ui_->playlist->SetManager(app_->playlist_manager());

  ui_->playlist->view()->Init(app_->player(),
                              app_->playlist_manager(),
                              app_->collection_backend(),
#ifdef HAVE_MOODBAR
                              app_->moodbar_loader(),
#endif
                              app_->current_albumcover_loader());

  collection_view_->view()->setModel(app_->collection()->model()->filter());
  collection_view_->view()->Init(app->task_manager(), app->tagreader_client(), app->network(), app->albumcover_loader(), app->current_albumcover_loader(), app->cover_providers(), app->lyrics_providers(), app->collection(), app->device_manager(), app->streaming_services());
  device_view_->view()->Init(app->task_manager(), app->tagreader_client(), app->device_manager(), app->collection_model()->directory_model());
  playlist_list_->Init(app_->task_manager(), app->tagreader_client(), app_->playlist_manager(), app_->playlist_backend(), app_->device_manager());

  organize_dialog_->SetDestinationModel(app_->collection()->model()->directory_model());

  radio_view_->view()->setModel(app_->radio_services()->sort_model());

  // Icons
  qLog(Debug) << "Creating UI";

  // Help menu

  ui_->action_about_strawberry->setIcon(IconLoader::Load(u"strawberry"_s));
  ui_->action_about_qt->setIcon(QIcon(u":/qt-project.org/qmessagebox/images/qtlogo-64.png"_s));

  // Music menu

  ui_->action_open_file->setIcon(IconLoader::Load(u"document-open"_s));
  ui_->action_open_cd->setIcon(IconLoader::Load(u"media-optical"_s));
  ui_->action_previous_track->setIcon(IconLoader::Load(u"media-skip-backward"_s));
  ui_->action_play_pause->setIcon(IconLoader::Load(u"media-playback-start"_s));
  ui_->action_stop->setIcon(IconLoader::Load(u"media-playback-stop"_s));
  ui_->action_stop_after_this_track->setIcon(IconLoader::Load(u"media-playback-stop"_s));
  ui_->action_next_track->setIcon(IconLoader::Load(u"media-skip-forward"_s));
  ui_->action_quit->setIcon(IconLoader::Load(u"application-exit"_s));

  // Playlist

  ui_->action_add_file->setIcon(IconLoader::Load(u"document-open"_s));
  ui_->action_add_folder->setIcon(IconLoader::Load(u"document-open-folder"_s));
  ui_->action_add_stream->setIcon(IconLoader::Load(u"document-open-remote"_s));
  ui_->action_shuffle_mode->setIcon(IconLoader::Load(u"media-playlist-shuffle"_s));
  ui_->action_repeat_mode->setIcon(IconLoader::Load(u"media-playlist-repeat"_s));
  ui_->action_new_playlist->setIcon(IconLoader::Load(u"document-new"_s));
  ui_->action_save_playlist->setIcon(IconLoader::Load(u"document-save"_s));
  ui_->action_load_playlist->setIcon(IconLoader::Load(u"document-open"_s));
  ui_->action_jump->setIcon(IconLoader::Load(u"go-jump"_s));
  ui_->action_clear_playlist->setIcon(IconLoader::Load(u"edit-clear-list"_s));
  ui_->action_shuffle->setIcon(IconLoader::Load(u"media-playlist-shuffle"_s));
  ui_->action_remove_duplicates->setIcon(IconLoader::Load(u"list-remove"_s));
  ui_->action_remove_unavailable->setIcon(IconLoader::Load(u"list-remove"_s));
  ui_->action_remove_from_playlist->setIcon(IconLoader::Load(u"list-remove"_s));
  ui_->action_save_all_playlists->setIcon(IconLoader::Load(u"document-save-all"_s));

  // Configure

  ui_->action_cover_manager->setIcon(IconLoader::Load(u"document-download"_s));
  ui_->action_edit_track->setIcon(IconLoader::Load(u"edit-rename"_s));
  ui_->action_edit_value->setIcon(IconLoader::Load(u"edit-rename"_s));
  ui_->action_selection_set_value->setIcon(IconLoader::Load(u"edit-rename"_s));
  ui_->action_equalizer->setIcon(IconLoader::Load(u"equalizer"_s));
  ui_->action_transcoder->setIcon(IconLoader::Load(u"tools-wizard"_s));
  ui_->action_update_collection->setIcon(IconLoader::Load(u"view-refresh"_s));
  ui_->action_full_collection_scan->setIcon(IconLoader::Load(u"view-refresh"_s));
  ui_->action_stop_collection_scan->setIcon(IconLoader::Load(u"dialog-error"_s));
  ui_->action_settings->setIcon(IconLoader::Load(u"configure"_s));
  ui_->action_import_data_from_last_fm->setIcon(IconLoader::Load(u"scrobble"_s));
  ui_->action_console->setIcon(IconLoader::Load(u"keyboard"_s));
  ui_->action_toggle_show_sidebar->setIcon(IconLoader::Load(u"view-choose"_s));
  ui_->action_auto_complete_tags->setIcon(IconLoader::Load(u"musicbrainz"_s));

  // Scrobble

  ui_->action_toggle_scrobbling->setIcon(IconLoader::Load(u"scrobble-disabled"_s));
  ui_->action_love->setIcon(IconLoader::Load(u"love"_s));

  // File view connections
  QObject::connect(file_view_, &FileView::AddToPlaylist, this, &MainWindow::AddToPlaylist);
  QObject::connect(file_view_, &FileView::PathChanged, this, &MainWindow::FilePathChanged);
  QObject::connect(file_view_, &FileView::CopyToCollection, this, &MainWindow::CopyFilesToCollection);
  QObject::connect(file_view_, &FileView::MoveToCollection, this, &MainWindow::MoveFilesToCollection);
  QObject::connect(file_view_, &FileView::EditTags, this, &MainWindow::EditFileTags);
  QObject::connect(file_view_, &FileView::CopyToDevice, this, &MainWindow::CopyFilesToDevice);
  file_view_->SetTaskManager(app_->task_manager());

  // Action connections
  QObject::connect(ui_->action_next_track, &QAction::triggered, &*app_->player(), &Player::Next);
  QObject::connect(ui_->action_previous_track, &QAction::triggered, &*app_->player(), &Player::Previous);
  QObject::connect(ui_->action_play_pause, &QAction::triggered, &*app_->player(), &Player::PlayPauseHelper);
  QObject::connect(ui_->action_stop, &QAction::triggered, &*app_->player(), &Player::Stop);
  QObject::connect(ui_->action_quit, &QAction::triggered, this, &MainWindow::Exit);
  QObject::connect(ui_->action_stop_after_this_track, &QAction::triggered, this, &MainWindow::StopAfterCurrent);
  QObject::connect(ui_->action_mute, &QAction::triggered, &*app_->player(), &Player::Mute);

  QObject::connect(ui_->action_clear_playlist, &QAction::triggered, this, &MainWindow::PlaylistClearCurrent);
  QObject::connect(ui_->action_remove_duplicates, &QAction::triggered, &*app_->playlist_manager(), &PlaylistManager::RemoveDuplicatesCurrent);
  QObject::connect(ui_->action_remove_unavailable, &QAction::triggered, &*app_->playlist_manager(), &PlaylistManager::RemoveUnavailableCurrent);
  QObject::connect(ui_->action_remove_from_playlist, &QAction::triggered, this, &MainWindow::PlaylistRemoveCurrent);
  QObject::connect(ui_->action_edit_track, &QAction::triggered, this, &MainWindow::EditTracks);
  QObject::connect(ui_->action_renumber_tracks, &QAction::triggered, this, &MainWindow::RenumberTracks);
  QObject::connect(ui_->action_selection_set_value, &QAction::triggered, this, &MainWindow::SelectionSetValue);
  QObject::connect(ui_->action_edit_value, &QAction::triggered, this, &MainWindow::EditValue);
#ifdef HAVE_MUSICBRAINZ
  QObject::connect(ui_->action_auto_complete_tags, &QAction::triggered, this, &MainWindow::AutoCompleteTags);
#endif
  QObject::connect(ui_->action_settings, &QAction::triggered, this, &MainWindow::OpenSettingsDialog);
  QObject::connect(ui_->action_import_data_from_last_fm, &QAction::triggered, lastfm_import_dialog_, &LastFMImportDialog::show);
  QObject::connect(ui_->action_toggle_show_sidebar, &QAction::toggled, this, &MainWindow::ToggleSidebar);
  QObject::connect(ui_->action_about_strawberry, &QAction::triggered, this, &MainWindow::ShowAboutDialog);
  QObject::connect(ui_->action_about_qt, &QAction::triggered, qApp, &QApplication::aboutQt);
  QObject::connect(ui_->action_shuffle, &QAction::triggered, &*app_->playlist_manager(), &PlaylistManager::ShuffleCurrent);
  QObject::connect(ui_->action_open_file, &QAction::triggered, this, &MainWindow::AddFile);
  QObject::connect(ui_->action_open_cd, &QAction::triggered, this, &MainWindow::AddCDTracks);
  QObject::connect(ui_->action_add_file, &QAction::triggered, this, &MainWindow::AddFile);
  QObject::connect(ui_->action_add_folder, &QAction::triggered, this, &MainWindow::AddFolder);
  QObject::connect(ui_->action_add_stream, &QAction::triggered, this, &MainWindow::AddStream);
  QObject::connect(ui_->action_cover_manager, &QAction::triggered, this, &MainWindow::ShowCoverManager);
  QObject::connect(ui_->action_equalizer, &QAction::triggered, this, &MainWindow::ShowEqualizer);
  QObject::connect(ui_->action_transcoder, &QAction::triggered, this, &MainWindow::ShowTranscodeDialog);
  QObject::connect(ui_->action_jump, &QAction::triggered, ui_->playlist->view(), &PlaylistView::JumpToCurrentlyPlayingTrack);
  QObject::connect(ui_->action_update_collection, &QAction::triggered, &*app_->collection(), &CollectionLibrary::IncrementalScan);
  QObject::connect(ui_->action_full_collection_scan, &QAction::triggered, &*app_->collection(), &CollectionLibrary::FullScan);
  QObject::connect(ui_->action_stop_collection_scan, &QAction::triggered, &*app_->collection(), &CollectionLibrary::StopScan);
  QObject::connect(ui_->action_add_files_to_transcoder, &QAction::triggered, this, &MainWindow::AddFilesToTranscoder);
  ui_->action_add_files_to_transcoder->setIcon(IconLoader::Load(u"tools-wizard"_s));

  QObject::connect(ui_->action_toggle_scrobbling, &QAction::triggered, &*app_->scrobbler(), &AudioScrobbler::ToggleScrobbling);
  QObject::connect(ui_->action_love, &QAction::triggered, this, &MainWindow::Love);
  QObject::connect(&*app_->scrobbler(), &AudioScrobbler::ErrorMessage, this, &MainWindow::ShowErrorDialog);

  // Playlist view actions
  ui_->action_next_playlist->setShortcuts(QList<QKeySequence>() << QKeySequence::fromString(u"Ctrl+Tab"_s) << QKeySequence::fromString(u"Ctrl+PgDown"_s));
  ui_->action_previous_playlist->setShortcuts(QList<QKeySequence>() << QKeySequence::fromString(u"Ctrl+Shift+Tab"_s) << QKeySequence::fromString(u"Ctrl+PgUp"_s));

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

  ui_->playlist->SetActions(ui_->action_new_playlist, ui_->action_load_playlist, ui_->action_save_playlist, ui_->action_clear_playlist, ui_->action_next_playlist, /* These two actions aren't associated */ ui_->action_previous_playlist /* to a button but to the main window */, ui_->action_save_all_playlists);
  // Add the shuffle and repeat action groups to the menu
  ui_->action_shuffle_mode->setMenu(ui_->playlist_sequence->shuffle_menu());
  ui_->action_repeat_mode->setMenu(ui_->playlist_sequence->repeat_menu());

  // Stop actions
  QMenu *stop_menu = new QMenu(this);
  stop_menu->addAction(ui_->action_stop);
  stop_menu->addAction(ui_->action_stop_after_this_track);
  ui_->stop_button->setMenu(stop_menu);

  // Player connections
  QObject::connect(ui_->volume, &VolumeSlider::valueChanged, &*app_->player(), &Player::SetVolumeFromSlider);

  QObject::connect(&*app_->player(), &Player::Error, this, &MainWindow::ShowErrorDialog);
  QObject::connect(&*app_->player(), &Player::SongChangeRequestProcessed, &*app_->playlist_manager(), &PlaylistManager::SongChangeRequestProcessed);

  QObject::connect(&*app_->player(), &Player::Paused, this, &MainWindow::MediaPaused);
  QObject::connect(&*app_->player(), &Player::Playing, this, &MainWindow::MediaPlaying);
  QObject::connect(&*app_->player(), &Player::Stopped, this, &MainWindow::MediaStopped);
  QObject::connect(&*app_->player(), &Player::Seeked, this, &MainWindow::Seeked);
  QObject::connect(&*app_->player(), &Player::TrackSkipped, this, &MainWindow::TrackSkipped);
  QObject::connect(&*app_->player(), &Player::VolumeChanged, this, &MainWindow::VolumeChanged);

  QObject::connect(&*app_->player(), &Player::Paused, ui_->playlist, &PlaylistContainer::ActivePaused);
  QObject::connect(&*app_->player(), &Player::Playing, ui_->playlist, &PlaylistContainer::ActivePlaying);
  QObject::connect(&*app_->player(), &Player::Stopped, ui_->playlist, &PlaylistContainer::ActiveStopped);

  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, osd_, &OSDBase::SongChanged);

  QObject::connect(&*app_->player(), &Player::Paused, osd_, &OSDBase::Paused);
  QObject::connect(&*app_->player(), &Player::Resumed, osd_, &OSDBase::Resumed);
  QObject::connect(&*app_->player(), &Player::Stopped, osd_, &OSDBase::Stopped);
  QObject::connect(&*app_->player(), &Player::PlaylistFinished, osd_, &OSDBase::PlaylistFinished);
  QObject::connect(&*app_->player(), &Player::VolumeChanged, osd_, &OSDBase::VolumeChanged);
  QObject::connect(&*app_->player(), &Player::VolumeChanged, ui_->volume, &VolumeSlider::SetValue);
  QObject::connect(&*app_->player(), &Player::ForceShowOSD, this, &MainWindow::ForceShowOSD);

  QObject::connect(&*app_->current_albumcover_loader(), &CurrentAlbumCoverLoader::ThumbnailLoaded, osd_, &OSDBase::AlbumCoverLoaded);

  QObject::connect(&*app_->player(), &Player::Paused, &*app_->playlist_manager(), &PlaylistManager::SetActivePaused);
  QObject::connect(&*app_->player(), &Player::Playing, &*app_->playlist_manager(), &PlaylistManager::SetActivePlaying);
  QObject::connect(&*app_->player(), &Player::Stopped, &*app_->playlist_manager(), &PlaylistManager::SetActiveStopped);

  QObject::connect(&*app_->player(), &Player::Paused, playlist_list_, &PlaylistListContainer::ActivePaused);
  QObject::connect(&*app_->player(), &Player::Playing, playlist_list_, &PlaylistListContainer::ActivePlaying);
  QObject::connect(&*app_->player(), &Player::Stopped, playlist_list_, &PlaylistListContainer::ActiveStopped);

  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::AllPlaylistsLoaded, this, &MainWindow::PlaylistsLoaded);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, this, &MainWindow::SongChanged);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, &*app_->player(), &Player::CurrentMetadataChanged);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::EditingFinished, this, &MainWindow::PlaylistEditFinished);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::Error, this, &MainWindow::ShowErrorDialog);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::SummaryTextChanged, ui_->playlist_summary, &QLabel::setText);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::PlayRequested, this, &MainWindow::PlayIndex);

  QObject::connect(ui_->playlist->view(), &PlaylistView::doubleClicked, this, &MainWindow::PlaylistDoubleClick);
  QObject::connect(ui_->playlist->view(), &PlaylistView::PlayItem, this, &MainWindow::PlayIndex);
  QObject::connect(ui_->playlist->view(), &PlaylistView::PlayPause, &*app_->player(), &Player::PlayPause);
  QObject::connect(ui_->playlist->view(), &PlaylistView::RightClicked, this, &MainWindow::PlaylistRightClick);
  QObject::connect(ui_->playlist->view(), &PlaylistView::SeekForward, &*app_->player(), &Player::SeekForward);
  QObject::connect(ui_->playlist->view(), &PlaylistView::SeekBackward, &*app_->player(), &Player::SeekBackward);
  QObject::connect(ui_->playlist->view(), &PlaylistView::BackgroundPropertyChanged, this, &MainWindow::RefreshStyleSheet);

  QObject::connect(ui_->track_slider, &TrackSlider::ValueChangedSeconds, &*app_->player(), &Player::SeekTo);
  QObject::connect(ui_->track_slider, &TrackSlider::SeekForward, &*app_->player(), &Player::SeekForward);
  QObject::connect(ui_->track_slider, &TrackSlider::SeekBackward, &*app_->player(), &Player::SeekBackward);
  QObject::connect(ui_->track_slider, &TrackSlider::Previous, &*app_->player(), &Player::Previous);
  QObject::connect(ui_->track_slider, &TrackSlider::Next, &*app_->player(), &Player::Next);

  // Collection connections
  QObject::connect(&*app_->collection(), &CollectionLibrary::Error, this, &MainWindow::ShowErrorDialog);
  QObject::connect(collection_view_->view(), &CollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(collection_view_->view(), &CollectionView::ShowSettingsDialog, this, &MainWindow::OpenCollectionSettingsDialog);
  QObject::connect(collection_view_->view(), &CollectionView::Error, this, &MainWindow::ShowErrorDialog);
  QObject::connect(app_->collection_model(), &CollectionModel::TotalSongCountUpdated, collection_view_->view(), &CollectionView::TotalSongCountUpdated);
  QObject::connect(app_->collection_model(), &CollectionModel::TotalArtistCountUpdated, collection_view_->view(), &CollectionView::TotalArtistCountUpdated);
  QObject::connect(app_->collection_model(), &CollectionModel::TotalAlbumCountUpdated, collection_view_->view(), &CollectionView::TotalAlbumCountUpdated);
  QObject::connect(app_->collection_model(), &CollectionModel::modelAboutToBeReset, collection_view_->view(), &CollectionView::SaveFocus);
  QObject::connect(app_->collection_model(), &CollectionModel::modelReset, collection_view_->view(), &CollectionView::RestoreFocus);

  QObject::connect(&*app_->task_manager(), &TaskManager::PauseCollectionWatchers, &*app_->collection(), &CollectionLibrary::PauseWatcher);
  QObject::connect(&*app_->task_manager(), &TaskManager::ResumeCollectionWatchers, &*app_->collection(), &CollectionLibrary::ResumeWatcher);

  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, &*app_->current_albumcover_loader(), &CurrentAlbumCoverLoader::LoadAlbumCover);
  QObject::connect(&*app_->current_albumcover_loader(), &CurrentAlbumCoverLoader::AlbumCoverLoaded, this, &MainWindow::AlbumCoverLoaded);
  QObject::connect(album_cover_choice_controller_, &AlbumCoverChoiceController::Error, this, &MainWindow::ShowErrorDialog);
  QObject::connect(album_cover_choice_controller_->cover_from_file_action(), &QAction::triggered, this, &MainWindow::LoadCoverFromFile);
  QObject::connect(album_cover_choice_controller_->cover_to_file_action(), &QAction::triggered, this, &MainWindow::SaveCoverToFile);
  QObject::connect(album_cover_choice_controller_->cover_from_url_action(), &QAction::triggered, this, &MainWindow::LoadCoverFromURL);
  QObject::connect(album_cover_choice_controller_->search_for_cover_action(), &QAction::triggered, this, &MainWindow::SearchForCover);
  QObject::connect(album_cover_choice_controller_->unset_cover_action(), &QAction::triggered, this, &MainWindow::UnsetCover);
  QObject::connect(album_cover_choice_controller_->clear_cover_action(), &QAction::triggered, this, &MainWindow::ClearCover);
  QObject::connect(album_cover_choice_controller_->delete_cover_action(), &QAction::triggered, this, &MainWindow::DeleteCover);
  QObject::connect(album_cover_choice_controller_->show_cover_action(), &QAction::triggered, this, &MainWindow::ShowCover);
  QObject::connect(album_cover_choice_controller_->search_cover_auto_action(), &QAction::triggered, this, &MainWindow::SearchCoverAutomatically);
  QObject::connect(album_cover_choice_controller_->search_cover_auto_action(), &QAction::toggled, this, &MainWindow::ToggleSearchCoverAuto);

  // Devices connections
  QObject::connect(device_view_->view(), &DeviceView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);

  // Collection filter widget
  QActionGroup *collection_view_group = new QActionGroup(this);

  collection_show_all_ = collection_view_group->addAction(tr("Show all songs"));
  collection_show_duplicates_ = collection_view_group->addAction(tr("Show only duplicates"));
  collection_show_untagged_ = collection_view_group->addAction(tr("Show only untagged"));

  collection_show_all_->setCheckable(true);
  collection_show_duplicates_->setCheckable(true);
  collection_show_untagged_->setCheckable(true);
  collection_show_all_->setChecked(true);

  QObject::connect(collection_view_group, &QActionGroup::triggered, this, &MainWindow::ChangeCollectionFilterMode);

  QAction *collection_config_action = new QAction(IconLoader::Load(u"configure"_s), tr("Configure collection..."), this);
  QObject::connect(collection_config_action, &QAction::triggered, this, &MainWindow::OpenCollectionSettingsDialog);
  collection_view_->filter_widget()->SetSettingsGroup(QLatin1String(CollectionSettings::kSettingsGroup));
  collection_view_->filter_widget()->Init(app_->collection()->model(), app_->collection()->model()->filter());

  QAction *separator = new QAction(this);
  separator->setSeparator(true);

  collection_view_->filter_widget()->AddMenuAction(collection_show_all_);
  collection_view_->filter_widget()->AddMenuAction(collection_show_duplicates_);
  collection_view_->filter_widget()->AddMenuAction(collection_show_untagged_);
  collection_view_->filter_widget()->AddMenuAction(separator);
  collection_view_->filter_widget()->AddMenuAction(collection_config_action);

#ifdef HAVE_SUBSONIC
  QObject::connect(subsonic_view_, &StreamingSongsView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(subsonic_view_->view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
#endif

#ifdef HAVE_TIDAL
  QObject::connect(tidal_view_, &StreamingTabsView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(tidal_view_->artists_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(tidal_view_->albums_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(tidal_view_->songs_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(tidal_view_->search_view(), &StreamingSearchView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(tidal_view_->search_view(), &StreamingSearchView::AddToPlaylist, this, &MainWindow::AddToPlaylist);
  if (TidalServicePtr tidalservice = app_->streaming_services()->Service<TidalService>()) {
    QObject::connect(this, &MainWindow::AuthorizationUrlReceived, &*tidalservice, &TidalService::AuthorizationUrlReceived);
  }
#endif

#ifdef HAVE_QOBUZ
  QObject::connect(qobuz_view_, &StreamingTabsView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(qobuz_view_->artists_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(qobuz_view_->albums_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(qobuz_view_->songs_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(qobuz_view_->search_view(), &StreamingSearchView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(qobuz_view_->search_view(), &StreamingSearchView::AddToPlaylist, this, &MainWindow::AddToPlaylist);
#endif

#ifdef HAVE_SPOTIFY
  QObject::connect(spotify_view_, &StreamingTabsView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(spotify_view_->artists_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(spotify_view_->albums_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(spotify_view_->songs_collection_view(), &StreamingCollectionView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);
  QObject::connect(spotify_view_->search_view(), &StreamingSearchView::OpenSettingsDialog, this, &MainWindow::OpenServiceSettingsDialog);
  QObject::connect(spotify_view_->search_view(), &StreamingSearchView::AddToPlaylist, this, &MainWindow::AddToPlaylist);
  if (SpotifyServicePtr spotifyservice = app_->streaming_services()->Service<SpotifyService>()) {
    QObject::connect(&*spotifyservice, &SpotifyService::UpdateSpotifyAccessToken, &*app_->player()->engine(), &EngineBase::UpdateSpotifyAccessToken);
  }
#endif

  QObject::connect(radio_view_, &RadioViewContainer::Refresh, &*app_->radio_services(), &RadioServices::RefreshChannels);
  QObject::connect(radio_view_->view(), &RadioView::GetChannels, &*app_->radio_services(), &RadioServices::GetChannels);
  QObject::connect(radio_view_->view(), &RadioView::AddToPlaylistSignal, this, &MainWindow::AddToPlaylist);

  // Playlist menu
  QObject::connect(playlist_menu_, &QMenu::aboutToHide, this, &MainWindow::PlaylistMenuHidden);
  playlist_play_pause_ = playlist_menu_->addAction(tr("Play"), this, &MainWindow::PlaylistPlay);
  playlist_menu_->addAction(ui_->action_stop);
  playlist_stop_after_ = playlist_menu_->addAction(IconLoader::Load(u"media-playback-stop"_s), tr("Stop after this track"), this, &MainWindow::PlaylistStopAfter);
  playlist_queue_ = playlist_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Toggle queue status"), this, &MainWindow::PlaylistQueue);
  playlist_queue_->setShortcut(QKeySequence(u"Ctrl+D"_s));
  ui_->playlist->addAction(playlist_queue_);
  playlist_queue_play_next_ = playlist_menu_->addAction(IconLoader::Load(u"go-next"_s), tr("Queue selected tracks to play next"), this, &MainWindow::PlaylistQueuePlayNext);
  playlist_queue_play_next_->setShortcut(QKeySequence(u"Ctrl+Shift+D"_s));
  ui_->playlist->addAction(playlist_queue_play_next_);
  playlist_skip_ = playlist_menu_->addAction(IconLoader::Load(u"media-skip-forward"_s), tr("Toggle skip status"), this, &MainWindow::PlaylistSkip);
  ui_->playlist->addAction(playlist_skip_);

  playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_remove_from_playlist);
  playlist_undoredo_ = playlist_menu_->addSeparator();
  playlist_menu_->addAction(ui_->action_edit_track);
  playlist_menu_->addAction(ui_->action_edit_value);
  playlist_menu_->addAction(ui_->action_renumber_tracks);
  playlist_menu_->addAction(ui_->action_selection_set_value);
#ifdef HAVE_MUSICBRAINZ
  playlist_menu_->addAction(ui_->action_auto_complete_tags);
#endif
  playlist_rescan_songs_ = playlist_menu_->addAction(IconLoader::Load(u"view-refresh"_s), tr("Rescan song(s)..."), this, &MainWindow::RescanSongs);
  playlist_menu_->addAction(playlist_rescan_songs_);
  playlist_menu_->addAction(ui_->action_add_files_to_transcoder);
  playlist_menu_->addSeparator();
  playlist_copy_url_ = playlist_menu_->addAction(IconLoader::Load(u"edit-copy"_s), tr("Copy URL(s)..."), this, &MainWindow::PlaylistCopyUrl);
  playlist_show_in_collection_ = playlist_menu_->addAction(IconLoader::Load(u"edit-find"_s), tr("Show in collection..."), this, &MainWindow::ShowInCollection);
  playlist_open_in_browser_ = playlist_menu_->addAction(IconLoader::Load(u"document-open-folder"_s), tr("Show in file browser..."), this, &MainWindow::PlaylistOpenInBrowser);
  playlist_organize_ = playlist_menu_->addAction(IconLoader::Load(u"edit-copy"_s), tr("Organize files..."), this, &MainWindow::PlaylistMoveToCollection);
  playlist_copy_to_collection_ = playlist_menu_->addAction(IconLoader::Load(u"edit-copy"_s), tr("Copy to collection..."), this, &MainWindow::PlaylistCopyToCollection);
  playlist_move_to_collection_ = playlist_menu_->addAction(IconLoader::Load(u"go-jump"_s), tr("Move to collection..."), this, &MainWindow::PlaylistMoveToCollection);
  playlist_copy_to_device_ = playlist_menu_->addAction(IconLoader::Load(u"device"_s), tr("Copy to device..."), this, &MainWindow::PlaylistCopyToDevice);
  playlist_delete_ = playlist_menu_->addAction(IconLoader::Load(u"edit-delete"_s), tr("Delete from disk..."), this, &MainWindow::PlaylistDelete);
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

  QObject::connect(ui_->playlist, &PlaylistContainer::UndoRedoActionsChanged, this, &MainWindow::PlaylistUndoRedoChanged);

  QObject::connect(&*app_->device_manager(), &DeviceManager::DeviceError, this, &MainWindow::ShowErrorDialog);
  QObject::connect(app_->device_manager()->connected_devices_model(), &DeviceStateFilterModel::IsEmptyChanged, playlist_copy_to_device_, &QAction::setDisabled);
  playlist_copy_to_device_->setDisabled(app_->device_manager()->connected_devices_model()->rowCount() == 0);

  QObject::connect(&*app_->scrobbler()->settings(), &ScrobblerSettingsService::ScrobblingEnabledChanged, this, &MainWindow::ScrobblingEnabledChanged);
  QObject::connect(&*app_->scrobbler()->settings(), &ScrobblerSettingsService::ScrobbleButtonVisibilityChanged, this, &MainWindow::ScrobbleButtonVisibilityChanged);
  QObject::connect(&*app_->scrobbler()->settings(), &ScrobblerSettingsService::LoveButtonVisibilityChanged, this, &MainWindow::LoveButtonVisibilityChanged);

#ifdef Q_OS_MACOS
  mac::SetApplicationHandler(this);
#endif
  // Tray icon
  systemtrayicon_->SetupMenu(ui_->action_previous_track, ui_->action_play_pause, ui_->action_stop, ui_->action_stop_after_this_track, ui_->action_next_track, ui_->action_mute, ui_->action_love, ui_->action_quit);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::PlayPause, &*app_->player(), &Player::PlayPauseHelper);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::SeekForward, &*app_->player(), &Player::SeekForward);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::SeekBackward, &*app_->player(), &Player::SeekBackward);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::NextTrack, &*app_->player(), &Player::Next);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::PreviousTrack, &*app_->player(), &Player::Previous);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::ShowHide, this, &MainWindow::ToggleShowHide);
  QObject::connect(&*systemtrayicon_, &SystemTrayIcon::ChangeVolume, this, &MainWindow::VolumeWheelEvent);

  // Windows 7 thumbbar buttons
#ifdef Q_OS_WIN32
  thumbbar_->SetActions(QList<QAction*>() << ui_->action_previous_track << ui_->action_play_pause << ui_->action_stop << ui_->action_next_track << nullptr << ui_->action_love);
#endif

#if defined(HAVE_SPARKLE) || defined(HAVE_QTSPARKLE)
  QAction *action_check_updates = ui_->menu_tools->addAction(tr("Check for updates..."));
  action_check_updates->setMenuRole(QAction::ApplicationSpecificRole);
#endif

#ifdef HAVE_GLOBALSHORTCUTS
  // Global shortcuts
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Play, &*app_->player(), &Player::PlayHelper);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Pause, &*app_->player(), &Player::Pause);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::PlayPause, ui_->action_play_pause, &QAction::trigger);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Stop, ui_->action_stop, &QAction::trigger);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::StopAfter, ui_->action_stop_after_this_track, &QAction::trigger);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Next, ui_->action_next_track, &QAction::trigger);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Previous, ui_->action_previous_track, &QAction::trigger);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::RestartOrPrevious, &*app_->player(), &Player::RestartOrPrevious);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::IncVolume, &*app_->player(), &Player::VolumeUp);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::DecVolume, &*app_->player(), &Player::VolumeDown);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Mute, &*app_->player(), &Player::Mute);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::SeekForward, &*app_->player(), &Player::SeekForward);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::SeekBackward, &*app_->player(), &Player::SeekBackward);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::ShowHide, this, &MainWindow::ToggleShowHide);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::ShowOSD, &*app_->player(), &Player::ShowOSD);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::TogglePrettyOSD, &*app_->player(), &Player::TogglePrettyOSD);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::ToggleScrobbling, &*app_->scrobbler(), &AudioScrobbler::ToggleScrobbling);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::Love, &*app_->scrobbler(), &AudioScrobbler::Love);
#endif

  // Fancy tabs
  QObject::connect(ui_->tabs, &FancyTabWidget::CurrentTabChanged, this, &MainWindow::TabSwitched);

  // Context
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, context_view_, &ContextView::SongChanged);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongMetadataChanged, context_view_, &ContextView::SongChanged);
  QObject::connect(&*app_->player(), &Player::PlaylistFinished, context_view_, &ContextView::Stopped);
  QObject::connect(&*app_->player(), &Player::Playing, context_view_, &ContextView::Playing);
  QObject::connect(&*app_->player(), &Player::Stopped, context_view_, &ContextView::Stopped);
  QObject::connect(&*app_->player(), &Player::Error, context_view_, &ContextView::Error);
  QObject::connect(this, &MainWindow::AlbumCoverReady, context_view_, &ContextView::AlbumCoverLoaded);
  QObject::connect(this, &MainWindow::SearchCoverInProgress, context_view_->album_widget(), &ContextAlbum::SearchCoverInProgress);
  QObject::connect(context_view_, &ContextView::AlbumEnabledChanged, this, &MainWindow::TabSwitched);

  // Analyzer
  QObject::connect(ui_->analyzer, &AnalyzerContainer::WheelEvent, this, &MainWindow::VolumeWheelEvent);

  // Statusbar widgets
  ui_->playlist_summary->setMinimumWidth(QFontMetrics(font()).horizontalAdvance(u"WW selected of WW tracks - [ WW:WW ]"_s));
  ui_->status_bar_stack->setCurrentWidget(ui_->playlist_summary_page);
  QObject::connect(ui_->multi_loading_indicator, &MultiLoadingIndicator::TaskCountChange, this, &MainWindow::TaskCountChanged);

  ui_->track_slider->Init();

#ifdef HAVE_MOODBAR
  // Moodbar connections
  QObject::connect(&*app_->moodbar_controller(), &MoodbarController::CurrentMoodbarDataChanged, ui_->track_slider->moodbar_proxy_style(), &MoodbarProxyStyle::SetMoodbarData);
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, &*app_->moodbar_controller(), &MoodbarController::CurrentSongChanged);
  QObject::connect(&*app_->player(), &Player::Stopped, &*app_->moodbar_controller(), &MoodbarController::PlaybackStopped);
  QObject::connect(ui_->track_slider->moodbar_proxy_style(), &MoodbarProxyStyle::StyleChanged, &*app_->moodbar_loader(), &MoodbarLoader::StyleChanged);
#endif

  // Playing widget
  qLog(Debug) << "Creating playing widget";
  ui_->widget_playing->set_ideal_height(ui_->status_bar->sizeHint().height() + ui_->player_controls->sizeHint().height());
  QObject::connect(&*app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, ui_->widget_playing, &PlayingWidget::SongChanged);
  QObject::connect(&*app_->player(), &Player::PlaylistFinished, ui_->widget_playing, &PlayingWidget::Stopped);
  QObject::connect(&*app_->player(), &Player::Playing, ui_->widget_playing, &PlayingWidget::Playing);
  QObject::connect(&*app_->player(), &Player::Stopped, ui_->widget_playing, &PlayingWidget::Stopped);
  QObject::connect(&*app_->player(), &Player::Error, ui_->widget_playing, &PlayingWidget::Error);
  QObject::connect(ui_->widget_playing, &PlayingWidget::ShowAboveStatusBarChanged, this, &MainWindow::PlayingWidgetPositionChanged);
  QObject::connect(this, &MainWindow::AlbumCoverReady, ui_->widget_playing, &PlayingWidget::AlbumCoverLoaded);
  QObject::connect(this, &MainWindow::SearchCoverInProgress, ui_->widget_playing, &PlayingWidget::SearchCoverInProgress);

  QObject::connect(ui_->action_console, &QAction::triggered, this, &MainWindow::ShowConsole);
  PlayingWidgetPositionChanged(ui_->widget_playing->show_above_status_bar());

  StyleSheetLoader *css_loader = new StyleSheetLoader(this);
  css_loader->SetStyleSheet(this, u":/style/strawberry.css"_s);

  // Load playlists
  app_->playlist_manager()->Init(ui_->playlist_sequence, ui_->playlist);

  queue_view_->SetPlaylistManager(app_->playlist_manager());

  // This connection must be done after the playlists have been initialized.
  QObject::connect(this, &MainWindow::StopAfterToggled, osd_, &OSDBase::StopAfterToggle);

  // We need to connect these global shortcuts here after the playlist have been initialized
#ifdef HAVE_GLOBALSHORTCUTS
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::CycleShuffleMode, app_->playlist_manager()->sequence(), &PlaylistSequence::CycleShuffleMode);
  QObject::connect(globalshortcuts_manager_, &GlobalShortcutsManager::CycleRepeatMode, app_->playlist_manager()->sequence(), &PlaylistSequence::CycleRepeatMode);
#endif
  QObject::connect(app_->playlist_manager()->sequence(), &PlaylistSequence::RepeatModeChanged, osd_, &OSDBase::RepeatModeChanged);
  QObject::connect(app_->playlist_manager()->sequence(), &PlaylistSequence::ShuffleModeChanged, osd_, &OSDBase::ShuffleModeChanged);

  // Smart playlists
  QObject::connect(smartplaylists_view_, &SmartPlaylistsViewContainer::AddToPlaylist, this, &MainWindow::AddToPlaylist);

  ScrobbleButtonVisibilityChanged(app_->scrobbler()->scrobble_button());
  LoveButtonVisibilityChanged(app_->scrobbler()->love_button());
  ScrobblingEnabledChanged(app_->scrobbler()->enabled());

  // Last.fm ImportData
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::Finished, lastfm_import_dialog_, &LastFMImportDialog::Finished);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::FinishedWithError, lastfm_import_dialog_, &LastFMImportDialog::FinishedWithError);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdateTotal, lastfm_import_dialog_, &LastFMImportDialog::UpdateTotal);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdateProgress, lastfm_import_dialog_, &LastFMImportDialog::UpdateProgress);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdateLastPlayed, &*app_->collection_backend(), &CollectionBackend::UpdateLastPlayed);
  QObject::connect(&*app_->lastfm_import(), &LastFMImport::UpdatePlayCount, &*app_->collection_backend(), &CollectionBackend::UpdatePlayCount);

#if !defined(HAVE_AUDIOCD)
  ui_->action_open_cd->setEnabled(false);
  ui_->action_open_cd->setVisible(false);
#endif

  // Load settings
  qLog(Debug) << "Loading settings";
  settings_.beginGroup(MainWindowSettings::kSettingsGroup);

  // Set last used geometry to position window on the correct monitor
  // Set window state only if the window was last maximized
  if (settings_.contains("geometry")) {
    restoreGeometry(settings_.value("geometry").toByteArray());
  }

  if (!settings_.contains(MainWindowSettings::kSplitterState) || !ui_->splitter->restoreState(settings_.value(MainWindowSettings::kSplitterState).toByteArray())) {
    ui_->splitter->setSizes(QList<int>() << 20 << (width() - 20));
  }

  ui_->tabs->setCurrentIndex(settings_.value("current_tab", 1).toInt());
  FancyTabWidget::Mode default_mode = FancyTabWidget::Mode::LargeSidebar;
  FancyTabWidget::Mode tab_mode = static_cast<FancyTabWidget::Mode>(settings_.value("tab_mode", static_cast<int>(default_mode)).toInt());
  if (tab_mode == FancyTabWidget::Mode::None) tab_mode = default_mode;
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

#ifdef Q_OS_MACOS  // Always show the mainwindow on startup for macOS
  show();
#else
  BehaviourSettings::StartupBehaviour startupbehaviour = BehaviourSettings::StartupBehaviour::Remember;
  {
    Settings s;
    s.beginGroup(BehaviourSettings::kSettingsGroup);
    startupbehaviour = static_cast<BehaviourSettings::StartupBehaviour>(s.value(BehaviourSettings::kStartupBehaviour, static_cast<int>(BehaviourSettings::StartupBehaviour::Remember)).toInt());
    s.endGroup();
  }
  switch (startupbehaviour) {
    case BehaviourSettings::StartupBehaviour::Show:
      show();
      break;
    case BehaviourSettings::StartupBehaviour::ShowMaximized:
      setWindowState(windowState() | Qt::WindowMaximized);
      show();
      break;
    case BehaviourSettings::StartupBehaviour::ShowMinimized:
      setWindowState(windowState() | Qt::WindowMinimized);
      show();
      break;
    case BehaviourSettings::StartupBehaviour::Hide:
      if (systemtrayicon_->IsSystemTrayAvailable() && systemtrayicon_->isVisible()) {
        break;
      }
      [[fallthrough]];
    case BehaviourSettings::StartupBehaviour::Remember:
    default:{

      was_maximized_ = settings_.value(MainWindowSettings::kMaximized, true).toBool();
      if (was_maximized_) setWindowState(windowState() | Qt::WindowMaximized);

      was_minimized_ = settings_.value(MainWindowSettings::kMinimized, false).toBool();
      if (was_minimized_) setWindowState(windowState() | Qt::WindowMinimized);

      if (!systemtrayicon_->IsSystemTrayAvailable() || !systemtrayicon_->isVisible() || !settings_.value(MainWindowSettings::kHidden, false).toBool()) {
        show();
      }
      break;
    }
  }
#endif

  bool show_sidebar = settings_.value(MainWindowSettings::kShowSidebar, true).toBool();
  ui_->sidebar_layout->setVisible(show_sidebar);
  ui_->action_toggle_show_sidebar->setChecked(show_sidebar);

  QShortcut *close_window_shortcut = new QShortcut(this);
  close_window_shortcut->setKey(Qt::CTRL | Qt::Key_W);
  QObject::connect(close_window_shortcut, &QShortcut::activated, this, &MainWindow::ToggleHide);

  QAction *action_focus_search = new QAction(this);
  action_focus_search->setShortcuts(QList<QKeySequence>() << QKeySequence(u"Ctrl+F"_s));
  addAction(action_focus_search);
  QObject::connect(action_focus_search, &QAction::triggered, this, &MainWindow::FocusSearchField);

  CheckFullRescanRevisions();

  CommandlineOptionsReceived(options);

  if (app_->scrobbler()->enabled() && !app_->scrobbler()->offline()) {
    app_->scrobbler()->Submit();
  }

#ifdef HAVE_SPARKLE
  SparkleUpdater *sparkle_updater = new SparkleUpdater(action_check_updates, this);
  QObject::connect(action_check_updates, &QAction::triggered, sparkle_updater, &SparkleUpdater::CheckForUpdates);
#endif

#ifdef HAVE_QTSPARKLE
  QUrl sparkle_url(QString::fromLatin1(QTSPARKLE_URL));
  if (!sparkle_url.isEmpty()) {
    qLog(Debug) << "Creating Qt Sparkle updater";
    qtsparkle::Updater *updater = new qtsparkle::Updater(sparkle_url, this);
    updater->SetVersion(QStringLiteral(STRAWBERRY_VERSION_PACKAGE));
    QObject::connect(action_check_updates, &QAction::triggered, updater, &qtsparkle::Updater::CheckNow);
  }
#endif

#ifdef Q_OS_LINUX
  if (!Utilities::GetEnv(u"SNAP"_s).isEmpty() && !Utilities::GetEnv(u"SNAP_NAME"_s).isEmpty()) {
    Settings s;
    s.beginGroup(MainWindowSettings::kSettingsGroup);
    const bool ignore_snap = s.value("ignore_snap", false).toBool();
    s.endGroup();
    if (!ignore_snap) {
      SnapDialog *snap_dialog = new SnapDialog(this);
      snap_dialog->setAttribute(Qt::WA_DeleteOnClose);
      snap_dialog->show();
    }
  }
#endif

#if defined(Q_OS_MACOS)
  if (Utilities::ProcessTranslated()) {
    Settings s;
    s.beginGroup(MainWindowSettings::kSettingsGroup);
    const bool ignore_rosetta = s.value("ignore_rosetta", false).toBool();
    s.endGroup();
    if (!ignore_rosetta) {
      MessageDialog *rosetta_message = new MessageDialog(this);
      rosetta_message->set_settings_group(QLatin1String(MainWindowSettings::kSettingsGroup));
      rosetta_message->set_do_not_show_message_again(u"ignore_rosetta"_s);
      rosetta_message->setAttribute(Qt::WA_DeleteOnClose);
      rosetta_message->ShowMessage(tr("Strawberry running under Rosetta"), tr("You are running Strawberry under Rosetta. Running Strawberry under Rosetta is unsupported and known to have issues. You should download Strawberry for the correct CPU architecture from %1").arg(QLatin1String("<a href=\"https://downloads.strawberrymusicplayer.org/\">downloads.strawberrymusicplayer.org</a>")), IconLoader::Load(u"dialog-warning"_s));
    }
  }
#endif

  {
    bool asked_permission = true;
    Settings s;
#ifdef HAVE_QTSPARKLE
    s.beginGroup("QtSparkle");
    asked_permission = s.value("asked_permission", false).toBool();
    s.endGroup();
#endif
    if (asked_permission) {
      s.beginGroup(MainWindowSettings::kSettingsGroup);
      const bool do_not_show_sponsor_message = s.value(MainWindowSettings::kDoNotShowSponsorMessage, false).toBool();
      s.endGroup();
      if (!do_not_show_sponsor_message) {
        MessageDialog *sponsor_message = new MessageDialog(this);
        sponsor_message->set_settings_group(QLatin1String(MainWindowSettings::kSettingsGroup));
        sponsor_message->set_do_not_show_message_again(QLatin1String(MainWindowSettings::kDoNotShowSponsorMessage));
        sponsor_message->setAttribute(Qt::WA_DeleteOnClose);
        sponsor_message->ShowMessage(tr("Sponsoring Strawberry"), tr("Strawberry is free and open source software. If you like Strawberry, please consider sponsoring the project. For more information about sponsorship see our website %1").arg(u"<a href= \"https://www.strawberrymusicplayer.org/\">www.strawberrymusicplayer.org</a>"_s), IconLoader::Load(u"dialog-information"_s));
      }
    }
  }

  qLog(Debug) << "Started" << QThread::currentThread();
  initialized_ = true;

}

MainWindow::~MainWindow() {
  delete ui_;
}

void MainWindow::ReloadSettings() {

  Settings s;

#ifdef Q_OS_MACOS
  constexpr bool keeprunning_available = true;
#else
  const bool systemtray_available = systemtrayicon_->IsSystemTrayAvailable();
  s.beginGroup(BehaviourSettings::kSettingsGroup);
  const bool showtrayicon = s.value(BehaviourSettings::kShowTrayIcon, systemtray_available).toBool();
  s.endGroup();
  const bool keeprunning_available = systemtray_available && showtrayicon;
  if (systemtray_available) {
    systemtrayicon_->setVisible(showtrayicon);
  }
  if ((!showtrayicon || !systemtray_available) && !isVisible()) {
    show();
  }
#endif

  s.beginGroup(BehaviourSettings::kSettingsGroup);
  keep_running_ = keeprunning_available && s.value(BehaviourSettings::kKeepRunning, false).toBool();
  playing_widget_ = s.value(BehaviourSettings::kPlayingWidget, true).toBool();
  bool trayicon_progress = s.value(BehaviourSettings::kTrayIconProgress, false).toBool();
#ifdef HAVE_DBUS
  const bool taskbar_progress = s.value(BehaviourSettings::kTaskbarProgress, true).toBool();
#endif
  if (playing_widget_ != ui_->widget_playing->IsEnabled()) TabSwitched();
  doubleclick_addmode_ = static_cast<BehaviourSettings::AddBehaviour>(s.value(BehaviourSettings::kDoubleClickAddMode, static_cast<int>(BehaviourSettings::AddBehaviour::Append)).toInt());
  doubleclick_playmode_ = static_cast<BehaviourSettings::PlayBehaviour>(s.value(BehaviourSettings::kDoubleClickPlayMode, static_cast<int>(BehaviourSettings::PlayBehaviour::Never)).toInt());
  doubleclick_playlist_addmode_ = static_cast<BehaviourSettings::PlaylistAddBehaviour>(s.value(BehaviourSettings::kDoubleClickPlaylistAddMode, static_cast<int>(BehaviourSettings::PlayBehaviour::Never)).toInt());
  menu_playmode_ = static_cast<BehaviourSettings::PlayBehaviour>(s.value(BehaviourSettings::kMenuPlayMode, static_cast<int>(BehaviourSettings::PlayBehaviour::Never)).toInt());
  s.endGroup();

  s.beginGroup(AppearanceSettings::kSettingsGroup);
  int iconsize = s.value(AppearanceSettings::kIconSizePlayControlButtons, 32).toInt();
  s.endGroup();

  systemtrayicon_->SetTrayiconProgress(trayicon_progress);

#ifdef HAVE_DBUS
  if (taskbar_progress_ && !taskbar_progress) {
    UpdateTaskbarProgress(false);
  }
  taskbar_progress_ = taskbar_progress;
#endif

  ui_->back_button->setIconSize(QSize(iconsize, iconsize));
  ui_->pause_play_button->setIconSize(QSize(iconsize, iconsize));
  ui_->stop_button->setIconSize(QSize(iconsize, iconsize));
  ui_->forward_button->setIconSize(QSize(iconsize, iconsize));
  ui_->button_love->setIconSize(QSize(iconsize, iconsize));

  s.beginGroup(BackendSettings::kSettingsGroup);
  bool volume_control = s.value("volume_control", true).toBool();
  s.endGroup();
  if (volume_control != ui_->volume->isEnabled()) {
    ui_->volume->SetEnabled(volume_control);
    if (volume_control) {
      if (!ui_->action_mute->isVisible()) ui_->action_mute->setVisible(true);
      if (!systemtrayicon_->MuteEnabled()) systemtrayicon_->SetMuteEnabled(true);
    }
    else {
      if (ui_->action_mute->isVisible()) ui_->action_mute->setVisible(false);
      if (systemtrayicon_->MuteEnabled()) systemtrayicon_->SetMuteEnabled(false);
    }
  }

  s.beginGroup(PlaylistSettings::kSettingsGroup);
  delete_files_ = s.value(PlaylistSettings::kDeleteFiles, false).toBool();
  s.endGroup();

  osd_->ReloadSettings();

  album_cover_choice_controller_->search_cover_auto_action()->setChecked(settings_.value(MainWindowSettings::kSearchForCoverAuto, true).toBool());

#ifdef HAVE_SUBSONIC
  s.beginGroup(SubsonicSettings::kSettingsGroup);
  bool enable_subsonic = s.value(SubsonicSettings::kEnabled, false).toBool();
  s.endGroup();
  if (enable_subsonic) {
    ui_->tabs->EnableTab(subsonic_view_);
  }
  else {
    ui_->tabs->DisableTab(subsonic_view_);
  }
#endif

#ifdef HAVE_TIDAL
  s.beginGroup(TidalSettings::kSettingsGroup);
  bool enable_tidal = s.value(TidalSettings::kEnabled, false).toBool();
  s.endGroup();
  if (enable_tidal) {
    ui_->tabs->EnableTab(tidal_view_);
  }
  else {
    ui_->tabs->DisableTab(tidal_view_);
  }
#endif

#ifdef HAVE_SPOTIFY
  s.beginGroup(SpotifySettings::kSettingsGroup);
  bool enable_spotify = s.value(SpotifySettings::kEnabled, false).toBool();
  s.endGroup();
  if (enable_spotify) {
    ui_->tabs->EnableTab(spotify_view_);
  }
  else {
    ui_->tabs->DisableTab(spotify_view_);
  }
#endif

#ifdef HAVE_QOBUZ
  s.beginGroup(QobuzSettings::kSettingsGroup);
  bool enable_qobuz = s.value(QobuzSettings::kEnabled, false).toBool();
  s.endGroup();
  if (enable_qobuz) {
    ui_->tabs->EnableTab(qobuz_view_);
  }
  else {
    ui_->tabs->DisableTab(qobuz_view_);
  }
#endif

  ui_->tabs->ReloadSettings();

}

void MainWindow::ReloadAllSettings() {

  ReloadSettings();

  // Other settings
  app_->collection()->ReloadSettings();
  app_->player()->ReloadSettings();
  collection_view_->ReloadSettings();
  ui_->playlist->view()->ReloadSettings();
  app_->playlist_manager()->playlist_container()->ReloadSettings();
  app_->current_albumcover_loader()->ReloadSettingsAsync();
  album_cover_choice_controller_->ReloadSettings();
  context_view_->ReloadSettings();
  file_view_->ReloadSettings();
  queue_view_->ReloadSettings();
  playlist_list_->ReloadSettings();
  smartplaylists_view_->ReloadSettings();
  radio_view_->ReloadSettings();
  app_->streaming_services()->ReloadSettings();
  app_->radio_services()->ReloadSettings();
  app_->cover_providers()->ReloadSettings();
  app_->lyrics_providers()->ReloadSettings();
#ifdef HAVE_MOODBAR
  app_->moodbar_controller()->ReloadSettings();
  app_->moodbar_loader()->ReloadSettings();
  ui_->track_slider->moodbar_proxy_style()->ReloadSettings();
#endif
#ifdef HAVE_SUBSONIC
  subsonic_view_->ReloadSettings();
#endif
#ifdef HAVE_TIDAL
  tidal_view_->ReloadSettings();
  tidal_view_->search_view()->ReloadSettings();
#endif
#ifdef HAVE_SPOTIFY
  spotify_view_->ReloadSettings();
  spotify_view_->search_view()->ReloadSettings();
#endif
#ifdef HAVE_QOBUZ
  qobuz_view_->ReloadSettings();
  qobuz_view_->search_view()->ReloadSettings();
#endif
#ifdef HAVE_DISCORD_RPC
  discord_rich_presence_->ReloadSettings();
#endif

}

void MainWindow::RefreshStyleSheet() {
  QString contents(styleSheet());
  setStyleSheet(""_L1);
  setStyleSheet(contents);
}

void MainWindow::SaveSettings() {

  SaveGeometry();
  app_->player()->SaveVolume();
  app_->player()->SavePlaybackStatus();
  ui_->tabs->SaveSettings(QLatin1String(MainWindowSettings::kSettingsGroup));
  ui_->playlist->view()->SaveSettings();
  app_->scrobbler()->WriteCache();

  settings_.setValue(MainWindowSettings::kShowSidebar, ui_->action_toggle_show_sidebar->isChecked());
  settings_.setValue(MainWindowSettings::kSearchForCoverAuto, album_cover_choice_controller_->search_cover_auto_action()->isChecked());

}

void MainWindow::Exit() {

  ++exit_count_;

  SaveSettings();

  // Make sure Settings dialog is destroyed first.
  settings_dialog_.reset();

  if (exit_count_ > 1) {
    exit_ = true;
    QCoreApplication::quit();
  }
  else {
    if (app_->player()->engine()->is_fadeout_enabled()) {
      // To shut down the application when fadeout will be finished
      QObject::connect(&*app_->player()->engine(), &EngineBase::Finished, this, &MainWindow::DoExit);
      if (app_->player()->GetState() == EngineBase::State::Playing) {
        app_->player()->Stop();
        hide();
        if (systemtrayicon_->IsSystemTrayAvailable()) {
          systemtrayicon_->setVisible(false);
        }
        return;  // Don't quit the application now: wait for the fadeout finished signal
      }
    }

#ifdef HAVE_DBUS
    if (taskbar_progress_) {
      UpdateTaskbarProgress(false);
    }
#endif

    DoExit();
  }

}

void MainWindow::DoExit() {

  QObject::connect(app_, &Application::ExitFinished, this, &MainWindow::ExitFinished);
  app_->Exit();

}

void MainWindow::ExitFinished() {

  exit_ = true;
  QCoreApplication::quit();

}

void MainWindow::PlaylistsLoaded() {

  playlists_loaded_ = true;

  if (options_.has_value()) {
    CommandlineOptionsReceived(options_.value());
    options_.reset();
  }

  app_->player()->PlaylistsLoaded();

}

void MainWindow::MediaStopped() {

  setWindowTitle(u"Strawberry Music Player"_s);

  ui_->action_stop->setEnabled(false);
  ui_->action_stop_after_this_track->setEnabled(false);
  ui_->action_play_pause->setIcon(IconLoader::Load(u"media-playback-start"_s));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

  ui_->action_love->setEnabled(false);
  ui_->button_love->setEnabled(false);
  systemtrayicon_->LoveStateChanged(false);

  if (track_position_timer_->isActive()) {
    track_position_timer_->stop();
  }
  if (track_slider_timer_->isActive()) {
    track_slider_timer_->stop();
  }
  ui_->track_slider->SetStopped();
  systemtrayicon_->SetProgress(0);
  systemtrayicon_->SetStopped();

#ifdef HAVE_DBUS
  if (taskbar_progress_) {
    UpdateTaskbarProgress(false);
  }
#endif

  song_playing_ = Song();
  song_ = Song();
  album_cover_ = AlbumCoverImageResult();

  app_->scrobbler()->ClearPlaying();

}

void MainWindow::MediaPaused() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load(u"media-playback-start"_s));
  ui_->action_play_pause->setText(tr("Play"));

  ui_->action_play_pause->setEnabled(true);

  if (!track_position_timer_->isActive()) {
    track_position_timer_->start();
  }
  if (!track_slider_timer_->isActive()) {
    track_slider_timer_->start();
  }

  systemtrayicon_->SetPaused();

}

void MainWindow::MediaPlaying() {

  ui_->action_stop->setEnabled(true);
  ui_->action_stop_after_this_track->setEnabled(true);
  ui_->action_play_pause->setIcon(IconLoader::Load(u"media-playback-pause"_s));
  ui_->action_play_pause->setText(tr("Pause"));

  bool enable_play_pause(false);
  bool can_seek(false);

  PlaylistItemPtr item(app_->player()->GetCurrentItem());
  if (item) {
    enable_play_pause = !(item->options() & PlaylistItem::Option::PauseDisabled);
    can_seek = !(item->options() & PlaylistItem::Option::SeekDisabled);
  }
  ui_->action_play_pause->setEnabled(enable_play_pause);
  ui_->track_slider->SetCanSeek(can_seek);
  systemtrayicon_->SetPlaying(enable_play_pause);

  if (!track_position_timer_->isActive()) {
    track_position_timer_->start();
  }
  if (!track_slider_timer_->isActive()) {
    track_slider_timer_->start();
  }

  UpdateTrackPosition();

}

void MainWindow::SendNowPlaying() {

  // Send now playing to scrobble services
  Playlist *playlist = app_->playlist_manager()->active();
  if (app_->scrobbler()->enabled() && playlist && playlist->current_item() && playlist->current_item()->EffectiveMetadata().is_metadata_good()) {
    app_->scrobbler()->UpdateNowPlaying(playlist->current_item()->EffectiveMetadata());
    ui_->action_love->setEnabled(true);
    ui_->button_love->setEnabled(true);
    systemtrayicon_->LoveStateChanged(true);
  }

}

void MainWindow::VolumeChanged(const uint volume) {
  ui_->action_mute->setChecked(volume == 0);
  systemtrayicon_->MuteButtonStateChanged(volume == 0);
}

void MainWindow::SongChanged(const Song &song) {

  qLog(Debug) << "Song changed to" << song.artist() << song.album() << song.PrettyTitle();

  song_playing_ = song;
  song_ = song;
  setWindowTitle(song.PrettyTitleWithArtist());
  systemtrayicon_->SetProgress(0);

#ifdef HAVE_DBUS
  if (taskbar_progress_) {
    UpdateTaskbarProgress(false);
  }
#endif

  SendNowPlaying();

  const bool enable_change_art = song.is_local_collection_song() && !song.effective_albumartist().isEmpty() && !song.album().isEmpty();
  album_cover_choice_controller_->show_cover_action()->setEnabled(song.has_valid_art() && !song.art_unset());
  album_cover_choice_controller_->cover_to_file_action()->setEnabled(song.has_valid_art() && !song.art_unset());
  album_cover_choice_controller_->cover_from_file_action()->setEnabled(enable_change_art);
  album_cover_choice_controller_->cover_from_url_action()->setEnabled(enable_change_art);
  album_cover_choice_controller_->search_for_cover_action()->setEnabled(app_->cover_providers()->HasAnyProviders() && enable_change_art);
  album_cover_choice_controller_->unset_cover_action()->setEnabled(enable_change_art && !song.art_unset());
  album_cover_choice_controller_->clear_cover_action()->setEnabled(enable_change_art && !song.art_manual().isEmpty());
  album_cover_choice_controller_->delete_cover_action()->setEnabled(enable_change_art && (song.art_embedded() || !song.art_automatic().isEmpty() || !song.art_manual().isEmpty()));

}

void MainWindow::TrackSkipped(PlaylistItemPtr item) {

  // If it was a collection item then we have to increment its skipped count in the database.

  if (item && item->IsLocalCollectionItem() && item->EffectiveMetadata().id() != -1) {

    Song song = item->EffectiveMetadata();
    const qint64 position = app_->player()->engine()->position_nanosec();
    const qint64 length = app_->player()->engine()->length_nanosec();
    const float percentage = (length == 0 ? 1 : static_cast<float>(position) / static_cast<float>(length));

    const qint64 seconds_left = (length - position) / kNsecPerSec;
    const qint64 seconds_total = length / kNsecPerSec;

    if (((0.05 * static_cast<double>(seconds_total) > 60.0 && percentage < 0.98) || percentage < 0.95) && seconds_left > 5) {  // Never count the skip if under 5 seconds left
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
  settings_.setValue(MainWindowSettings::kShowSidebar, checked);

}

void MainWindow::ToggleSearchCoverAuto(const bool checked) {
  settings_.setValue(MainWindowSettings::kSearchForCoverAuto, checked);
}

void MainWindow::SaveGeometry() {

  if (!initialized_) return;

  settings_.setValue(MainWindowSettings::kMaximized, isMaximized());
  settings_.setValue(MainWindowSettings::kMinimized, isMinimized());
  settings_.setValue(MainWindowSettings::kHidden, isHidden());
  settings_.setValue(MainWindowSettings::kGeometry, saveGeometry());
  settings_.setValue(MainWindowSettings::kSplitterState, ui_->splitter->saveState());

}

void MainWindow::PlayIndex(const QModelIndex &idx, Playlist::AutoScroll autoscroll) {

  if (!idx.isValid()) return;

  int row = idx.row();
  if (idx.model() == app_->playlist_manager()->current()->filter()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    row = app_->playlist_manager()->current()->filter()->mapToSource(idx).row();
  }

  app_->playlist_manager()->SetActiveToCurrent();
  app_->player()->PlayAt(row, false, 0, EngineBase::TrackChangeType::Manual, autoscroll, true);

}

void MainWindow::PlaylistDoubleClick(const QModelIndex &idx) {

  if (!idx.isValid()) return;

  QModelIndex source_idx = idx;
  if (idx.model() == app_->playlist_manager()->current()->filter()) {
    // The index was in the proxy model (might've been filtered), so we need to get the actual row in the source model.
    source_idx = app_->playlist_manager()->current()->filter()->mapToSource(idx);
  }

  switch (doubleclick_playlist_addmode_) {
    case BehaviourSettings::PlaylistAddBehaviour::Play:
      app_->playlist_manager()->SetActiveToCurrent();
      app_->player()->PlayAt(source_idx.row(), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Never, true, true);
      break;

    case BehaviourSettings::PlaylistAddBehaviour::Enqueue:
      app_->playlist_manager()->current()->queue()->ToggleTracks(QModelIndexList() << source_idx);
      if (app_->player()->GetState() != EngineBase::State::Playing) {
        app_->playlist_manager()->SetActiveToCurrent();
        app_->player()->PlayAt(app_->playlist_manager()->current()->queue()->TakeNext(), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Never, true);
      }
      break;
  }

}

void MainWindow::VolumeWheelEvent(const int delta) {
  ui_->volume->HandleWheel(delta);
}

void MainWindow::ToggleShowHide() {

  if (isHidden()) {
    SetHiddenInTray(false);
  }
  else if (isActiveWindow()) {
    setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    SetHiddenInTray(true);
  }
  else if (isMinimized()) {
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

void MainWindow::ToggleHide() {
  if (isVisible()) SetHiddenInTray(true);
}

void MainWindow::StopAfterCurrent() {
  app_->playlist_manager()->current()->StopAfter(app_->playlist_manager()->current()->current_row());
  Q_EMIT StopAfterToggled(app_->playlist_manager()->active()->stop_after_current());
}

void MainWindow::showEvent(QShowEvent *e) {

  if (error_dialog_ && error_dialog_->isVisible() && error_dialog_->isMinimized()) {
    error_dialog_->raise();
    error_dialog_->activateWindow();
  }

  QMainWindow::showEvent(e);

}

void MainWindow::hideEvent(QHideEvent *e) {

  // Some window managers don't remember maximized state between
  // calls to hide() and show(), so we have to remember it ourself.

  was_maximized_ = isMaximized();
  was_minimized_ = isMinimized();

  QMainWindow::hideEvent(e);

}

void MainWindow::closeEvent(QCloseEvent *e) {

  if (!exit_ && (!systemtrayicon_->IsSystemTrayAvailable() || !systemtrayicon_->isVisible() || !keep_running_)) {
    Exit();
  }

  QMainWindow::closeEvent(e);

}

void MainWindow::SetHiddenInTray(const bool hidden) {

  if (hidden && isVisible()) {
    if (systemtrayicon_->IsSystemTrayAvailable() && systemtrayicon_->isVisible() && keep_running_) {
      close();
    }
    else {
      showMinimized();
    }
  }
  else if (!hidden && isHidden()) {
    if (was_minimized_) {
      showMinimized();
    }
    else if (was_maximized_) {
      showMaximized();
    }
    else {
      show();
    }
  }

}

void MainWindow::FilePathChanged(const QString &path) {
  settings_.setValue("file_path", path);
}

void MainWindow::Seeked(const qint64 microseconds) {

  const qint64 position = microseconds / kUsecPerSec;
  const qint64 length = app_->player()->GetCurrentItem()->EffectiveMetadata().length_nanosec() / kNsecPerSec;
  systemtrayicon_->SetProgress(static_cast<int>(static_cast<double>(position) / static_cast<double>(length) * 100.0));

#ifdef HAVE_DBUS
  if (taskbar_progress_) {
    UpdateTaskbarProgress(true, static_cast<double>(position) / static_cast<double>(length));
  }
#endif

}

void MainWindow::UpdateTrackPosition() {

  PlaylistItemPtr item(app_->player()->GetCurrentItem());
  if (!item) return;

  const qint64 length = (item->EffectiveMetadata().length_nanosec() / kNsecPerSec);
  if (length <= 0) return;
  const int position = std::floor(static_cast<float>(app_->player()->engine()->position_nanosec()) / static_cast<float>(kNsecPerSec) + 0.5);

  // Update the tray icon every 10 seconds
  if (position % 10 == 0) systemtrayicon_->SetProgress(static_cast<int>(static_cast<double>(position) / static_cast<double>(length) * 100.0));

#ifdef HAVE_DBUS
  if (taskbar_progress_) {
    UpdateTaskbarProgress(true, static_cast<double>(position) / static_cast<double>(length));
  }
#endif

  // Send Scrobble
  if (app_->scrobbler()->enabled() && item->EffectiveMetadata().is_metadata_good()) {
    Playlist *playlist = app_->playlist_manager()->active();
    if (playlist && !playlist->scrobbled()) {
      const qint64 scrobble_point = (playlist->scrobble_point_nanosec() / kNsecPerSec);
      if (position >= scrobble_point) {
        app_->scrobbler()->Scrobble(item->EffectiveMetadata(), scrobble_point);
        playlist->set_scrobbled(true);
      }
    }
  }

}

void MainWindow::UpdateTrackSliderPosition() {

  PlaylistItemPtr item(app_->player()->GetCurrentItem());

  const int slider_position = std::floor(static_cast<float>(app_->player()->engine()->position_nanosec()) / kNsecPerMsec);
  const int slider_length = static_cast<int>(app_->player()->engine()->length_nanosec() / kNsecPerMsec);

  // Update the slider
  ui_->track_slider->SetValue(slider_position, slider_length);

}

#ifdef HAVE_DBUS
void MainWindow::UpdateTaskbarProgress(const bool visible, const double progress) {

  QVariantMap map;
  QDBusMessage msg = QDBusMessage::createSignal(u"/org/strawberrymusicplayer/strawberry"_s, u"com.canonical.Unity.LauncherEntry"_s, u"Update"_s);

  map.insert(u"progress-visible"_s, visible);
  map.insert(u"progress"_s, progress);
  msg << u"application://org.strawberrymusicplayer.strawberry.desktop"_s << map;

  QDBusConnection::sessionBus().send(msg);

}
#endif

void MainWindow::ApplyAddBehaviour(const BehaviourSettings::AddBehaviour b, MimeData *mimedata) {

  switch (b) {
    case BehaviourSettings::AddBehaviour::Append:
      mimedata->clear_first_ = false;
      mimedata->enqueue_now_ = false;
      break;

    case BehaviourSettings::AddBehaviour::Enqueue:
      mimedata->clear_first_ = false;
      mimedata->enqueue_now_ = true;
      break;

    case BehaviourSettings::AddBehaviour::Load:
      mimedata->clear_first_ = true;
      mimedata->enqueue_now_ = false;
      break;

    case BehaviourSettings::AddBehaviour::OpenInNew:
      mimedata->open_in_new_playlist_ = true;
      break;
  }
}

void MainWindow::ApplyPlayBehaviour(const BehaviourSettings::PlayBehaviour b, MimeData *mimedata) const {

  switch (b) {
    case BehaviourSettings::PlayBehaviour::Always:
      mimedata->play_now_ = true;
      break;

    case BehaviourSettings::PlayBehaviour::Never:
      mimedata->play_now_ = false;
      break;

    case BehaviourSettings::PlayBehaviour::IfStopped:
      mimedata->play_now_ = !(app_->player()->GetState() == EngineBase::State::Playing);
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

void MainWindow::AddToPlaylistFromAction(QAction *action) {

  const int destination = action->data().toInt();
  PlaylistItemPtrList items;
  SongList songs;

  // Get the selected playlist items
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    items << item;
    songs << item->EffectiveMetadata();
  }

  // We're creating a new playlist
  if (destination == -1) {
    // Save the current playlist to reactivate it
    const int current_id = app_->playlist_manager()->current_id();
    // Get the name from selection
    app_->playlist_manager()->New(Song::GetNameForNewPlaylist(songs));
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

void MainWindow::PlaylistRightClick(const QPoint global_pos, const QModelIndex &index) {

  QModelIndex source_index = index;
  if (index.model() == app_->playlist_manager()->current()->filter()) {
    source_index = app_->playlist_manager()->current()->filter()->mapToSource(index);
  }

  playlist_menu_index_ = source_index;

  // Is this song currently playing?
  if (app_->playlist_manager()->current()->current_row() == source_index.row() && app_->player()->GetState() == EngineBase::State::Playing) {
    playlist_play_pause_->setText(tr("Pause"));
    playlist_play_pause_->setIcon(IconLoader::Load(u"media-playback-pause"_s));
  }
  else {
    playlist_play_pause_->setText(tr("Play"));
    playlist_play_pause_->setIcon(IconLoader::Load(u"media-playback-start"_s));
  }

  // Are we allowed to pause?
  if (source_index.isValid()) {
    playlist_play_pause_->setEnabled(app_->playlist_manager()->current()->current_row() != source_index.row() || !(app_->playlist_manager()->current()->item_at(source_index.row())->options() & PlaylistItem::Option::PauseDisabled));
  }
  else {
    playlist_play_pause_->setEnabled(false);
  }

  playlist_stop_after_->setEnabled(source_index.isValid());

  // Are any of the selected songs editable or queued?
  const QModelIndexList selection = ui_->playlist->view()->selectionModel()->selectedRows();
  bool cue_selected = false;
  qint64 selected = ui_->playlist->view()->selectionModel()->selectedRows().count();
  int editable = 0;
  int in_queue = 0;
  int not_in_queue = 0;
  int in_skipped = 0;
  int not_in_skipped = 0;
  int local_songs = 0;

  for (const QModelIndex &idx : selection) {

    const QModelIndex src_idx = app_->playlist_manager()->current()->filter()->mapToSource(idx);
    if (!src_idx.isValid()) continue;

    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(src_idx.row());
    if (!item) continue;

    if (item->EffectiveMetadata().url().isLocalFile()) ++local_songs;

    if (item->EffectiveMetadata().has_cue()) {
      cue_selected = true;
    }
    else if (item->EffectiveMetadata().IsEditable()) {
      ++editable;
    }

    if (src_idx.data(Playlist::Role_QueuePosition).toInt() == -1) ++not_in_queue;
    else ++in_queue;

    if (item->GetShouldSkip()) ++in_skipped;
    else ++not_in_skipped;

  }

  // this is available when we have one or many files and at least one of those is not CUE related
  ui_->action_edit_track->setEnabled(local_songs > 0 && editable > 0);
  ui_->action_edit_track->setVisible(local_songs > 0 && editable > 0);
#ifdef HAVE_MUSICBRAINZ
  ui_->action_auto_complete_tags->setEnabled(local_songs > 0 && editable > 0);
  ui_->action_auto_complete_tags->setVisible(local_songs > 0 && editable > 0);
#endif

  playlist_rescan_songs_->setEnabled(local_songs > 0 && editable > 0);
  playlist_rescan_songs_->setVisible(local_songs > 0 && editable > 0);

  ui_->action_add_files_to_transcoder->setEnabled(local_songs > 0 && editable > 0);
  ui_->action_add_files_to_transcoder->setVisible(local_songs > 0 && editable > 0);

  playlist_open_in_browser_->setVisible(selected > 0 && local_songs == selected);

  const bool track_column = (index.column() == static_cast<int>(Playlist::Column::Track));
  ui_->action_renumber_tracks->setVisible(local_songs > 0 && !cue_selected && editable >= 2 && track_column);
  ui_->action_selection_set_value->setVisible(editable >= 2 && !cue_selected && !track_column);
  ui_->action_edit_value->setVisible(editable > 0 && !cue_selected);
  ui_->action_remove_from_playlist->setEnabled(selected > 0);
  ui_->action_remove_from_playlist->setVisible(selected > 0);

  playlist_show_in_collection_->setVisible(false);
  playlist_copy_to_collection_->setVisible(false);
  playlist_move_to_collection_->setVisible(false);
  playlist_copy_to_device_->setVisible(false);
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

    if (selected > 1) {
      playlist_queue_play_next_->setText(tr("Queue selected tracks to play next"));
    }
    else {
      playlist_queue_play_next_->setText(tr("Queue to play next"));
    }

    if (in_skipped == 1 && not_in_skipped == 0) playlist_skip_->setText(tr("Unskip track"));
    else if (in_skipped > 1 && not_in_skipped == 0) playlist_skip_->setText(tr("Unskip selected tracks"));
    else if (in_skipped == 0 && not_in_skipped == 1) playlist_skip_->setText(tr("Skip track"));
    else if (in_skipped == 0 && not_in_skipped > 1) playlist_skip_->setText(tr("Skip selected tracks"));
    else playlist_skip_->setText(tr("Toggle skip status"));
  }

  if (not_in_queue == 0) playlist_queue_->setIcon(IconLoader::Load(u"go-previous"_s));
  else playlist_queue_->setIcon(IconLoader::Load(u"go-next"_s));

  if (in_skipped < selected) playlist_skip_->setIcon(IconLoader::Load(u"media-skip-forward"_s));
  else playlist_skip_->setIcon(IconLoader::Load(u"media-playback-start"_s));


  if (!index.isValid()) {
    ui_->action_selection_set_value->setVisible(false);
    ui_->action_edit_value->setVisible(false);
  }
  else {

    Playlist::Column column = static_cast<Playlist::Column>(index.column());
    bool column_is_editable = (Playlist::column_is_editable(column) && editable > 0 && !cue_selected);

    ui_->action_selection_set_value->setVisible(ui_->action_selection_set_value->isVisible() && column_is_editable);
    ui_->action_edit_value->setVisible(ui_->action_edit_value->isVisible() && column_is_editable);

    QString column_name = Playlist::column_name(column);
    QString column_value = app_->playlist_manager()->current()->data(source_index).toString();
    if (column_value.length() > 25) column_value = column_value.left(25) + u"..."_s;

    ui_->action_selection_set_value->setText(tr("Set %1 to \"%2\"...").arg(column_name.toLower(), column_value));
    ui_->action_edit_value->setText(tr("Edit tag \"%1\"...").arg(column_name));

    // Is it a collection item?
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (item && item->IsLocalCollectionItem() && item->EffectiveMetadata().id() != -1) {
      playlist_organize_->setVisible(local_songs > 0 && editable > 0 && !cue_selected);
      playlist_show_in_collection_->setVisible(true);
      playlist_open_in_browser_->setVisible(true);
    }
    else {
      playlist_copy_to_collection_->setVisible(local_songs > 0);
      playlist_move_to_collection_->setVisible(local_songs > 0);
    }

    playlist_copy_to_device_->setVisible(local_songs > 0);

    playlist_delete_->setVisible(delete_files_ && local_songs > 0);

    // Remove old item actions, if any.
    for (QAction *action : std::as_const(playlistitem_actions_)) {
      playlist_menu_->removeAction(action);
    }

    // Get the new item actions, and add them
    playlistitem_actions_ = item->actions();
    playlistitem_actions_separator_->setVisible(!playlistitem_actions_.isEmpty());
    playlist_menu_->insertActions(playlistitem_actions_separator_, playlistitem_actions_);
  }

  // If it isn't the first time we right click, we need to remove the menu previously created
  if (playlist_add_to_another_ != nullptr) {
    playlist_menu_->removeAction(playlist_add_to_another_);
    delete playlist_add_to_another_;
    playlist_add_to_another_ = nullptr;
  }

  // Create the playlist submenu if songs are selected.
  if (selected > 0) {
    QMenu *add_to_another_menu = new QMenu(tr("Add to another playlist"), this);
    add_to_another_menu->setIcon(IconLoader::Load(u"list-add"_s));

    const QList<int> playlist_ids = app_->playlist_manager()->playlist_ids();
    for (const int playlist_id : playlist_ids) {
      // Don't add the current playlist
      if (playlist_id != app_->playlist_manager()->current()->id()) {
        QAction *existing_playlist = new QAction(this);
        existing_playlist->setText(app_->playlist_manager()->playlist_name(playlist_id));
        existing_playlist->setData(playlist_id);
        add_to_another_menu->addAction(existing_playlist);
      }
    }

    add_to_another_menu->addSeparator();
    // Add to a new playlist
    QAction *new_playlist = new QAction(this);
    new_playlist->setText(tr("New playlist"));
    new_playlist->setData(-1);  // fake id
    add_to_another_menu->addAction(new_playlist);
    playlist_add_to_another_ = playlist_menu_->insertMenu(ui_->action_remove_from_playlist, add_to_another_menu);

    QObject::connect(add_to_another_menu, &QMenu::triggered, this, &MainWindow::AddToPlaylistFromAction);

  }

  playlist_menu_->popup(global_pos);

}

void MainWindow::PlaylistPlay() {

  if (app_->playlist_manager()->current()->current_row() == playlist_menu_index_.row()) {
    app_->player()->PlayPause(0, Playlist::AutoScroll::Never);
  }
  else {
    PlayIndex(playlist_menu_index_, Playlist::AutoScroll::Never);
  }

}

void MainWindow::PlaylistStopAfter() {
  app_->playlist_manager()->current()->StopAfter(playlist_menu_index_.row());
}

void MainWindow::RescanSongs() {

  SongList songs;

  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item(app_->playlist_manager()->current()->item_at(source_index.row()));
    if (!item) continue;
    if (item->IsLocalCollectionItem()) {
      songs << item->EffectiveMetadata();
    }
    else if (item->EffectiveMetadata().source() == Song::Source::LocalFile) {
      QPersistentModelIndex persistent_index = QPersistentModelIndex(source_index);
      app_->playlist_manager()->current()->ItemReload(persistent_index, item->OriginalMetadata(), false);
    }
  }

  if (!songs.isEmpty()) {
    app_->collection()->Rescan(songs);
  }

}

void MainWindow::EditTracks() {

  SongList songs;
  PlaylistItemPtrList items;

  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
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

  const PlaylistItemPtrList items = edit_tag_dialog_->playlist_items();
  for (PlaylistItemPtr item : items) {
    item->Reload();
  }

  // FIXME: This is really lame but we don't know what rows have changed.
  ui_->playlist->view()->update();

  app_->playlist_manager()->current()->ScheduleSaveAsync();

}

void MainWindow::RenumberTracks() {

  QModelIndexList indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  int track = 1;

  // Get the index list in order
  std::stable_sort(indexes.begin(), indexes.end());

  // If first selected song has a track number set, start from that offset
  if (!indexes.isEmpty()) {
    const Song first_song = app_->playlist_manager()->current()->item_at(indexes[0].row())->OriginalMetadata();
    if (first_song.track() > 0) track = first_song.track();
  }

  for (const QModelIndex &proxy_index : std::as_const(indexes)) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (song.IsEditable()) {
      song.set_track(track);
      TagReaderReplyPtr reply = app_->tagreader_client()->WriteFileAsync(song.url().toLocalFile(), song);
      QPersistentModelIndex persistent_index = QPersistentModelIndex(source_index);
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, persistent_index, connection]() {
        SongSaveComplete(reply, persistent_index);
        QObject::disconnect(*connection);
      }, Qt::QueuedConnection);
    }
    ++track;
  }

}

void MainWindow::SongSaveComplete(TagReaderReplyPtr reply, const QPersistentModelIndex &idx) {

  if (reply->success() && idx.isValid()) {
    app_->playlist_manager()->current()->ReloadItems(QList<int>() << idx.row());
  }

}

void MainWindow::SelectionSetValue() {

  Playlist::Column column = static_cast<Playlist::Column>(playlist_menu_index_.column());
  QVariant column_value = app_->playlist_manager()->current()->data(playlist_menu_index_);

  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    Song song = item->OriginalMetadata();
    if (!song.is_valid()) continue;
    if (song.url().isLocalFile() && Playlist::set_column_value(song, column, column_value)) {
      TagReaderReplyPtr reply = app_->tagreader_client()->WriteFileAsync(song.url().toLocalFile(), song);
      QPersistentModelIndex persistent_index = QPersistentModelIndex(source_index);
      SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
      *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [this, reply, persistent_index, connection]() {
        SongSaveComplete(reply, persistent_index);
        QObject::disconnect(*connection);
      }, Qt::QueuedConnection);
    }
    else if (song.source() == Song::Source::Stream) {
      app_->playlist_manager()->current()->setData(source_index, column_value, 0);
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
      if (!Playlist::column_is_editable(static_cast<Playlist::Column>(i))) continue;
      column = i;
      break;
    }
  }

  ui_->playlist->view()->edit(current.sibling(current.row(), column));

}

void MainWindow::AddFile() {

  // Last used directory
  QString directory = settings_.value("add_media_path", QDir::currentPath()).toString();

  PlaylistParser parser(app_->tagreader_client(), app_->collection_backend());

  // Show dialog
  const QStringList filenames = QFileDialog::getOpenFileNames(this, tr("Add file"), directory, QStringLiteral("%1 (%2);;%3;;%4").arg(tr("Music"), QLatin1String(kFileFilter), parser.filters(PlaylistParser::Type::Load), tr(kAllFilesFilterSpec)));

  if (filenames.isEmpty()) return;

  // Save last used directory
  settings_.setValue("add_media_path", filenames[0]);

  // Convert to URLs
  QList<QUrl> urls;
  urls.reserve(filenames.count());
  for (const QString &path : filenames) {
    urls << QUrl::fromLocalFile(QDir::cleanPath(path));
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
  mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(QDir::cleanPath(directory)));
  AddToPlaylist(mimedata);

}

void MainWindow::AddCDTracks() {

  MimeData *mimedata = new MimeData;
  // We are putting empty data, but we specify cdda mimetype to indicate that we want to load audio cd tracks
  mimedata->open_in_new_playlist_ = true;
  mimedata->setData(QLatin1String(Playlist::kCddaMimeType), QByteArray());
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
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (item && item->IsLocalCollectionItem()) {
      songs << item->OriginalMetadata();
      break;
    }
  }
  QString search;
  if (!songs.isEmpty()) {
    search = "artist:"_L1 + songs.first().artist() + " album:"_L1 + songs.first().album();
  }
  collection_view_->filter_widget()->ShowInCollection(search);

}

void MainWindow::PlaylistRemoveCurrent() {
  ui_->playlist->view()->RemoveSelected();
}

void MainWindow::PlaylistClearCurrent() {

  if (app_->playlist_manager()->current()->rowCount() > Playlist::kUndoItemLimit) {
    QMessageBox messagebox(QMessageBox::Warning, tr("Clear playlist"), tr("Playlist has %1 songs, too large to undo, are you sure you want to clear the playlist?").arg(app_->playlist_manager()->current()->rowCount()), QMessageBox::Ok | QMessageBox::Cancel);
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

void MainWindow::PlaylistEditFinished(const int playlist_id, const QModelIndex &idx) {

  if (app_->playlist_manager()->current() && playlist_id == app_->playlist_manager()->current()->id() && idx == playlist_menu_index_) {
    SelectionSetValue();
  }

}

void MainWindow::CommandlineOptionsReceived(const QByteArray &string_options) {

  CommandlineOptions options;
  options.Load(string_options);

  if (options.is_empty()) {
    raise();
    show();
    activateWindow();
    return;
  }

  CommandlineOptionsReceived(options);

}

void MainWindow::CommandlineOptionsReceived(const CommandlineOptions &options) {

  if (!playlists_loaded_) {
    options_ = options;
    return;
  }

  switch (options.player_action()) {
    case CommandlineOptions::PlayerAction::Play:
      if (options.urls().empty()) {
        app_->player()->Play();
      }
      break;
    case CommandlineOptions::PlayerAction::PlayPause:
      app_->player()->PlayPause(0, Playlist::AutoScroll::Maybe);
      break;
    case CommandlineOptions::PlayerAction::Pause:
      app_->player()->Pause();
      break;
    case CommandlineOptions::PlayerAction::Stop:
      app_->player()->Stop();
      break;
    case CommandlineOptions::PlayerAction::StopAfterCurrent:
      app_->player()->StopAfterCurrent();
      break;
    case CommandlineOptions::PlayerAction::Previous:
      app_->player()->Previous();
      break;
    case CommandlineOptions::PlayerAction::Next:
      app_->player()->Next();
      break;
    case CommandlineOptions::PlayerAction::PlayPlaylist:
      if (options.playlist_name().isEmpty()) {
        qLog(Error) << "ERROR: playlist name missing";
      }
      else {
        app_->player()->PlayPlaylist(options.playlist_name());
      }
      break;
    case CommandlineOptions::PlayerAction::RestartOrPrevious:
      app_->player()->RestartOrPrevious();
      break;

    case CommandlineOptions::PlayerAction::ResizeWindow:{
      if (options.window_size().contains(u'x') && options.window_size().length() >= 4) {
        QString str_w = options.window_size().left(options.window_size().indexOf(u'x'));
        QString str_h = options.window_size().right(options.window_size().length() - options.window_size().indexOf(u'x') - 1);
        bool w_ok = false;
        bool h_ok = false;
        int w = str_w.toInt(&w_ok);
        int h = str_h.toInt(&h_ok);
        if (w_ok && h_ok) {
          QSize window_size(w, h);
          if (window_size.isValid()) {
            QScreen *screen = Utilities::GetScreen(this);
            if (screen) {
              const QRect sr = screen->availableGeometry();
              window_size = window_size.boundedTo(sr.size());
              if (window_size.width() >= sr.width() && window_size.height() >= sr.height()) {
                resize(window_size);
                showMaximized();
              }
              else {
                showNormal();
                resize(window_size);
                const QRect wr({}, size().boundedTo(sr.size()));
                resize(wr.size());
                move(sr.center() - wr.center());
              }
            }
          }
        }
      }
      break;
    }

    case CommandlineOptions::PlayerAction::None:
      break;

  }

  if (!options.urls().empty()) {

#ifdef HAVE_TIDAL
    const QList<QUrl> urls = options.urls();
    for (const QUrl &url : urls) {
      if (url.scheme() == "tidal"_L1 && url.host() == "login"_L1) {
        Q_EMIT AuthorizationUrlReceived(url);
        return;
      }
    }
#endif
    MimeData *mimedata = new MimeData;
    mimedata->setUrls(options.urls());
    // Behaviour depends on command line options, so set it here
    mimedata->override_user_settings_ = true;

    if (options.player_action() == CommandlineOptions::PlayerAction::Play) mimedata->play_now_ = true;
    else ApplyPlayBehaviour(doubleclick_playmode_, mimedata);

    switch (options.url_list_action()) {
      case CommandlineOptions::UrlListAction::Load:
        mimedata->clear_first_ = true;
        break;
      case CommandlineOptions::UrlListAction::Append:
        // Nothing to do
        break;
      case CommandlineOptions::UrlListAction::None:
        ApplyAddBehaviour(doubleclick_addmode_, mimedata);
        break;
      case CommandlineOptions::UrlListAction::CreateNew:
        mimedata->name_for_new_playlist_ = options.playlist_name();
        ApplyAddBehaviour(BehaviourSettings::AddBehaviour::OpenInNew, mimedata);
        break;
    }

    AddToPlaylist(mimedata);
  }

  if (options.set_volume() != -1) app_->player()->SetVolume(static_cast<uint>(qBound(0, options.set_volume(), 100)));

  if (options.volume_modifier() != 0) {
    app_->player()->SetVolume(static_cast<uint>(qBound(0, static_cast<int>(app_->player()->GetVolume()) + options.volume_modifier(), 100)));
  }

  if (options.seek_to() != -1) {
    app_->player()->SeekTo(static_cast<quint64>(options.seek_to()));
  }
  else if (options.seek_by() != 0) {
    app_->player()->SeekTo(static_cast<quint64>(app_->player()->engine()->position_nanosec() / kNsecPerSec + options.seek_by()));
  }

  if (options.play_track_at() != -1) app_->player()->PlayAt(options.play_track_at(), false, 0, EngineBase::TrackChangeType::Manual, Playlist::AutoScroll::Maybe, true);

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

  if (QFile::exists(url)) {
    MimeData *mimedata = new MimeData;
    mimedata->setUrls(QList<QUrl>() << QUrl::fromLocalFile(url));
    AddToPlaylist(mimedata);
    return true;
  }
#ifdef HAVE_TIDAL
  if (url.startsWith("tidal://login"_L1)) {
    Q_EMIT AuthorizationUrlReceived(QUrl(url));
    return true;
  }
#endif

  qLog(Error) << "Can't open" << url;

  return false;

}

void MainWindow::PlaylistUndoRedoChanged(QAction *undo, QAction *redo) {

  playlist_menu_->insertAction(playlist_undoredo_, undo);
  playlist_menu_->insertAction(playlist_undoredo_, redo);
}

void MainWindow::AddFilesToTranscoder() {

  QStringList filenames;

  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
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

}

void MainWindow::OpenCollectionSettingsDialog() {
  settings_dialog_->OpenAtPage(SettingsDialog::Page::Collection);
}

void MainWindow::OpenServiceSettingsDialog(const Song::Source source) {

  switch (source) {
    case Song::Source::Collection:
      settings_dialog_->OpenAtPage(SettingsDialog::Page::Collection);
      break;
    case Song::Source::Subsonic:
      settings_dialog_->OpenAtPage(SettingsDialog::Page::Subsonic);
      break;
    case Song::Source::Tidal:
      settings_dialog_->OpenAtPage(SettingsDialog::Page::Tidal);
      break;
    case Song::Source::Qobuz:
      settings_dialog_->OpenAtPage(SettingsDialog::Page::Qobuz);
      break;
    case Song::Source::Spotify:
      settings_dialog_->OpenAtPage(SettingsDialog::Page::Spotify);
      break;
    default:
      break;
  }

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

  organize_dialog_->SetDestinationModel(app_->device_manager()->connected_devices_model(), true);
  organize_dialog_->SetCopy(true);
  if (organize_dialog_->SetUrls(urls)) {
    organize_dialog_->show();
    organize_dialog_->raise();
  }
  else {
    QMessageBox::warning(this, tr("Error"), tr("None of the selected songs were suitable for copying to a device"));
  }

}

void MainWindow::EditFileTags(const QList<QUrl> &urls) {

  SongList songs;
  songs.reserve(urls.count());
  for (const QUrl &url : urls) {
    Song song;
    song.set_url(url);
    song.set_valid(true);
    song.set_filetype(Song::FileType::MPEG);
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
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
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
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    urls << QUrl(source_index.sibling(source_index.row(), static_cast<int>(Playlist::Column::URL)).data().toString());
  }

  Utilities::OpenInFileBrowser(urls);

}

void MainWindow::PlaylistCopyUrl() {

  QList<QUrl> urls;
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
    if (!source_index.isValid()) continue;
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_index.row());
    if (!item) continue;
    urls << item->EffectiveUrl();
  }

  if (urls.count() > 0) {
    QMimeData mime_data;
    mime_data.setUrls(urls);
    QApplication::clipboard()->setText(mime_data.text());
  }

}

void MainWindow::PlaylistQueue() {

  const QModelIndexList selected_rows = ui_->playlist->view()->selectionModel()->selectedRows();
  QModelIndexList indexes;
  indexes.reserve(selected_rows.count());
  for (const QModelIndex &proxy_index : selected_rows) {
    indexes << app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->queue()->ToggleTracks(indexes);

}

void MainWindow::PlaylistQueuePlayNext() {

  const QModelIndexList selected_rows = ui_->playlist->view()->selectionModel()->selectedRows();
  QModelIndexList indexes;
  indexes.reserve(selected_rows.count());
  for (const QModelIndex &proxy_index : selected_rows) {
    indexes << app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->queue()->InsertFirst(indexes);

}

void MainWindow::PlaylistSkip() {

  const QModelIndexList selected_rows = ui_->playlist->view()->selectionModel()->selectedRows();
  QModelIndexList indexes;
  indexes.reserve(selected_rows.count());
  for (const QModelIndex &proxy_index : selected_rows) {
    indexes << app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
  }

  app_->playlist_manager()->current()->SkipTracks(indexes);

}

void MainWindow::PlaylistCopyToDevice() {

  SongList songs;

  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
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

}

void MainWindow::ChangeCollectionFilterMode(QAction *action) {

  if (action == collection_show_duplicates_) {
    collection_view_->filter_widget()->SetFilterMode(CollectionFilterOptions::FilterMode::Duplicates);
  }
  else if (action == collection_show_untagged_) {
    collection_view_->filter_widget()->SetFilterMode(CollectionFilterOptions::FilterMode::Untagged);
  }
  else {
    collection_view_->filter_widget()->SetFilterMode(CollectionFilterOptions::FilterMode::All);
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

  SettingsDialog *settings_dialog = new SettingsDialog(app_->player(),
                                                       app_->device_finders(),
                                                       app_->collection(),
                                                       app_->cover_providers(),
                                                       app_->lyrics_providers(),
                                                       app_->scrobbler(),
                                                       app_->streaming_services(),
#ifdef HAVE_GLOBALSHORTCUTS
                                                       globalshortcuts_manager_,
#endif
                                                       osd_,
                                                       this);

  // Settings
  QObject::connect(settings_dialog, &SettingsDialog::ReloadSettings, this, &MainWindow::ReloadAllSettings);

  // Allows custom notification preview
  QObject::connect(settings_dialog, &SettingsDialog::NotificationPreview, this, &MainWindow::HandleNotificationPreview);

  return settings_dialog;

}

void MainWindow::OpenSettingsDialog() {

  settings_dialog_->show();
  settings_dialog_->raise();

}

void MainWindow::OpenSettingsDialogAtPage(const SettingsDialog::Page page) {
  settings_dialog_->OpenAtPage(page);
}

EditTagDialog *MainWindow::CreateEditTagDialog() {

  EditTagDialog *edit_tag_dialog = new EditTagDialog(app_->network(), app_->tagreader_client(), app_->collection_backend(), app_->albumcover_loader(), app_->current_albumcover_loader(), app_->cover_providers(), app_->lyrics_providers(), app_->streaming_services());
  QObject::connect(edit_tag_dialog, &EditTagDialog::accepted, this, &MainWindow::EditTagDialogAccepted);
  QObject::connect(edit_tag_dialog, &EditTagDialog::Error, this, &MainWindow::ShowErrorDialog);
  return edit_tag_dialog;

}

void MainWindow::ShowAboutDialog() {

  about_dialog_->show();
  about_dialog_->raise();

}

void MainWindow::ShowTranscodeDialog() {

  transcode_dialog_->show();
  transcode_dialog_->raise();

}

void MainWindow::ShowErrorDialog(const QString &message) {
  error_dialog_->ShowMessage(message);
}

void MainWindow::CheckFullRescanRevisions() {

  int from = app_->database()->startup_schema_version();
  int to = app_->database()->current_schema_version();

  // If we're restoring DB from scratch or nothing has changed, do nothing
  if (from == 0 || from == to) {
    return;
  }

  // Collect all reasons
  QSet<QString> reasons;
  for (int i = from; i <= to; ++i) {
    QString reason = app_->collection()->full_rescan_reason(i);
    if (!reason.isEmpty()) {
      reasons.insert(reason);
    }
  }

  // If we have any...
  if (!reasons.isEmpty()) {
    QString message = tr("The version of Strawberry you've just updated to requires a full collection rescan because of the new features listed below:") + u"<ul>"_s;
    for (const QString &reason : reasons) {
      message += "<li>"_L1 + reason + "</li>"_L1;
    }
    message += "</ul>"_L1 + tr("Would you like to run a full rescan right now?");
    if (QMessageBox::question(this, tr("Collection rescan notice"), message, QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
      app_->collection()->FullScan();
    }
  }
}

void MainWindow::PlaylistViewSelectionModelChanged() {

  QObject::connect(ui_->playlist->view()->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::PlaylistCurrentChanged);

}

void MainWindow::PlaylistCurrentChanged(const QModelIndex &proxy_current) {

  const QModelIndex source_current = app_->playlist_manager()->current()->filter()->mapToSource(proxy_current);

  // If the user moves the current index using the keyboard and then presses
  // F2, we don't want that editing the last column that was right clicked on.
  if (source_current != playlist_menu_index_) playlist_menu_index_ = QModelIndex();

}

void MainWindow::Raise() {

  show();
  activateWindow();

}

#ifdef Q_OS_WIN32
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {

  if (exit_count_ == 0 && message) {
    MSG *msg = static_cast<MSG*>(message);
    thumbbar_->HandleWinEvent(msg);
  }
  return QMainWindow::nativeEvent(eventType, message, result);
}
#endif  // Q_OS_WIN32

void MainWindow::AutoCompleteTags() {

#ifdef HAVE_MUSICBRAINZ

  autocomplete_tag_items_.clear();

  // Create the tag fetching stuff if it hasn't been already
  if (!tag_fetcher_) {
    tag_fetcher_ = make_unique<TagFetcher>(app_->network());
    track_selection_dialog_ = make_unique<TrackSelectionDialog>(app_->tagreader_client());
    track_selection_dialog_->set_save_on_close(true);

    QObject::connect(&*tag_fetcher_, &TagFetcher::ResultAvailable, &*track_selection_dialog_, &TrackSelectionDialog::FetchTagFinished, Qt::QueuedConnection);
    QObject::connect(&*tag_fetcher_, &TagFetcher::Progress, &*track_selection_dialog_, &TrackSelectionDialog::FetchTagProgress);
    QObject::connect(&*track_selection_dialog_, &TrackSelectionDialog::accepted, this, &MainWindow::AutoCompleteTagsAccepted);
    QObject::connect(&*track_selection_dialog_, &TrackSelectionDialog::finished, &*tag_fetcher_, &TagFetcher::Cancel);
    QObject::connect(&*track_selection_dialog_, &TrackSelectionDialog::Error, this, &MainWindow::ShowErrorDialog);
  }

  // Get the selected songs and start fetching tags for them
  SongList songs;
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_index : proxy_indexes) {
    const QModelIndex source_index = app_->playlist_manager()->current()->filter()->mapToSource(proxy_index);
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

  for (PlaylistItemPtr item : std::as_const(autocomplete_tag_items_)) {
    item->Reload();
  }
  autocomplete_tag_items_.clear();

  // This is really lame but we don't know what rows have changed
  ui_->playlist->view()->update();

}

void MainWindow::HandleNotificationPreview(const OSDSettings::Type type, const QString &line1, const QString &line2) {

  if (!app_->playlist_manager()->current()->GetAllSongs().isEmpty()) {
    // Show a preview notification for the first song in the current playlist
    osd_->ShowPreview(type, line1, line2, app_->playlist_manager()->current()->GetAllSongs().first());
  }
  else {
    qLog(Debug) << "The current playlist is empty, showing a fake song";
    // Create a fake song
    Song fake(Song::Source::LocalFile);
    fake.Init(u"Title"_s, u"Artist"_s, u"Album"_s, 123);
    fake.set_genre(u"Classical"_s);
    fake.set_composer(u"Anonymous"_s);
    fake.set_performer(u"Anonymous"_s);
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
    app_->player()->PlayPause(0, Playlist::AutoScroll::Never);
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
  album_cover_choice_controller_->SaveCoverToFileManual(song_, album_cover_);
}

void MainWindow::UnsetCover() {
  album_cover_choice_controller_->UnsetCover(&song_);
}

void MainWindow::ClearCover() {
  album_cover_choice_controller_->ClearCover(&song_);
}

void MainWindow::DeleteCover() {
  album_cover_choice_controller_->DeleteCover(&song_, true);
}

void MainWindow::ShowCover() {
  album_cover_choice_controller_->ShowCover(song_, album_cover_.image);
}

void MainWindow::SearchCoverAutomatically() {

  GetCoverAutomatically();

}

void MainWindow::AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result) {

  if (song != song_playing_) return;

  song_ = song;
  album_cover_ = result.album_cover;

  Q_EMIT AlbumCoverReady(song, result.album_cover.image);

  const bool enable_change_art = song.is_local_collection_song() && !song.effective_albumartist().isEmpty() && !song.album().isEmpty();
  album_cover_choice_controller_->show_cover_action()->setEnabled(result.success && result.type != AlbumCoverLoaderResult::Type::Unset);
  album_cover_choice_controller_->cover_to_file_action()->setEnabled(result.success && result.type != AlbumCoverLoaderResult::Type::Unset);
  album_cover_choice_controller_->cover_from_file_action()->setEnabled(enable_change_art);
  album_cover_choice_controller_->cover_from_url_action()->setEnabled(enable_change_art);
  album_cover_choice_controller_->search_for_cover_action()->setEnabled(app_->cover_providers()->HasAnyProviders() && enable_change_art);
  album_cover_choice_controller_->unset_cover_action()->setEnabled(enable_change_art && !song.art_unset());
  album_cover_choice_controller_->clear_cover_action()->setEnabled(enable_change_art && !song.art_manual().isEmpty());
  album_cover_choice_controller_->delete_cover_action()->setEnabled(enable_change_art && result.success && result.type != AlbumCoverLoaderResult::Type::Unset);

  GetCoverAutomatically();

}

void MainWindow::GetCoverAutomatically() {

  // Search for cover automatically?
  const bool search = album_cover_choice_controller_->search_cover_auto_action()->isChecked() &&
                      !song_.art_unset() &&
                      !song_.art_embedded() &&
                      !song_.art_automatic_is_valid() &&
                      !song_.art_manual_is_valid() &&
                      !song_.effective_albumartist().isEmpty() &&
                      !song_.effective_album().isEmpty();

  if (search) {
    Q_EMIT SearchCoverInProgress();
    album_cover_choice_controller_->SearchCoverAutomatically(song_);
  }

}

void MainWindow::ScrobblingEnabledChanged(const bool value) {
  if (app_->scrobbler()->scrobble_button()) SetToggleScrobblingIcon(value);
}

void MainWindow::ScrobbleButtonVisibilityChanged(const bool value) {

  ui_->button_scrobble->setVisible(value);
  ui_->action_toggle_scrobbling->setVisible(value);
  if (value) SetToggleScrobblingIcon(app_->scrobbler()->enabled());

}

void MainWindow::LoveButtonVisibilityChanged(const bool value) {

  if (value)
    ui_->widget_love->show();
  else
    ui_->widget_love->hide();

  systemtrayicon_->LoveVisibilityChanged(value);

}

void MainWindow::SetToggleScrobblingIcon(const bool value) {

  if (value) {
    if (app_->playlist_manager()->active() && app_->playlist_manager()->active()->scrobbled())
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load(u"scrobble"_s, true, 22));
    else
      ui_->action_toggle_scrobbling->setIcon(IconLoader::Load(u"scrobble"_s, true, 22));  // TODO: Create a faint version of the icon
  }
  else {
    ui_->action_toggle_scrobbling->setIcon(IconLoader::Load(u"scrobble-disabled"_s, true, 22));
  }

}

void MainWindow::Love() {

  app_->scrobbler()->Love();
  ui_->button_love->setEnabled(false);
  ui_->action_love->setEnabled(false);
  systemtrayicon_->LoveStateChanged(false);

}

void MainWindow::PlaylistDelete() {

  if (!delete_files_) return;

  SongList selected_songs;
  QStringList files;
  bool is_current_item = false;
  const QModelIndexList proxy_indexes = ui_->playlist->view()->selectionModel()->selectedRows();
  for (const QModelIndex &proxy_idx : proxy_indexes) {
    QModelIndex source_idx = app_->playlist_manager()->current()->filter()->mapToSource(proxy_idx);
    PlaylistItemPtr item = app_->playlist_manager()->current()->item_at(source_idx.row());
    if (!item || !item->EffectiveMetadata().url().isLocalFile()) continue;
    QString filename = item->EffectiveMetadata().url().toLocalFile();
    if (files.contains(filename)) continue;
    selected_songs << item->EffectiveMetadata();
    files << filename;
    if (item == app_->player()->GetCurrentItem()) is_current_item = true;
  }
  if (selected_songs.isEmpty()) return;

  if (DeleteConfirmationDialog::warning(files) != QDialogButtonBox::Yes) return;

  if (app_->player()->GetState() == EngineBase::State::Playing && app_->playlist_manager()->current()->rowCount() == selected_songs.count()) {
    app_->player()->Stop();
  }

  ui_->playlist->view()->RemoveSelected();

  if (app_->player()->GetState() == EngineBase::State::Playing && is_current_item) {
    app_->player()->Next();
  }

  SharedPtr<MusicStorage> storage = make_shared<FilesystemMusicStorage>(Song::Source::LocalFile, u"/"_s);
  DeleteFiles *delete_files = new DeleteFiles(app_->task_manager(), storage, true);
  QObject::connect(delete_files, &DeleteFiles::Finished, this, &MainWindow::DeleteFilesFinished);
  delete_files->Start(selected_songs);

}

void MainWindow::DeleteFilesFinished(const SongList &songs_with_errors) {

  if (songs_with_errors.isEmpty()) return;

  OrganizeErrorDialog *dialog = new OrganizeErrorDialog(this);
  dialog->Show(OrganizeErrorDialog::OperationType::Delete, songs_with_errors);
  // It deletes itself when the user closes it

}

void MainWindow::FocusSearchField() {

  if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(collection_view_) && !collection_view_->filter_widget()->SearchFieldHasFocus()) {
    collection_view_->filter_widget()->FocusSearchField();
  }
#ifdef HAVE_SUBSONIC
  else if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(subsonic_view_) && !subsonic_view_->SearchFieldHasFocus()) {
    subsonic_view_->FocusSearchField();
  }
#endif
#ifdef HAVE_TIDAL
  else if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(tidal_view_) && !tidal_view_->SearchFieldHasFocus()) {
    tidal_view_->FocusSearchField();
  }
#endif
#ifdef HAVE_SPOTIFY
  else if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(spotify_view_) && !spotify_view_->SearchFieldHasFocus()) {
    spotify_view_->FocusSearchField();
  }
#endif
#ifdef HAVE_QOBUZ
  else if (ui_->tabs->currentIndex() == ui_->tabs->IndexOfTab(qobuz_view_) && !qobuz_view_->SearchFieldHasFocus()) {
    qobuz_view_->FocusSearchField();
  }
#endif
  else if (!ui_->playlist->SearchFieldHasFocus()) {
    ui_->playlist->FocusSearchField();
  }

}
