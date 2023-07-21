/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SMARTPLAYLISTSVIEWCONTAINER_H
#define SMARTPLAYLISTSVIEWCONTAINER_H

#include "config.h"

#include <QWidget>
#include <QModelIndex>

class QMimeData;
class QMenu;
class QAction;
class QShowEvent;

class Application;
class SmartPlaylistsModel;
class SmartPlaylistsView;
class Ui_SmartPlaylistsViewContainer;

class SmartPlaylistsViewContainer : public QWidget {
  Q_OBJECT

 public:
  explicit SmartPlaylistsViewContainer(Application *app, QWidget *parent = nullptr);
  ~SmartPlaylistsViewContainer() override;

  SmartPlaylistsView *view() const;

  void ReloadSettings();

 protected:
  void showEvent(QShowEvent *e) override;

 private slots:
  void ItemsSelectedChanged();
  void ItemDoubleClicked(const QModelIndex &idx);

  void RightClicked(const QPoint global_pos, const QModelIndex &idx);

  void AppendToPlaylist();
  void ReplaceCurrentPlaylist();
  void OpenInNewPlaylist();

  void AddToPlaylistEnqueue();
  void AddToPlaylistEnqueueNext();

  void NewSmartPlaylist();

  void EditSmartPlaylist(const QModelIndex &idx);
  void DeleteSmartPlaylist(const QModelIndex &idx);

  void EditSmartPlaylistFromButton();
  void DeleteSmartPlaylistFromButton();
  void EditSmartPlaylistFromContext();
  void DeleteSmartPlaylistFromContext();

  void NewSmartPlaylistFinished();
  void EditSmartPlaylistFinished();

 signals:
  void AddToPlaylist(QMimeData *data);

 private:
  Ui_SmartPlaylistsViewContainer *ui_;
  Application *app_;
  SmartPlaylistsModel *model_;

  QMenu *context_menu_;
  QMenu *context_menu_selected_;
  QAction *action_new_smart_playlist_;
  QAction *action_edit_smart_playlist_;
  QAction *action_delete_smart_playlist_;
  QAction *action_append_to_playlist_;
  QAction *action_replace_current_playlist_;
  QAction *action_open_in_new_playlist_;
  QAction *action_add_to_playlist_enqueue_;
  QAction *action_add_to_playlist_enqueue_next_;
  QModelIndex context_menu_index_;
};

#endif  // SMARTPLAYLISTSVIEWCONTAINER_H
