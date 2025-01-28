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

#ifndef PLAYLISTLISTCONTAINER_H
#define PLAYLISTLISTCONTAINER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QModelIndex>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"

class QStandardItem;
class QSortFilterProxyModel;
class QMenu;
class QAction;
class QContextMenuEvent;
class QShowEvent;

class TaskManager;
class TagReaderClient;
class DeviceManager;
class PlaylistManager;
class PlaylistBackend;
class Playlist;
class PlaylistListModel;
class Ui_PlaylistListContainer;
class OrganizeDialog;

class PlaylistListContainer : public QWidget {
  Q_OBJECT

 public:
  explicit PlaylistListContainer(QWidget *parent = nullptr);
  ~PlaylistListContainer() override;

  void Init(const SharedPtr<TaskManager> task_manager,
            const SharedPtr<TagReaderClient> tagreader_client,
            const SharedPtr<PlaylistManager> playlist_manager,
            const SharedPtr<PlaylistBackend> playlist_backend,
            const SharedPtr<DeviceManager> device_manager);

  void ReloadSettings();

 protected:
  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 public Q_SLOTS:
  // From the Player
  void ActivePlaying();
  void ActivePaused();
  void ActiveStopped();

 private Q_SLOTS:
  // From the UI
  void NewFolderClicked();
  void ItemDoubleClicked(const QModelIndex &proxy_idx);
  void ItemMimeDataDropped(const QModelIndex &proxy_idx, const QMimeData *q_mimedata);

  // From the model
  void PlaylistPathChanged(const int id, const QString &new_path);

  // From the PlaylistManager
  void PlaylistRenamed(const int id, const QString &new_name);
  // Add playlist if favorite == true
  void AddPlaylist(const int id, const QString &name, const bool favorite);
  void RemovePlaylist(const int id);
  void PlaylistFavoriteStateChanged(const int id, const bool favorite);
  void CurrentChanged(Playlist *new_playlist);
  void ActiveChanged(Playlist *new_playlist);

  void ItemsSelectedChanged(const bool selected);

  void SavePlaylist();
  void Delete();
  void CopyToDevice();

 private:
  QStandardItem *ItemForPlaylist(const QString &name, int id);
  QStandardItem *ItemForFolder(const QString &name) const;
  void RecursivelySetIcons(QStandardItem *parent) const;

  void RecursivelyFindPlaylists(const QModelIndex &parent, QSet<int> *ids) const;

  void UpdateActiveIcon(int id, const QIcon &icon);

  SharedPtr<TaskManager> task_manager_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<PlaylistManager> playlist_manager_;
  SharedPtr<PlaylistBackend> playlist_backend_;
  SharedPtr<DeviceManager> device_manager_;

  Ui_PlaylistListContainer *ui_;
  QMenu *menu_;

  QAction *action_new_folder_;
  QAction *action_remove_;
  QAction *action_save_playlist_;
#ifndef Q_OS_WIN32
  QAction *action_copy_to_device_;
#endif

  PlaylistListModel *model_;
  QSortFilterProxyModel *proxy_;

  bool loaded_icons_;
  QIcon padded_play_icon_;

  int active_playlist_id_;

  ScopedPtr<OrganizeDialog> organize_dialog_;
};

#endif  // PLAYLISTLISTCONTAINER_H
