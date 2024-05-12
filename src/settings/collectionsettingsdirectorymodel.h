/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONSETTINGSDIRECTORYMODEL_H
#define COLLECTIONSETTINGSDIRECTORYMODEL_H

#include "config.h"

#include <QStandardItemModel>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QIcon>

class CollectionSettingsDirectoryModel : public QStandardItemModel {
  Q_OBJECT

 public:
  explicit CollectionSettingsDirectoryModel(QObject *parent = nullptr);

  void AddDirectory(const QString &path);
  void AddDirectories(const QStringList &paths);
  void RemoveDirectory(const QModelIndex &idx);

  QStringList paths() const { return paths_; }

 private:
  QIcon dir_icon_;
  QStringList paths_;
};

#endif  // COLLECTIONSETTINGSDIRECTORYMODEL_H
