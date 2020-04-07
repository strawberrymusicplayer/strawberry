/*
 * Strawberry Music Player
 * This file was part of Clementine.
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

#ifndef DEVICEVIEW_H
#define DEVICEVIEW_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QStyleOption>
#include <QStyleOptionViewItem>
#include <QAbstractItemModel>
#include <QString>

#include "core/song.h"
#include "collection/collectionitemdelegate.h"
#include "widgets/autoexpandingtreeview.h"

class QSortFilterProxyModel;
class QPainter;
class QWidget;
class QMenu;
class QAction;
class QMouseEvent;
class QContextMenuEvent;

class Application;
class DeviceProperties;
class MergedProxyModel;
class OrganiseDialog;

class DeviceItemDelegate : public CollectionItemDelegate {
  Q_OBJECT

 public:
  explicit DeviceItemDelegate(QObject *parent);

  static const int kIconPadding;

  void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

};

class DeviceView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit DeviceView(QWidget *parent = nullptr);
  ~DeviceView();

  void SetApplication(Application *app);

 protected:
  void contextMenuEvent(QContextMenuEvent*);
  void mouseDoubleClickEvent(QMouseEvent *event);

 private slots:
  // Device menu actions
  void Connect();
  void Unmount();
  void Forget();
  void Properties();

  // Collection menu actions
  void Load();
  void AddToPlaylist();
  void OpenInNewPlaylist();
  void Organise();
  void Delete();

  void DeviceConnected(QModelIndex idx);
  void DeviceDisconnected(QModelIndex idx);

  void DeleteFinished(const SongList &songs_with_errors);

  // AutoExpandingTreeView
  bool CanRecursivelyExpand(const QModelIndex &idx) const;

 private:
  QModelIndex MapToDevice(const QModelIndex &merged_model_index) const;
  QModelIndex MapToCollection(const QModelIndex &merged_model_index) const;
  QModelIndex FindParentDevice(const QModelIndex &merged_model_index) const;
  SongList GetSelectedSongs() const;

 private:
  Application *app_;
  MergedProxyModel *merged_model_;
  QSortFilterProxyModel *sort_model_;

  std::unique_ptr<DeviceProperties> properties_dialog_;
  std::unique_ptr<OrganiseDialog> organise_dialog_;

  QMenu *device_menu_;
  QAction *eject_action_;
  QAction *forget_action_;
  QAction *properties_action_;

  QMenu *collection_menu_;
  QAction *load_action_;
  QAction *add_to_playlist_action_;
  QAction *open_in_new_playlist_;
  QAction *organise_action_;
  QAction *delete_action_;

  QModelIndex menu_index_;
};

#endif  // DEVICEVIEW_H
