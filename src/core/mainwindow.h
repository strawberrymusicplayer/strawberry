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

#include <memory>

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

#include "core/lazy.h"
#include "core/tagreaderclient.h"
#include "core/song.h"
#include "engine/enginetype.h"
#include "engine/engine_fwd.h"
#include "mac_startup.h"
#include "osd/osdbase.h"
#include "collection/collectionmodel.h"
#include "playlist/playlist.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"
#include "settings/behavioursettingspage.h"
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
#ifndef Q_OS_WIN
class DeviceViewContainer;
#endif
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
#ifdef HAVE_GSTREAMER
class TranscodeDialog;
#endif
class Ui_MainWindow;
class InternetSongsView;
class InternetTabsView;
class SmartPlaylistsViewContainer;
#ifdef Q_OS_WIN
class Windows7ThumbBar;
#endif
class AddStreamDialog;
class LastFMImportDialog;
class RadioViewContainer;

class MainWindow : public QMainWindow, public PlatformInterface {
  Q_OBJECT

 public:
  explicit MainWindow(Application *app, std::shared_ptr<SystemTrayIcon> tray_icon, OSDBase *osd, const CommandlineOptions &options, QWidget *parent = nullptr);
  ~MainWindow() override;

  static const char *kSettingsGroup;
  static const char *kAllFilesFilterSpec;

  void SetHiddenInTray(const bool hidden);
  void CommandlineOptionsReceived(const CommandlineOptions &options);

 protected:
  void showEvent(QShowEvent *e) override;
  void closeEvent(QCloseEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
#ifdef Q_OS_WIN
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
  bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
#endif

  // PlatformInterface
  void Activate() override;
  bool LoadUrl(const QString &url) override;

 signals:
  void AlbumCoverReady(Song song, QImage image);
  void SearchCoverInProgress();
  // Signals that stop playing after track was toggled.
  void StopAfterToggled(bool stop);

  void AuthorizationUrlReceived(QUrl url);

 private slots:
  void FilePathChanged(const QString &path);

  void EngineChanged(Engine::EngineType enginetype);
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

  void ChangeCollectionQueryMode(QAction *action);

  void PlayIndex(const QModelIndex &idx, Playlist::AutoScroll autoscroll);
  void PlaylistDoubleClick(const QModelIndex &idx);
  void StopAfterCurrent();

  void SongChanged(const Song &song);
  void VolumeChanged(const int volume);

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

  void ShowCollectionConfig();
  void ReloadSettings();
  void ReloadAllSettings();
  void RefreshStyleSheet();
  void SetHiddenInTray() { SetHiddenInTray(true); }

  void AddFile();
  void AddFolder();
  void AddCDTracks();
  void AddStream();
  void AddStreamAccepted();

  void CheckForUpdates();

  void PlayingWidgetPositionChanged(const bool above_status_bar);

  void SongSaveComplete(TagReaderReply *reply, const QPersistentModelIndex &idx);

  void ShowCoverManager();
  void ShowEqualizer();

  void ShowAboutDialog();
  void ShowErrorDialog(const QString &message);
  void ShowTranscodeDialog();
  SettingsDialog *CreateSettingsDialog();
  EditTagDialog *CreateEditTagDialog();
  void OpenSettingsDialog();
  void OpenSettingsDialogAtPage(SettingsDialog::Page page);

  void TabSwitched();
  void ToggleSidebar(const bool checked);
  void ToggleSearchCoverAuto(const bool checked);
  void SaveGeometry();
  void SavePlaybackStatus();
  void LoadPlaybackStatus();
  void ResumePlayback();

  void Exit();
  void DoExit();

  void HandleNotificationPreview(const OSDBase::Behaviour type, const QString &line1, const QString &line2);

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

 public slots:
  void CommandlineOptionsReceived(const quint32 instanceId, const QByteArray &string_options);
  void Raise();

 private:

  void SaveSettings();

  static void ApplyAddBehaviour(const BehaviourSettingsPage::AddBehaviour b, MimeData *mimedata);
  void ApplyPlayBehaviour(const BehaviourSettingsPage::PlayBehaviour b, MimeData *mimedata) const;

  void CheckFullRescanRevisions();

  // creates the icon by painting the full one depending on the current position
  QPixmap CreateOverlayedIcon(const int position, const int scrobble_point);

  void GetCoverAutomatically();

  void SetToggleScrobblingIcon(const bool value);

 private:
  Ui_MainWindow *ui_;
#ifdef Q_OS_WIN
  Windows7ThumbBar *thumbbar_;
#endif

  Application *app_;
  std::shared_ptr<SystemTrayIcon> tray_icon_;
  OSDBase *osd_;
  Lazy<About> about_dialog_;
  Lazy<Console> console_;
  Lazy<EditTagDialog> edit_tag_dialog_;
  AlbumCoverChoiceController *album_cover_choice_controller_;

  GlobalShortcutsManager *globalshortcuts_manager_;

  ContextView *context_view_;
  CollectionViewContainer *collection_view_;
  FileView *file_view_;
#ifndef Q_OS_WIN
  DeviceViewContainer *device_view_;
#endif
  PlaylistListContainer *playlist_list_;
  QueueView *queue_view_;

  Lazy<ErrorDialog> error_dialog_;
  Lazy<SettingsDialog> settings_dialog_;
  Lazy<AlbumCoverManager> cover_manager_;
  std::unique_ptr<Equalizer> equalizer_;
  Lazy<OrganizeDialog> organize_dialog_;
#ifdef HAVE_GSTREAMER
  Lazy<TranscodeDialog> transcode_dialog_;
#endif
  Lazy<AddStreamDialog> add_stream_dialog_;

#ifdef HAVE_MUSICBRAINZ
  std::unique_ptr<TagFetcher> tag_fetcher_;
#endif
  std::unique_ptr<TrackSelectionDialog> track_selection_dialog_;
  PlaylistItemList autocomplete_tag_items_;

  SmartPlaylistsViewContainer *smartplaylists_view_;

  InternetSongsView *subsonic_view_;
  InternetTabsView *tidal_view_;
  InternetTabsView *qobuz_view_;

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
#ifndef Q_OS_WIN
  QAction *playlist_copy_to_device_;
#endif
  QAction *playlist_delete_;
  QAction *playlist_queue_;
  QAction *playlist_queue_play_next_;
  QAction *playlist_skip_;
  QAction *playlist_add_to_another_;
  QList<QAction*> playlistitem_actions_;
  QAction *playlistitem_actions_separator_;
  QAction *playlist_rescan_songs_;

  QModelIndex playlist_menu_index_;

  CollectionFilter *collection_filter_;

  QTimer *track_position_timer_;
  QTimer *track_slider_timer_;
  QSettings settings_;

  bool keep_running_;
  bool playing_widget_;
  BehaviourSettingsPage::AddBehaviour doubleclick_addmode_;
  BehaviourSettingsPage::PlayBehaviour doubleclick_playmode_;
  BehaviourSettingsPage::PlaylistAddBehaviour doubleclick_playlist_addmode_;
  BehaviourSettingsPage::PlayBehaviour menu_playmode_;

  bool initialized_;
  bool was_maximized_;
  bool was_minimized_;
  bool hidden_;

  Song song_;
  Song song_playing_;
  AlbumCoverImageResult album_cover_;
  bool exit_;
  int exit_count_;
  bool delete_files_;

};

#endif  // MAINWINDOW_H
