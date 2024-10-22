/*
 * Strawberry Music Player
 * This code was part of Clementine
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

#ifndef STREAMINGCOLLECTIONVIEW_H
#define STREAMINGCOLLECTIONVIEW_H

#include "config.h"

#include <QObject>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QSet>
#include <QMap>
#include <QString>
#include <QPixmap>

#include "includes/shared_ptr.h"
#include "core/song.h"

#include "widgets/autoexpandingtreeview.h"

class QWidget;
class QMenu;
class QAction;
class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;

class CollectionBackend;
class CollectionModel;
class CollectionFilterWidget;

class StreamingCollectionView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit StreamingCollectionView(QWidget *parent = nullptr);

  void Init(const SharedPtr<CollectionBackend> collection_backend, CollectionModel *collection_model, const bool favorite = false);

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void SetFilter(CollectionFilterWidget *filter);

  // QTreeView
  void keyboardSearch(const QString &search) override;
  void scrollTo(const QModelIndex &idx, ScrollHint hint = EnsureVisible) override;

  int TotalSongs() const;
  int TotalArtists() const;
  int TotalAlbums() const;

 public Q_SLOTS:
  void TotalSongCountUpdated(int count);
  void TotalArtistCountUpdated(int count);
  void TotalAlbumCountUpdated(int count);
  void ReloadSettings();

  void FilterReturnPressed();

  void SaveFocus();
  void RestoreFocus();

 Q_SIGNALS:
  void GetSongs();
  void TotalSongCountUpdated_();
  void TotalArtistCountUpdated_();
  void TotalAlbumCountUpdated_();
  void Error(const QString &error);
  void RemoveSongs(const SongList &songs);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 private Q_SLOTS:
  void Load();
  void AddToPlaylist();
  void AddToPlaylistEnqueue();
  void AddToPlaylistEnqueueNext();
  void OpenInNewPlaylist();
  void RemoveSelectedSongs();

 private:
  void RecheckIsEmpty();
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex());
  void SaveContainerPath(const QModelIndex &child);

 private:
  SharedPtr<CollectionBackend> collection_backend_;
  CollectionModel *collection_model_;
  CollectionFilterWidget *filter_;
  bool favorite_;

  int total_song_count_;
  int total_artist_count_;
  int total_album_count_;

  QPixmap nomusic_;

  QMenu *context_menu_;
  QModelIndex context_menu_index_;
  QAction *load_;
  QAction *add_to_playlist_;
  QAction *add_to_playlist_enqueue_;
  QAction *add_to_playlist_enqueue_next_;
  QAction *open_in_new_playlist_;
  QAction *remove_songs_;

  bool is_in_keyboard_search_;

  // Save focus
  Song last_selected_song_;
  QString last_selected_container_;
  QSet<QString> last_selected_path_;
};

#endif  // STREAMINGCOLLECTIONVIEW_H
