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

#include "config.h"

#include <QObject>
#include <QFlags>
#include <QMimeData>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QStandardItemModel>
#include <QAbstractItemModel>

#include "playlistlistmodel.h"

PlaylistListModel::PlaylistListModel(QObject *parent) : QStandardItemModel(parent), dropping_rows_(false) {

  QObject::connect(this, &PlaylistListModel::dataChanged, this, &PlaylistListModel::RowsChanged);
  QObject::connect(this, &PlaylistListModel::rowsAboutToBeRemoved, this, &PlaylistListModel::RowsAboutToBeRemoved);
  QObject::connect(this, &PlaylistListModel::rowsInserted, this, &PlaylistListModel::RowsInserted);

}

void PlaylistListModel::SetIcons(const QIcon &playlist_icon, const QIcon &folder_icon) {
  playlist_icon_ = playlist_icon;
  folder_icon_ = folder_icon;
}

bool PlaylistListModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) {

  dropping_rows_ = true;
  bool ret = QStandardItemModel::dropMimeData(data, action, row, column, parent);
  dropping_rows_ = false;

  return ret;

}

QString PlaylistListModel::ItemPath(const QStandardItem *item) {

  QStringList components;

  const QStandardItem *current = item;
  while (current) {
    if (current->data(Role_Type).toInt() == Type_Folder) {
      components.insert(0, current->data(Qt::DisplayRole).toString());
    }
    current = current->parent();
  }

  return components.join(u'/');

}

void PlaylistListModel::RowsChanged(const QModelIndex &begin, const QModelIndex &end) {
  AddRowMappings(begin, end);
}

void PlaylistListModel::RowsInserted(const QModelIndex &parent, const int start, const int end) {

  // RowsChanged will take care of these when dropping.
  if (!dropping_rows_) {
    AddRowMappings(index(start, 0, parent), index(end, 0, parent));
  }

}

void PlaylistListModel::AddRowMappings(const QModelIndex &begin, const QModelIndex &end) {

  const QString parent_path = ItemPath(itemFromIndex(begin));

  for (int i = begin.row(); i <= end.row(); ++i) {
    const QModelIndex index = begin.sibling(i, 0);
    QStandardItem *item = itemFromIndex(index);
    AddRowItem(item, parent_path);
  }

}

void PlaylistListModel::AddRowItem(QStandardItem *item, const QString &parent_path) {

  switch (item->data(Role_Type).toInt()) {
    case Type_Playlist:{
      const int id = item->data(Role_PlaylistId).toInt();

      playlists_by_id_[id] = item;
      if (dropping_rows_) {
        Q_EMIT PlaylistPathChanged(id, parent_path);
      }

      break;
    }

    case Type_Folder:
      for (int j = 0; j < item->rowCount(); ++j) {
        QStandardItem *child_item = item->child(j);
        AddRowItem(child_item, parent_path);
      }
      break;

    default:
      break;
  }

}

void PlaylistListModel::RowsAboutToBeRemoved(const QModelIndex &parent, const int start, const int end) {

  for (int i = start; i <= end; ++i) {
    const QModelIndex idx = index(i, 0, parent);
    const QStandardItem *item = itemFromIndex(idx);

    switch (idx.data(Role_Type).toInt()) {
      case Type_Playlist:{
        const int id = idx.data(Role_PlaylistId).toInt();
        QMap<int, QStandardItem*>::const_iterator it = playlists_by_id_.constFind(id);
        if (it != playlists_by_id_.constEnd() && it.value() == item) {
          playlists_by_id_.erase(it);
        }
        break;
      }

      case Type_Folder:
        break;

      default:
        break;
    }
  }

}

QStandardItem *PlaylistListModel::PlaylistById(const int id) const {
  return playlists_by_id_[id];
}

QStandardItem *PlaylistListModel::FolderByPath(const QString &path) {

  if (path.isEmpty()) {
    return invisibleRootItem();
  }

  // Walk down from the root until we find the target folder.  This is pretty
  // inefficient but maintaining a path -> item map is difficult.
  QStandardItem *parent = invisibleRootItem();

  const QStringList parts = path.split(u'/', Qt::SkipEmptyParts);
  for (const QString &part : parts) {
    QStandardItem *matching_child = nullptr;

    const int child_count = parent->rowCount();
    for (int i = 0; i < child_count; ++i) {
      if (parent->child(i)->data(Qt::DisplayRole).toString() == part) {
        matching_child = parent->child(i);
        break;
      }
    }

    // Does this folder exist already?
    if (matching_child) {
      parent = matching_child;
    }
    else {
      QStandardItem *child = NewFolder(part);
      parent->appendRow(child);
      parent = child;
    }
  }

  return parent;

}

QStandardItem *PlaylistListModel::NewFolder(const QString &name) const {

  QStandardItem *ret = new QStandardItem;
  ret->setText(name);
  ret->setData(PlaylistListModel::Type_Folder, PlaylistListModel::Role_Type);
  ret->setIcon(folder_icon_);
  ret->setFlags(Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
  return ret;

}

QStandardItem *PlaylistListModel::NewPlaylist(const QString &name, const int id) const {

  QStandardItem *ret = new QStandardItem;
  ret->setText(name);
  ret->setData(PlaylistListModel::Type_Playlist, PlaylistListModel::Role_Type);
  ret->setData(id, PlaylistListModel::Role_PlaylistId);
  ret->setIcon(playlist_icon_);
  ret->setFlags(Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
  return ret;

}

bool PlaylistListModel::setData(const QModelIndex &idx, const QVariant &value, int role) {

  if (!QStandardItemModel::setData(idx, value, role)) {
    return false;
  }

  switch (idx.data(Role_Type).toInt()) {
    case Type_Playlist:
      Q_EMIT PlaylistRenamed(idx.data(Role_PlaylistId).toInt(), value.toString());
      break;

    case Type_Folder:
      // Walk all the children and modify their paths.
      UpdatePathsRecursive(idx);
      break;

    default:
      break;
  }

  return true;

}

void PlaylistListModel::UpdatePathsRecursive(const QModelIndex &parent) {

  switch (parent.data(Role_Type).toInt()) {
    case Type_Playlist:
      Q_EMIT PlaylistPathChanged(parent.data(Role_PlaylistId).toInt(), ItemPath(itemFromIndex(parent)));
      break;

    case Type_Folder:
      for (int i = 0; i < rowCount(parent); ++i) {
        UpdatePathsRecursive(index(i, 0, parent));
      }
      break;
  }

}
