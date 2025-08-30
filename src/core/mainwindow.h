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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

#include <optional>

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QSortFilterProxyModel>
#include <QAbstractItemModel>
#include <QPersistentModelIndex>
#include <QMenu>
#include <QAction>
#include <QPoint>
#include <QMimeData>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <QSettings>
#include <QtEvents>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "includes/lazy.h"
#include "core/platforminterface.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/commandlineoptions.h"
#include "tagreader/tagreaderclient.h"
#include "osd/osdbase.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"
#include "constants/behavioursettings.h"
#include "covermanager/albumcoverloaderresult.h"
#include "covermanager/albumcoverimageresult.h"

class About;
class Console;
class AlbumCoverManager;
class Application;
class ContextView;
class CollectionViewContainer;
class CollectionFilter;
class AlbumCoverChoiceController;
class CommandlineOptions;
class DeviceViewContainer;
class EditTagDialog;
class Equalizer;
class ErrorDialog;
class FileView;
class GlobalShortcutsManager;
class MimeData;
class OrganizeDialog;
class PlaylistListContainer;
class QueueView;
class SystemTrayIcon;
#ifdef HAVE_MUSICBRAINZ
class TagFetcher;
#endif
class TrackSelectionDialog;
class TranscodeDialog;
class Ui_MainWindow;
class StreamingSongsView;
class StreamingTabsView;
class SmartPlaylistsViewContainer;
#ifdef Q_OS_WIN32
class Windows7ThumbBar;
#endif
class AddStreamDialog;
class LastFMImportDialog;
class RadioViewContainer;

#ifdef HAVE_DISCORD_RPC
namespace discord {
class RichPresence;
}
#endif

class MainWindow : public QMainWindow, public PlatformInterface {
  Q_OBJECT

 public:
  explicit MainWindow(Application *app,
                      SharedPtr<SystemTrayIcon> systemtrayicon,
                      OSDBase *osd,
#ifdef HAVE_DISCORD_RPC
                      discord::RichPresence *discord_rich_presence,
#endif
                      const CommandlineOptions &options,
                      QWidget *parent = nullptr);
  ~MainWindow() override;

  void SetHiddenInTray(const bool hidden);
  void CommandlineOptionsReceived(const CommandlineOptions &options);

 protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void closeEvent(QCloseEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
#ifdef Q_OS_WIN32
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#endif

  // PlatformInterface
  void Activate() override;
  bool LoadUrl(const QString &url) override;

 Q_SIGNALS:
  void AlbumCoverReady(const Song &song, const QImage &image);
  void SearchCoverInProgress();
  // Signals that stop playing after track was toggled.
  void StopAfterToggled(const bool stop);

  void AuthorizationUrlReceived(const QUrl &url);

 private Q_SLOTS:
  void PlaylistsLoaded();

  void FilePathChanged(const QString &path);

  void MediaStopped();
  void MediaPaused();
  void MediaPlaying();
  void TrackSkipped(PlaylistItemPtr item);
  void ForceShowOSD(const Song &song, const bool toggle);

  void PlaylistMenuHidden();
  void PlaylistRightClick(const QPoint global_pos, const QModelIndex &index);
  void PlaylistCurrentChanged(const QModelIndex &current);
  void PlaylistViewSelectionModelChanged();
  void PlaylistPlay();
  void PlaylistStopAfter();
  void PlaylistQueue();
  void PlaylistQueuePlayNext();
  void PlaylistSkip();
  void PlaylistRemoveCurrent();
  void PlaylistEditFinished(const int playlist_id, const QModelIndex &idx);
  void PlaylistClearCurrent();
  void RescanSongs();
  void EditTracks();
  void EditTagDialogAccepted();
  void RenumberTracks();
  void SelectionSetValue();
  void EditValue();
  void AutoCompleteTags();
  void AutoCompleteTagsAccepted();
  void PlaylistUndoRedoChanged(QAction *undo, QAction *redo);
  void AddFilesToTranscoder();

  void PlaylistCopyToCollection();
  void PlaylistMoveToCollection();
  void PlaylistCopyToDevice();
  void PlaylistOrganizeSelected(const bool copy);
  void PlaylistOpenInBrowser();
  void PlaylistCopyUrl();
  void ShowInCollection();

  void ChangeCollectionFilterMode(QAction *action);

  void PlayIndex(const QModelIndex &idx, Playlist::AutoScroll autoscroll);
  void PlaylistDoubleClick(const QModelIndex &idx);
  void StopAfterCurrent();

  void SongChanged(const Song &song);
  void VolumeChanged(const uint volume);

  void CopyFilesToCollection(const QList<QUrl> &urls);
  void MoveFilesToCollection(const QList<QUrl> &urls);
  void CopyFilesToDevice(const QList<QUrl> &urls);
  void EditFileTags(const QList<QUrl> &urls);

  void AddToPlaylist(QMimeData *q_mimedata);
  void AddToPlaylistFromAction(QAction *action);

  void VolumeWheelEvent(const int delta);
  void ToggleShowHide();
  void ToggleHide();

  void Seeked(const qint64 microseconds);
  void UpdateTrackPosition();
  void UpdateTrackSliderPosition();

  void TaskCountChanged(const int count);

  void OpenCollectionSettingsDialog();
  void OpenServiceSettingsDialog(const Song::Source source);

  void ReloadSettings();
  void ReloadAllSettings();
  void RefreshStyleSheet();
  void SetHiddenInTray() { SetHiddenInTray(true); }

  void AddFile();
  void AddFolder();
  void AddCDTracks();
  void AddStream();
  void AddStreamAccepted();

  void PlayingWidgetPositionChanged(const bool above_status_bar);

  void SongSaveComplete(TagReaderReplyPtr reply, const QPersistentModelIndex &idx);

  void ShowCoverManager();
  void ShowEqualizer();

  void ShowAboutDialog();
  void ShowErrorDialog(const QString &message);
  void ShowTranscodeDialog();
  SettingsDialog *CreateSettingsDialog();
  EditTagDialog *CreateEditTagDialog();
  void OpenSettingsDialog();
  void OpenSettingsDialogAtPage(const SettingsDialog::Page page);

  void TabSwitched();
  void ToggleSidebar(const bool checked);
  void ToggleSearchCoverAuto(const bool checked);
  void SaveGeometry();

  void Exit();
  void DoExit();

  void HandleNotificationPreview(const OSDSettings::Type type, const QString &line1, const QString &line2);

  void ShowConsole();

  void LoadCoverFromFile();
  void SaveCoverToFile();
  void LoadCoverFromURL();
  void SearchForCover();
  void UnsetCover();
  void ClearCover();
  void DeleteCover();
  void ShowCover();
  void SearchCoverAutomatically();
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result);

  void ScrobblingEnabledChanged(const bool value);
  void ScrobbleButtonVisibilityChanged(const bool value);
  void LoveButtonVisibilityChanged(const bool value);
  void SendNowPlaying();
  void Love();

  void ExitFinished();

  void PlaylistDelete();

  void FocusSearchField();

  void DeleteFilesFinished(const SongList &songs_with_errors);

 public Q_SLOTS:
  void CommandlineOptionsReceived(const QByteArray &string_options);
  void Raise();

 private:

  void SaveSettings();

  static void ApplyAddBehaviour(const BehaviourSettings::AddBehaviour b, MimeData *mimedata);
  void ApplyPlayBehaviour(const BehaviourSettings::PlayBehaviour b, MimeData *mimedata) const;

  void CheckFullRescanRevisions();

  // creates the icon by painting the full one depending on the current position
  QPixmap CreateOverlayedIcon(const int position, const int scrobble_point);

  void GetCoverAutomatically();

  void SetToggleScrobblingIcon(const bool value);

#ifdef HAVE_DBUS
  void UpdateTaskbarProgress(const bool visible, const double progress = 0);
#endif

