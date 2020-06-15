/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <memory>

#include <QObject>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QString>
#include <QPixmap>
#include <QSet>

#include "core/song.h"
#include "widgets/autoexpandingtreeview.h"

class QWidget;
class QMenu;
class QAction;
class QContextMenuEvent;
class QMouseEvent;
class QPaintEvent;

class Application;
class CollectionFilterWidget;
class EditTagDialog;
class OrganiseDialog;

class CollectionView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit CollectionView(QWidget *parent = nullptr);
  ~CollectionView() override;

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void SetApplication(Application *app);
  void SetFilter(CollectionFilterWidget *filter);

  // QTreeView
  void keyboardSearch(const QString &search) override;
  void scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) override;

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

  void EditTagError(const QString &message);

 signals:
  void ShowConfigDialog();

  void TotalSongCountUpdated_();
  void TotalArtistCountUpdated_();
  void TotalAlbumCountUpdated_();
  void Error(const QString &message);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 private slots:
  void Load();
  void AddToPlaylist();
  void AddToPlaylistEnqueue();
  void AddToPlaylistEnqueueNext();
  void OpenInNewPlaylist();
  void Organise();
  void CopyToDevice();
  void EditTracks();
  void RescanSongs();
  void ShowInBrowser();
  void ShowInVarious();
  void NoShowInVarious();

 private:
  void RecheckIsEmpty();
  void ShowInVarious(bool on);
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex());
  void SaveContainerPath(const QModelIndex &child);

 private:
  Application *app_;
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
  QAction *organise_;
#ifndef Q_OS_WIN
  QAction *copy_to_device_;
#endif
  QAction *delete_;
  QAction *edit_track_;
  QAction *edit_tracks_;
  QAction *rescan_songs_;
  QAction *show_in_browser_;
  QAction *show_in_various_;
  QAction *no_show_in_various_;

  std::unique_ptr<OrganiseDialog> organise_dialog_;
  std::unique_ptr<EditTagDialog> edit_tag_dialog_;

  bool is_in_keyboard_search_;

  // Save focus
  Song last_selected_song_;
  QString last_selected_container_;
  QSet<QString> last_selected_path_;
};

#endif  // COLLECTIONVIEW_H
