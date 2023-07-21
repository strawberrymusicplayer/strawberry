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

#include <memory>

#include <QObject>
#include <QStandardItemModel>
#include <QAbstractItemModel>
#include <QVariant>
#include <QString>

#include "core/shared_ptr.h"
#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "utilities/diskutils.h"
#include "collectiondirectory.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"

using std::make_shared;

CollectionDirectoryModel::CollectionDirectoryModel(SharedPtr<CollectionBackend> backend, QObject *parent)
    : QStandardItemModel(parent),
      dir_icon_(IconLoader::Load("document-open-folder")),
      backend_(backend) {

  QObject::connect(&*backend_, &CollectionBackend::DirectoryDiscovered, this, &CollectionDirectoryModel::DirectoryDiscovered);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, this, &CollectionDirectoryModel::DirectoryDeleted);

}

void CollectionDirectoryModel::DirectoryDiscovered(const CollectionDirectory &dir) {

  QStandardItem *item = new QStandardItem(dir.path);
  item->setData(dir.id, kIdRole);
  item->setIcon(dir_icon_);
  storage_ << make_shared<FilesystemMusicStorage>(backend_->source(), dir.path, dir.id);
  appendRow(item);

}

void CollectionDirectoryModel::DirectoryDeleted(const CollectionDirectory &dir) {

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

void CollectionDirectoryModel::RemoveDirectory(const QModelIndex &idx) {

  if (!backend_ || !idx.isValid()) return;

  CollectionDirectory dir;
  dir.path = idx.data().toString();
  dir.id = idx.data(kIdRole).toInt();

  backend_->RemoveDirectory(dir);

}

QVariant CollectionDirectoryModel::data(const QModelIndex &idx, int role) const {

  switch (role) {
    case MusicStorage::Role_Storage:
    case MusicStorage::Role_StorageForceConnect:
      return QVariant::fromValue(storage_[idx.row()]);
    case MusicStorage::Role_FreeSpace:
      return Utilities::FileSystemFreeSpace(data(idx, Qt::DisplayRole).toString());
    case MusicStorage::Role_Capacity:
      return Utilities::FileSystemCapacity(data(idx, Qt::DisplayRole).toString());
    default:
      return QStandardItemModel::data(idx, role);
  }

}