 private:
  Ui_MainWindow *ui_;
#ifdef Q_OS_WIN32
  Windows7ThumbBar *thumbbar_;
#endif

  Application *app_;
  SharedPtr<SystemTrayIcon> systemtrayicon_;
  OSDBase *osd_;
#ifdef HAVE_DISCORD_RPC
  discord::RichPresence *discord_rich_presence_;
#endif
  Lazy<About> about_dialog_;
  Lazy<Console> console_;
  Lazy<EditTagDialog> edit_tag_dialog_;
  AlbumCoverChoiceController *album_cover_choice_controller_;

  GlobalShortcutsManager *globalshortcuts_manager_;

  ContextView *context_view_;
  CollectionViewContainer *collection_view_;
  FileView *file_view_;
  DeviceViewContainer *device_view_;
  PlaylistListContainer *playlist_list_;
  QueueView *queue_view_;

  Lazy<ErrorDialog> error_dialog_;
  Lazy<SettingsDialog> settings_dialog_;
  Lazy<AlbumCoverManager> cover_manager_;
  SharedPtr<Equalizer> equalizer_;
  Lazy<OrganizeDialog> organize_dialog_;
  Lazy<TranscodeDialog> transcode_dialog_;
  Lazy<AddStreamDialog> add_stream_dialog_;

#ifdef HAVE_MUSICBRAINZ
  ScopedPtr<TagFetcher> tag_fetcher_;
#endif
  ScopedPtr<TrackSelectionDialog> track_selection_dialog_;
  PlaylistItemPtrList autocomplete_tag_items_;

  SmartPlaylistsViewContainer *smartplaylists_view_;

#ifdef HAVE_SUBSONIC
  StreamingSongsView *subsonic_view_;
#endif
#ifdef HAVE_TIDAL
  StreamingTabsView *tidal_view_;
#endif
#ifdef HAVE_SPOTIFY
  StreamingTabsView *spotify_view_;
#endif
#ifdef HAVE_QOBUZ
  StreamingTabsView *qobuz_view_;
#endif

  RadioViewContainer *radio_view_;

  LastFMImportDialog *lastfm_import_dialog_;

  QAction *collection_show_all_;
  QAction *collection_show_duplicates_;
  QAction *collection_show_untagged_;

  QMenu *playlist_menu_;
  QAction *playlist_play_pause_;
  QAction *playlist_stop_after_;
  QAction *playlist_undoredo_;
  QAction *playlist_copy_url_;
  QAction *playlist_show_in_collection_;
  QAction *playlist_copy_to_collection_;
  QAction *playlist_move_to_collection_;
  QAction *playlist_open_in_browser_;
  QAction *playlist_organize_;
  QAction *playlist_copy_to_device_;
  QAction *playlist_delete_;
  QAction *playlist_queue_;
  QAction *playlist_queue_play_next_;
  QAction *playlist_skip_;
  QAction *playlist_add_to_another_;
  QList<QAction*> playlistitem_actions_;
  QAction *playlistitem_actions_separator_;
  QAction *playlist_rescan_songs_;

  QModelIndex playlist_menu_index_;

  QTimer *track_position_timer_;
  QTimer *track_slider_timer_;
  Settings settings_;

  bool keep_running_;
  bool playing_widget_;
#ifdef HAVE_DBUS
  bool taskbar_progress_;
#endif
  BehaviourSettings::AddBehaviour doubleclick_addmode_;
  BehaviourSettings::PlayBehaviour doubleclick_playmode_;
  BehaviourSettings::PlaylistAddBehaviour doubleclick_playlist_addmode_;
  BehaviourSettings::PlayBehaviour menu_playmode_;

  bool initialized_;
  bool was_maximized_;
  bool was_minimized_;

  Song song_;
  Song song_playing_;
  AlbumCoverImageResult album_cover_;
  bool exit_;
  int exit_count_;
  bool playlists_loaded_;
  bool delete_files_;
  std::optional<CommandlineOptions> options_;

};

#endif  // MAINWINDOW_H
