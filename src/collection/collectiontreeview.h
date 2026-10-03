/*
 * Strawberry Music Player
 * This file was part of Clementine.
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

#ifndef COLLECTIONTREEVIEW_H
#define COLLECTIONTREEVIEW_H

#include "config.h"

#include <QObject>
#include <QAbstractItemView>
#include <QPersistentModelIndex>
#include <QList>
#include <QString>
#include <QPixmap>

#include "core/song.h"
#include "widgets/autoexpandingtreeview.h"

class QWidget;
class QTimer;
class QMenu;
class QAction;
class QMimeData;
class QSortFilterProxyModel;
class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

class CollectionModel;
class CollectionFilterWidget;

// Tree view for a CollectionModel, shared by the collection view and the streaming collection views.
// It handles the playlist actions, the empty collection text, keyboard search and keeping the focus when the model is reset.
class CollectionTreeView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  // A total song count of -1 means the count is not known yet, so the collection is not shown as empty until it is received.
  explicit CollectionTreeView(QWidget *parent = nullptr, const int total_song_count = 0);

  // Returns Songs currently selected in the collection view.
  // Please note that the selection is recursive meaning that if for example an album is selected this will return all of it's songs.
  SongList GetSelectedSongs() const;

  void SetFilterWidget(CollectionFilterWidget *filter_widget);

  int TotalSongs() const { return total_song_count_; }

  // QTreeView
  void keyboardSearch(const QString &search) override;
  void scrollTo(const QModelIndex &idx, ScrollHint hint = EnsureVisible) override;

 public Q_SLOTS:
  void TotalSongCountUpdated(const int count);
  void SaveFocus();
  void RestoreFocus();
  void FilterReturnPressed();

 Q_SIGNALS:
  void TotalSongCountUpdated_();

 protected:
  void SetCollectionModel(CollectionModel *collection_model);
  CollectionModel *collection_model() const { return collection_model_; }
  CollectionFilterWidget *filter_widget() const { return filter_widget_; }
  QSortFilterProxyModel *filter_model() const;

  // The source model index of the item the context menu was opened on.
  const QPersistentModelIndex &context_menu_index() const { return context_menu_index_; }

  // The texts shown when the collection is empty, and what to do when it is clicked.
  virtual QString EmptyTitleText() const = 0;
  virtual QString EmptyText() const = 0;
  virtual void EmptyClicked() = 0;

  // Called once when the context menu is created, to add actions after the playlist actions.
  virtual void AddContextMenuActions(QMenu *menu) { Q_UNUSED(menu) }
  // Called before the context menu is shown, to enable or disable the added actions.
  virtual void UpdateContextMenuActions(const bool has_selection) { Q_UNUSED(has_selection) }

  // QAbstractItemView
  void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

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
  void ScheduleRestoreFocus();
  void TryRestoreFocus();

 private:
  QMimeData *SelectedMimeData() const;
  // Whether the collection model only shows the loading indicator, between the two resets of reloading it.
  bool IsLoading() const;
  // Whether the saved containers are on the same levels with the current grouping.
  bool SavedPathMatchesGrouping() const;
  bool RestoreSongFocus();
  bool RestoreLevelFocus(const QModelIndex &parent = QModelIndex(), const qsizetype level = 0);

 private:
  // A container saved to find it again after the model is reset.
  struct SavedContainer {
    SavedContainer() : container_type(0) {}
    SavedContainer(const QString &_sort_text, const int _container_type) : sort_text(_sort_text), container_type(_container_type) {}
    QString sort_text;
    // The CollectionModel::GroupBy of the container, None for a divider.
    int container_type;
  };

  CollectionModel *collection_model_;
  CollectionFilterWidget *filter_widget_;

  int total_song_count_;

  QPixmap nomusic_;

  QMenu *context_menu_;
  QPersistentModelIndex context_menu_index_;
  QAction *action_load_;
  QAction *action_add_to_playlist_;
  QAction *action_add_to_playlist_enqueue_;
  QAction *action_add_to_playlist_enqueue_next_;
  QAction *action_open_in_new_playlist_;
  QAction *action_search_for_this_;

  bool is_in_keyboard_search_;

  // The current item saved before the model is reset, to select it again after.
  Song last_selected_song_;
  SavedContainer last_selected_container_;
  bool last_selected_container_expanded_;
  // The containers above the current item, from the top level down.
  QList<SavedContainer> last_selected_path_;
  // After a reset, the songs are added to the model later, so restoring is retried as they are added, until it succeeds or the user selects another item.
  bool restore_focus_pending_;
  bool restoring_focus_;
  QTimer *timer_restore_focus_;
};

#endif  // COLLECTIONTREEVIEW_H
