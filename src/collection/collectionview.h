/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONVIEW_H
#define COLLECTIONVIEW_H

#include "config.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QString>
#include <QPixmap>
#include <QSet>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "widgets/autoexpandingtreeview.h"

class QSortFilterProxyModel;
class QMenu;
class QAction;
class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;
class QKeyEvent;

class TaskManager;
class TagReaderClient;
class NetworkAccessManager;
class CollectionLibrary;
class CollectionBackend;
class CollectionModel;
class CollectionFilter;
class CollectionFilterWidget;
class DeviceManager;
class StreamingServices;
class AlbumCoverLoader;
class CurrentAlbumCoverLoader;
class CoverProviders;
class LyricsProviders;
class EditTagDialog;
class OrganizeDialog;

class CollectionView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit CollectionView(QWidget *parent = nullptr);
  ~CollectionView() override;

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void Init(const SharedPtr<TaskManager> task_manager,
            const SharedPtr<TagReaderClient> tagreader_client,
            const SharedPtr<NetworkAccessManager> network,
            const SharedPtr<AlbumCoverLoader> albumcover_loader,
            const SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader,
            const SharedPtr<CoverProviders> cover_providers,
            const SharedPtr<LyricsProviders> lyrics_providers,
            const SharedPtr<CollectionLibrary> collection,
            const SharedPtr<DeviceManager> device_manager,
            const SharedPtr<StreamingServices> streaming_services);

  void SetFilterWidget(CollectionFilterWidget *filter_widget);

  // QTreeView
  void keyboardSearch(const QString &search) override;
  void scrollTo(const QModelIndex &idx, ScrollHint hint = EnsureVisible) override;

  int TotalSongs() const;
  int TotalArtists() const;
  int TotalAlbums() const;

 public Q_SLOTS:
  void TotalSongCountUpdated(const int count);
  void TotalArtistCountUpdated(const int count);
  void TotalAlbumCountUpdated(const int count);
  void ReloadSettings();

  void FilterReturnPressed();

  void SaveFocus();
  void RestoreFocus();

  void EditTagError(const QString &message);

 Q_SIGNALS:
  void ShowSettingsDialog();

  void TotalSongCountUpdated_();
  void TotalArtistCountUpdated_();
  void TotalAlbumCountUpdated_();
  void Error(const QString &error);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event) override;
  void keyPressEvent(QKeyEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 private Q_SLOTS:
  void Load();
  void AddToPlaylist();
  void AddToPlaylistEnqueue();
  void AddToPlaylistEnqueueNext();
  void OpenInNewPlaylist();
  void SearchForThis();
  void Organize();
  void CopyToDevice();
  void EditTracks();
  void RescanSongs();
  void ShowInBrowser() const;
  void ShowInVarious();
  void NoShowInVarious();
  void Delete();
  void DeleteFilesFinished(const SongList &songs_with_errors);

 private:
  void SetShowInVarious(const bool on);
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex());
  void SaveContainerPath(const QModelIndex &child);

 private:
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<NetworkAccessManager> network_;
  SharedPtr<DeviceManager> device_manager_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  SharedPtr<CurrentAlbumCoverLoader> current_albumcover_loader_;
  SharedPtr<CollectionLibrary> collection_;
  SharedPtr<CoverProviders> cover_providers_;
  SharedPtr<LyricsProviders> lyrics_providers_;
  SharedPtr<StreamingServices> streaming_services_;

  SharedPtr<CollectionBackend> backend_;
  CollectionModel *model_;
  CollectionFilter *filter_;
  CollectionFilterWidget *filter_widget_;

  int total_song_count_;
  int total_artist_count_;
  int total_album_count_;

  QPixmap nomusic_;

  QMenu *context_menu_;
  QModelIndex context_menu_index_;
  QAction *action_load_;
  QAction *action_add_to_playlist_;
  QAction *action_add_to_playlist_enqueue_;
  QAction *action_add_to_playlist_enqueue_next_;
  QAction *action_open_in_new_playlist_;
  QAction *action_organize_;
  QAction *action_search_for_this_;

  QAction *action_copy_to_device_;
  QAction *action_edit_track_;
  QAction *action_edit_tracks_;
  QAction *action_rescan_songs_;
  QAction *action_show_in_browser_;
  QAction *action_show_in_various_;
  QAction *action_no_show_in_various_;
  QAction *action_delete_files_;

  ScopedPtr<OrganizeDialog> organize_dialog_;
  ScopedPtr<EditTagDialog> edit_tag_dialog_;

  bool is_in_keyboard_search_;
  bool delete_files_;

  // Save focus
  Song last_selected_song_;
  QString last_selected_container_;
  QSet<QString> last_selected_path_;
};

#endif  // COLLECTIONVIEW_H
