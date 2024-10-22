/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/filesystemmusicstorage.h"
#include "core/iconloader.h"
#include "core/musicstorage.h"
#include "utilities/diskutils.h"
#include "collectiondirectory.h"
#include "collectionbackend.h"
#include "collectiondirectorymodel.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

CollectionDirectoryModel::CollectionDirectoryModel(SharedPtr<CollectionBackend> backend, QObject *parent)
    : QStandardItemModel(parent),
      dir_icon_(IconLoader::Load(u"document-open-folder"_s)),
      backend_(backend) {

  QObject::connect(&*backend_, &CollectionBackend::DirectoryAdded, this, &CollectionDirectoryModel::AddDirectory);
  QObject::connect(&*backend_, &CollectionBackend::DirectoryDeleted, this, &CollectionDirectoryModel::RemoveDirectory);

}

void CollectionDirectoryModel::AddDirectory(const CollectionDirectory &dir) {

  directories_.insert(dir.id, dir);
  paths_.append(dir.path);

  QStandardItem *item = new QStandardItem(dir.path);
  item->setData(dir.id, kIdRole);
  item->setIcon(dir_icon_);
  storage_ << make_shared<FilesystemMusicStorage>(backend_->source(), dir.path, dir.id);
  appendRow(item);

}

void CollectionDirectoryModel::RemoveDirectory(const CollectionDirectory &dir) {

  directories_.remove(dir.id);
  paths_.removeAll(dir.path);

  for (int i = 0; i < rowCount(); ++i) {
    if (item(i, 0)->data(kIdRole).toInt() == dir.id) {
      removeRow(i);
      storage_.removeAt(i);
      break;
    }
  }

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
