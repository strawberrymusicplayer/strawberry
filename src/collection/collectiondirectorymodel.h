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

#ifndef COLLECTIONDIRECTORYMODEL_H
#define COLLECTIONDIRECTORYMODEL_H

#include "config.h"

#include <QObject>
#include <QStandardItemModel>
#include <QList>
#include <QMap>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QIcon>

#include "includes/shared_ptr.h"
#include "collectiondirectory.h"

class QModelIndex;

class CollectionBackend;
class MusicStorage;

class CollectionDirectoryModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit CollectionDirectoryModel(SharedPtr<CollectionBackend> collection_backend, QObject *parent = nullptr);

  QVariant data(const QModelIndex &idx, int role) const override;

  SharedPtr<CollectionBackend> backend() const { return backend_; }

  QMap<int, CollectionDirectory> directories() const { return directories_; }
  QStringList paths() const { return paths_; }

 private Q_SLOTS:
  void AddDirectory(const CollectionDirectory &directory);
  void RemoveDirectory(const CollectionDirectory &directory);

 private:
  static const int kIdRole = Qt::UserRole + 1;

  QIcon dir_icon_;
  SharedPtr<CollectionBackend> backend_;
  QMap<int, CollectionDirectory> directories_;
  QStringList paths_;
  QList<SharedPtr<MusicStorage>> storage_;
};

#endif  // COLLECTIONDIRECTORYMODEL_H
