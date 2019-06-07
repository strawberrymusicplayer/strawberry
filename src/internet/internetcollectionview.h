/*
 * Strawberry Music Player
 * This code was part of Clementine
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef INTERNETCOLLECTIONVIEW_H
#define INTERNETCOLLECTIONVIEW_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QWidget>
#include <QSet>
#include <QString>
#include <QPixmap>
#include <QAction>
#include <QMenu>
#include <QtEvents>

#include "widgets/autoexpandingtreeview.h"
#include "core/song.h"

class QContextMenuEvent;
class QHelpEvent;
class QMouseEvent;
class QPaintEvent;

class Application;
class CollectionBackend;
class CollectionModel;
class CollectionFilterWidget;

class InternetCollectionView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  InternetCollectionView(QWidget *parent = nullptr);
  ~InternetCollectionView();
  
  void Init(Application *app, CollectionBackend *backend, CollectionModel *model);

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void SetApplication(Application *app);
  void SetFilter(CollectionFilterWidget *filter);

  // QTreeView
  void keyboardSearch(const QString &search);
  void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible);

  int TotalSongs();
  int TotalArtists();
  int TotalAlbums();

 public slots:
  void TotalSongCountUpdated(int count);
  void TotalArtistCountUpdated(int count);
  void TotalAlbumCountUpdated(int count);
  void ReloadSettings();

  void FilterReturnPressed();

  void SaveFocus();
  void RestoreFocus();

 signals:
  void GetSongs();
  void TotalSongCountUpdated_();
  void TotalArtistCountUpdated_();
  void TotalAlbumCountUpdated_();
  void Error(const QString &message);
  void RemoveSongs(const SongList &songs);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event);
  void mouseReleaseEvent(QMouseEvent *e);
  void contextMenuEvent(QContextMenuEvent *e);

 private slots:
  void Load();
  void AddToPlaylist();
  void AddToPlaylistEnqueue();
  void AddToPlaylistEnqueueNext();
  void OpenInNewPlaylist();
  void RemoveSongs();

 private:
  void RecheckIsEmpty();
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex());
  void SaveContainerPath(const QModelIndex &child);

 private:
  Application *app_;
  CollectionBackend *collection_backend_;
  CollectionModel*collection_model_;
  CollectionFilterWidget *filter_;

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

#endif  // INTERNETCOLLECTIONVIEW_H
