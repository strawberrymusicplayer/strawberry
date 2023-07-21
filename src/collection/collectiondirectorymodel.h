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

#ifndef COLLECTIONDIRECTORYMODEL_H
#define COLLECTIONDIRECTORYMODEL_H

#include "config.h"

#include <QObject>
#include <QStandardItemModel>
#include <QList>
#include <QVariant>
#include <QString>
#include <QIcon>

#include "core/shared_ptr.h"

class QModelIndex;

struct CollectionDirectory;
class CollectionBackend;
class MusicStorage;

class CollectionDirectoryModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit CollectionDirectoryModel(SharedPtr<CollectionBackend> collection_backend, QObject *parent = nullptr);

  // To be called by GUIs
  void AddDirectory(const QString &path);
  void RemoveDirectory(const QModelIndex &idx);

  QVariant data(const QModelIndex &idx, int role) const override;

 private slots:
  // To be called by the backend
  void DirectoryDiscovered(const CollectionDirectory &directories);
  void DirectoryDeleted(const CollectionDirectory &directories);

 private:
  static const int kIdRole = Qt::UserRole + 1;

  QIcon dir_icon_;
  SharedPtr<CollectionBackend> backend_;
  QList<SharedPtr<MusicStorage>> storage_;
};

#endif  // COLLECTIONDIRECTORYMODEL_H
