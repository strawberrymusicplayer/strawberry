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

#ifndef SIMPLETREEMODEL_H
#define SIMPLETREEMODEL_H

#include "config.h"

#include <QObject>
#include <QAbstractItemModel>

template<typename T>
class SimpleTreeModel : public QAbstractItemModel {
 public:
  explicit SimpleTreeModel(T *root = nullptr, QObject *parent = nullptr);
  ~SimpleTreeModel() override {}

  // QAbstractItemModel
  int columnCount(const QModelIndex &parent) const override;
  QModelIndex index(const int row, const int column, const QModelIndex &parent = QModelIndex()) const override;
  QModelIndex parent(const QModelIndex &idx) const override;
  int rowCount(const QModelIndex &parent) const override;
  bool hasChildren(const QModelIndex &parent) const override;

  T *IndexToItem(const QModelIndex &idx) const;
  QModelIndex ItemToIndex(T *item) const;

  // Called by items
  void BeginInsert(T *parent, int start, int end = -1);
  void EndInsert();
  void BeginDelete(T *parent, int start, int end = -1);
  void EndDelete();
  void EmitDataChanged(T *item);

 protected:
  T *root_;
};

template<typename T>
SimpleTreeModel<T>::SimpleTreeModel(T *root, QObject *parent)
    : QAbstractItemModel(parent), root_(root) {}

template<typename T>
T *SimpleTreeModel<T>::IndexToItem(const QModelIndex &idx) const {
  if (!idx.isValid()) return root_;
  return reinterpret_cast<T*>(idx.internalPointer());
}

template<typename T>
QModelIndex SimpleTreeModel<T>::ItemToIndex(T *item) const {
  if (!item || !item->parent) return QModelIndex();
  return createIndex(item->row, 0, item);
}

template <typename T>
int SimpleTreeModel<T>::columnCount(const QModelIndex&) const {
  return 1;
}

template<typename T>
QModelIndex SimpleTreeModel<T>::index(const int row, const int column, const QModelIndex &parent) const {

  Q_UNUSED(column);

  T *parent_item = IndexToItem(parent);
  if (!parent_item || row < 0 || parent_item->children.count() <= row)
    return QModelIndex();

  return ItemToIndex(parent_item->children[row]);

}

template<typename T>
QModelIndex SimpleTreeModel<T>::parent(const QModelIndex &idx) const {
  return ItemToIndex(IndexToItem(idx)->parent);
}

template<typename T>
int SimpleTreeModel<T>::rowCount(const QModelIndex &parent) const {
  T *item = IndexToItem(parent);
  if (!item) return 0;
  return item->children.count();
}

template<typename T>
bool SimpleTreeModel<T>::hasChildren(const QModelIndex &parent) const {
  T *item = IndexToItem(parent);
  if (!item) return 0;
  return !item->children.isEmpty();
}

template<typename T>
void SimpleTreeModel<T>::BeginInsert(T *parent, int start, int end) {
  if (end == -1) end = start;
  beginInsertRows(ItemToIndex(parent), start, end);
}

template<typename T>
void SimpleTreeModel<T>::EndInsert() {
  endInsertRows();
}

template<typename T>
void SimpleTreeModel<T>::BeginDelete(T *parent, int start, int end) {
  if (end == -1) end = start;
  beginRemoveRows(ItemToIndex(parent), start, end);
}

template<typename T>
void SimpleTreeModel<T>::EndDelete() {
  endRemoveRows();
}

template<typename T>
void SimpleTreeModel<T>::EmitDataChanged(T *item) {
  QModelIndex index(ItemToIndex(item));
  Q_EMIT dataChanged(index, index);
}

#endif  // SIMPLETREEMODEL_H
