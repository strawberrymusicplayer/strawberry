/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MERGEDPROXYMODEL_H
#define MERGEDPROXYMODEL_H

#include "config.h"

#include <cstddef>

#include <QObject>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
#include <QMap>
#include <QHash>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "includes/scoped_ptr.h"

class QMimeData;

std::size_t hash_value(const QModelIndex &idx);

class MergedProxyModelPrivate;

class MergedProxyModel : public QAbstractProxyModel {
  Q_OBJECT

 public:
  explicit MergedProxyModel(QObject *parent = nullptr);
  ~MergedProxyModel() override;

  // Make another model appear as a child of the given item in the source model.
  void AddSubModel(const QModelIndex &source_parent, QAbstractItemModel *submodel);
  void RemoveSubModel(const QModelIndex &source_parent);

  // Find the item in the source model that is the parent of the model containing proxy_index.
  // If proxy_index is in the source model, then this just returns mapToSource(proxy_index).
  QModelIndex FindSourceParent(const QModelIndex &proxy_index) const;

  // QAbstractItemModel
  QModelIndex index(const int row, const int column, const QModelIndex &parent) const override;
  QModelIndex parent(const QModelIndex &child) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &proxy_index, const int role = Qt::DisplayRole) const override;
  bool hasChildren(const QModelIndex &parent) const override;
  QMap<int, QVariant> itemData(const QModelIndex &proxy_index) const override;
  Qt::ItemFlags flags(const QModelIndex &idx) const override;
  bool setData(const QModelIndex &idx, const QVariant &value, const int role) override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, const int row, const int column, const QModelIndex &parent) override;
  bool canFetchMore(const QModelIndex &parent) const override;
  void fetchMore(const QModelIndex &parent) override;

  // QAbstractProxyModel
  // Note that these implementations of map{To,From}Source will not always give you an index in sourceModel(),
  // you might get an index in one of the child models instead.
  QModelIndex mapFromSource(const QModelIndex &source_index) const override;
  QModelIndex mapToSource(const QModelIndex &proxy_index) const override;
  void setSourceModel(QAbstractItemModel *source_model) override;

  // Convenience functions that call map{To,From}Source multiple times.
  QModelIndexList mapFromSource(const QModelIndexList &source_indexes) const;
  QModelIndexList mapToSource(const QModelIndexList &proxy_indexes) const;

 Q_SIGNALS:
  void SubModelReset(const QModelIndex root, QAbstractItemModel *model);

 private Q_SLOTS:
  void SourceModelReset();
  void SubModelAboutToBeReset();
  void SubModelResetSlot();

  void RowsAboutToBeInserted(const QModelIndex &source_parent, const int start, const int end);
  void RowsInserted(const QModelIndex &source_parent, const int start, const int end);
  void RowsAboutToBeRemoved(const QModelIndex &source_parent, const int start, const int end);
  void RowsRemoved(const QModelIndex &source_parent, const int start, const int end);
  void DataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right);

  void LayoutAboutToBeChanged();
  void LayoutChanged();

 private:
  QModelIndex GetActualSourceParent(const QModelIndex &source_parent, QAbstractItemModel *model) const;
  QAbstractItemModel *GetModel(const QModelIndex &source_index) const;
  void DeleteAllMappings();
  bool IsKnownModel(const QAbstractItemModel *model) const;

  QHash<QAbstractItemModel*, QPersistentModelIndex> merge_points_;
  QAbstractItemModel *resetting_model_;

  QHash<QAbstractItemModel*, QModelIndex> old_merge_points_;

  ScopedPtr<MergedProxyModelPrivate> p_;
};

#endif  // MERGEDPROXYMODEL_H
