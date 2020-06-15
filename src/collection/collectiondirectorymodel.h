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

#include <memory>

#include <QObject>
#include <QStandardItemModel>
#include <QList>
#include <QVariant>
#include <QString>
#include <QIcon>

class QModelIndex;

struct Directory;
class CollectionBackend;
class MusicStorage;

class CollectionDirectoryModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit CollectionDirectoryModel(CollectionBackend* backend, QObject *parent = nullptr);
  ~CollectionDirectoryModel() override;

  // To be called by GUIs
  void AddDirectory(const QString &path);
  void RemoveDirectory(const QModelIndex &index);

  QVariant data(const QModelIndex &index, int role) const override;

 private slots:
  // To be called by the backend
  void DirectoryDiscovered(const Directory &directories);
  void DirectoryDeleted(const Directory &directories);

 private:
  static const int kIdRole = Qt::UserRole + 1;

  QIcon dir_icon_;
  CollectionBackend* backend_;
  QList<std::shared_ptr<MusicStorage> > storage_;
};

#endif  // COLLECTIONDIRECTORYMODEL_H
