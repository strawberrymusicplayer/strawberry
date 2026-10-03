/*
 * Strawberry Music Player
 * This code was part of Clementine
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QAbstractItemView>
#include <QSet>
#include <QString>
#include <QPixmap>

#include "core/song.h"

#include "widgets/autoexpandingtreeview.h"

class QWidget;
class QMenu;
class QAction;
class QMimeData;
class QSortFilterProxyModel;
class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;

class CollectionModel;
class CollectionFilterWidget;

class StreamingCollectionView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit StreamingCollectionView(QWidget *parent = nullptr);

  void Init(CollectionModel *collection_model, const bool favorite = false);

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void SetFilter(CollectionFilterWidget *filter);

  // QTreeView
  void keyboardSearch(const QString &search) override;
  void scrollTo(const QModelIndex &idx, ScrollHint hint = EnsureVisible) override;

 public Q_SLOTS:
  void TotalSongCountUpdated(const int count);
  void ReloadSettings();

  void FilterReturnPressed();

  void SaveFocus();
  void RestoreFocus();

 Q_SIGNALS:
  void GetSongs();
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
  QSortFilterProxyModel *filter_model() const;
  QMimeData *SelectedMimeData() const;
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex());
  void SaveContainerPath(const QModelIndex &child);

 private:
  CollectionModel *collection_model_;
  CollectionFilterWidget *filter_;
  bool favorite_;

  int total_song_count_;

  QPixmap nomusic_;

  QMenu *context_menu_;
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
