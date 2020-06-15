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

#include "config.h"

#include <QObject>
#include <QStandardItemModel>
#include <QAbstractItemModel>
#include <QVariant>
#include <QString>

#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "core/utilities.h"
#include "directory.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"

CollectionDirectoryModel::CollectionDirectoryModel(CollectionBackend *backend, QObject *parent)
    : QStandardItemModel(parent),
      dir_icon_(IconLoader::Load("document-open-folder")),
    backend_(backend)
{

  connect(backend_, SIGNAL(DirectoryDiscovered(Directory, SubdirectoryList)), SLOT(DirectoryDiscovered(Directory)));
  connect(backend_, SIGNAL(DirectoryDeleted(Directory)), SLOT(DirectoryDeleted(Directory)));

}

CollectionDirectoryModel::~CollectionDirectoryModel() = default;

void CollectionDirectoryModel::DirectoryDiscovered(const Directory &dir) {

  QStandardItem *item = new QStandardItem(dir.path);
  item->setData(dir.id, kIdRole);
  item->setIcon(dir_icon_);
  storage_ << std::shared_ptr<MusicStorage>(new FilesystemMusicStorage(dir.path));
  appendRow(item);

}

void CollectionDirectoryModel::DirectoryDeleted(const Directory &dir) {

  for (int i = 0; i < rowCount(); ++i) {
    if (item(i, 0)->data(kIdRole).toInt() == dir.id) {
      removeRow(i);
      storage_.removeAt(i);
      break;
    }
  }

}

void CollectionDirectoryModel::AddDirectory(const QString &path) {

  if (!backend_) return;

  backend_->AddDirectory(path);

}

void CollectionDirectoryModel::RemoveDirectory(const QModelIndex &index) {

  if (!backend_ || !index.isValid()) return;

  Directory dir;
  dir.path = index.data().toString();
  dir.id = index.data(kIdRole).toInt();

  backend_->RemoveDirectory(dir);

}

QVariant CollectionDirectoryModel::data(const QModelIndex &index, int role) const {

  switch (role) {
    case MusicStorage::Role_Storage:
    case MusicStorage::Role_StorageForceConnect:
      return QVariant::fromValue(storage_[index.row()]);

    case MusicStorage::Role_FreeSpace:
    return Utilities::FileSystemFreeSpace(data(index, Qt::DisplayRole).toString());

    case MusicStorage::Role_Capacity:
    return Utilities::FileSystemCapacity(data(index, Qt::DisplayRole).toString());

    default:
      return QStandardItemModel::data(index, role);
  }

}

