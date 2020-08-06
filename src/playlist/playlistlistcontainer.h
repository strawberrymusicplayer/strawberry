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

#ifndef PLAYLISTLISTCONTAINER_H
#define PLAYLISTLISTCONTAINER_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>

class QStandardItem;
class QSortFilterProxyModel;
class QMenu;
class QAction;
class QContextMenuEvent;
class QShowEvent;

class QModelIndex;
class Application;
class Playlist;
class PlaylistListModel;
class Ui_PlaylistListContainer;
class OrganizeDialog;

class PlaylistListContainer : public QWidget {
  Q_OBJECT

 public:
  explicit PlaylistListContainer(QWidget *parent = nullptr);
  ~PlaylistListContainer() override;

  void SetApplication(Application *app);
  void ReloadSettings();

 protected:
  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 private slots:
  // From the UI
  void NewFolderClicked();
  void DeleteClicked();
  void ItemDoubleClicked(const QModelIndex &index);
  void CopyToDevice();

  // From the model
  void PlaylistPathChanged(int id, const QString &new_path);

  // From the PlaylistManager
  void PlaylistRenamed(int id, const QString &new_name);
  // Add playlist if favorite == true
  void AddPlaylist(int id, const QString &name, bool favorite);
  void RemovePlaylist(int id);
  void SavePlaylist();
  void PlaylistFavoriteStateChanged(int id, bool favorite);
  void CurrentChanged(Playlist *new_playlist);
  void ActiveChanged(Playlist *new_playlist);

  // From the Player
  void ActivePlaying();
  void ActivePaused();
  void ActiveStopped();

  void ItemsSelectedChanged(const bool selected);

 private:
  QStandardItem *ItemForPlaylist(const QString &name, int id);
  QStandardItem *ItemForFolder(const QString &name) const;
  void RecursivelySetIcons(QStandardItem *parent) const;

  void RecursivelyFindPlaylists(const QModelIndex &parent, QSet<int> *ids) const;

  void UpdateActiveIcon(int id, const QIcon &icon);

  Application *app_;
  Ui_PlaylistListContainer *ui_;
  QMenu *menu_;

  QAction *action_new_folder_;
  QAction *action_remove_;
  QAction *action_save_playlist_;
  QAction *action_copy_to_device_;

  PlaylistListModel *model_;
  QSortFilterProxyModel *proxy_;

  bool loaded_icons_;
  QIcon padded_play_icon_;

  int active_playlist_id_;

  std::unique_ptr<OrganizeDialog> organize_dialog_;
};

#endif  // PLAYLISTLISTCONTAINER_H
