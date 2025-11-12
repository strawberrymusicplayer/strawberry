/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef FILEVIEWTREEMODEL_H
#define FILEVIEWTREEMODEL_H

#include "config.h"

#include <QObject>
#include <QVariant>
#include <QStringList>
#include <QIcon>

#include "core/simpletreemodel.h"
#include "fileviewtreeitem.h"

class QFileIconProvider;
class QMimeData;

class FileViewTreeModel : public SimpleTreeModel<FileViewTreeItem> {
  Q_OBJECT

 public:
  explicit FileViewTreeModel(QObject *parent = nullptr);
  ~FileViewTreeModel() override;

  enum Role {
    Role_Type = Qt::UserRole + 1,
    Role_FilePath,
    Role_FileName,
    RoleCount
  };

  // QAbstractItemModel
  Qt::ItemFlags flags(const QModelIndex &idx) const override;
  QVariant data(const QModelIndex &idx, const int role) const override;
  bool hasChildren(const QModelIndex &parent) const override;
  bool canFetchMore(const QModelIndex &parent) const override;
  void fetchMore(const QModelIndex &parent) override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  void SetRootPaths(const QStringList &paths);
  void SetNameFilters(const QStringList &filters);

 private:
  void Reset();
  void LazyLoad(FileViewTreeItem *item);
  QIcon GetIcon(const FileViewTreeItem *item) const;

 private:
  QFileIconProvider *icon_provider_;
  QStringList name_filters_;
};

#endif  // FILEVIEWTREEMODEL_H
