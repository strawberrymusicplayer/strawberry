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

#include "config.h"

#include <functional>

#include <QObject>
#include <QMimeData>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QtAlgorithms>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>

#include "mergedproxymodel.h"

#include <boost/multi_index/detail/bidir_node_iterator.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/operators.hpp>

using boost::multi_index::hashed_unique;
using boost::multi_index::identity;
using boost::multi_index::indexed_by;
using boost::multi_index::member;
using boost::multi_index::multi_index_container;
using boost::multi_index::ordered_unique;
using boost::multi_index::tag;

size_t hash_value(const QModelIndex &idx) { return qHash(idx); }

namespace {

struct Mapping {
  explicit Mapping(const QModelIndex &_source_index) : source_index(_source_index) {}

  QModelIndex source_index;
};

struct tag_by_source {};
struct tag_by_pointer {};

}  // namespace

class MergedProxyModelPrivate {
 private:
  using MappingContainer = multi_index_container<Mapping*, indexed_by<hashed_unique<tag<tag_by_source>, member<Mapping, QModelIndex, &Mapping::source_index>>, ordered_unique<tag<tag_by_pointer>, identity<Mapping*>>>>;

 public:
  MappingContainer mappings_;
};

MergedProxyModel::MergedProxyModel(QObject *parent)
    : QAbstractProxyModel(parent),
      resetting_model_(nullptr),
      p_(new MergedProxyModelPrivate) {}

MergedProxyModel::~MergedProxyModel() { DeleteAllMappings(); }

void MergedProxyModel::DeleteAllMappings() {
  const auto &begin = p_->mappings_.get<tag_by_pointer>().begin();
  const auto &end = p_->mappings_.get<tag_by_pointer>().end();
  qDeleteAll(begin, end);
}

void MergedProxyModel::AddSubModel(const QModelIndex &source_parent, QAbstractItemModel *submodel) {

  QObject::connect(submodel, &QAbstractItemModel::modelAboutToBeReset, this, &MergedProxyModel::SubModelAboutToBeReset);
  QObject::connect(submodel, &QAbstractItemModel::modelReset, this, &MergedProxyModel::SubModelResetSlot);
  QObject::connect(submodel, &QAbstractItemModel::rowsAboutToBeInserted, this, &MergedProxyModel::RowsAboutToBeInserted);
  QObject::connect(submodel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &MergedProxyModel::RowsAboutToBeRemoved);
  QObject::connect(submodel, &QAbstractItemModel::rowsInserted, this, &MergedProxyModel::RowsInserted);
  QObject::connect(submodel, &QAbstractItemModel::rowsRemoved, this, &MergedProxyModel::RowsRemoved);
  QObject::connect(submodel, &QAbstractItemModel::dataChanged, this, &MergedProxyModel::DataChanged);

  QModelIndex proxy_parent = mapFromSource(source_parent);
  const int rows = submodel->rowCount();

  if (rows > 0) beginInsertRows(proxy_parent, 0, rows - 1);

  merge_points_.insert(submodel, source_parent);

  if (rows > 0) endInsertRows();
}

void MergedProxyModel::RemoveSubModel(const QModelIndex &source_parent) {

  // Find the submodel that the parent corresponded to
  QAbstractItemModel *submodel = merge_points_.key(source_parent);
  merge_points_.remove(submodel);

  // The submodel might have been deleted already so we must be careful not to dereference it.

  // Remove all the children of the item that got deleted
  QModelIndex proxy_parent = mapFromSource(source_parent);
  if (proxy_parent.isValid()) {
    const int row_count = rowCount(proxy_parent);
    if (row_count > 0) {
      resetting_model_ = submodel;
      beginRemoveRows(proxy_parent, 0, row_count - 1);
      endRemoveRows();
      resetting_model_ = nullptr;
    }
  }

  // Delete all the mappings that reference the submodel
  auto it = p_->mappings_.get<tag_by_pointer>().begin();
  auto end = p_->mappings_.get<tag_by_pointer>().end();
  while (it != end) {
    if ((*it)->source_index.model() == submodel) {
      delete *it;
      it = p_->mappings_.get<tag_by_pointer>().erase(it);
    }
    else {
      ++it;
    }
  }

}

void MergedProxyModel::setSourceModel(QAbstractItemModel *source_model) {

  if (sourceModel()) {
    QObject::disconnect(sourceModel(), &QAbstractItemModel::modelReset, this, &MergedProxyModel::SourceModelReset);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::rowsAboutToBeInserted, this, &MergedProxyModel::RowsAboutToBeInserted);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &MergedProxyModel::RowsAboutToBeRemoved);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::rowsInserted, this, &MergedProxyModel::RowsInserted);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &MergedProxyModel::RowsRemoved);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::dataChanged, this, &MergedProxyModel::DataChanged);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::layoutAboutToBeChanged, this, &MergedProxyModel::LayoutAboutToBeChanged);
    QObject::disconnect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &MergedProxyModel::LayoutChanged);
  }

  QAbstractProxyModel::setSourceModel(source_model);

  QObject::connect(sourceModel(), &QAbstractItemModel::modelReset, this, &MergedProxyModel::SourceModelReset);
  QObject::connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeInserted, this, &MergedProxyModel::RowsAboutToBeInserted);
  QObject::connect(sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &MergedProxyModel::RowsAboutToBeRemoved);
  QObject::connect(sourceModel(), &QAbstractItemModel::rowsInserted, this, &MergedProxyModel::RowsInserted);
  QObject::connect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &MergedProxyModel::RowsRemoved);
  QObject::connect(sourceModel(), &QAbstractItemModel::dataChanged, this, &MergedProxyModel::DataChanged);
  QObject::connect(sourceModel(), &QAbstractItemModel::layoutAboutToBeChanged, this, &MergedProxyModel::LayoutAboutToBeChanged);
  QObject::connect(sourceModel(), &QAbstractItemModel::layoutChanged, this, &MergedProxyModel::LayoutChanged);

}

void MergedProxyModel::SourceModelReset() {

  // Delete all mappings
  DeleteAllMappings();

  // Reset the proxy
  beginResetModel();

  // Clear the containers
  p_->mappings_.clear();
  merge_points_.clear();

  endResetModel();

}

void MergedProxyModel::SubModelAboutToBeReset() {

  QAbstractItemModel *submodel = qobject_cast<QAbstractItemModel*>(sender());

  QModelIndex source_parent = merge_points_.value(submodel);
  QModelIndex proxy_parent = mapFromSource(source_parent);

  if (proxy_parent.isValid()) {
    const int row_count = submodel->rowCount();
    if (row_count > 0) {
      resetting_model_ = submodel;
      beginRemoveRows(proxy_parent, 0, row_count - 1);
      endRemoveRows();
      resetting_model_ = nullptr;
    }
  }

  // Delete all the mappings that reference the submodel
  auto it = p_->mappings_.get<tag_by_pointer>().begin();
  auto end = p_->mappings_.get<tag_by_pointer>().end();
  while (it != end) {
    if ((*it)->source_index.model() == submodel) {
      delete *it;
      it = p_->mappings_.get<tag_by_pointer>().erase(it);
    }
    else {
      ++it;
    }
  }

}

void MergedProxyModel::SubModelResetSlot() {

  QAbstractItemModel *submodel = static_cast<QAbstractItemModel*>(sender());

  QModelIndex source_parent = merge_points_.value(submodel);
  QModelIndex proxy_parent = mapFromSource(source_parent);

  // "Insert" items from the newly reset submodel
  int count = submodel->rowCount();
  if (count > 0) {
    beginInsertRows(proxy_parent, 0, count - 1);
    endInsertRows();
  }

  Q_EMIT SubModelReset(proxy_parent, submodel);

}

QModelIndex MergedProxyModel::GetActualSourceParent(const QModelIndex &source_parent, QAbstractItemModel *model) const {

  if (!source_parent.isValid() && model != sourceModel()) {
    return merge_points_.value(model);
  }
  return source_parent;

}

void MergedProxyModel::RowsAboutToBeInserted(const QModelIndex &source_parent, const int start, const int end) {
  beginInsertRows(mapFromSource(GetActualSourceParent(source_parent, qobject_cast<QAbstractItemModel*>(sender()))), start, end);
}

void MergedProxyModel::RowsInserted(const QModelIndex &source_parent, const int start, const int end) {

  Q_UNUSED(source_parent)
  Q_UNUSED(start)
  Q_UNUSED(end)

  endInsertRows();

}

void MergedProxyModel::RowsAboutToBeRemoved(const QModelIndex &source_parent, const int start, const int end) {
  beginRemoveRows(mapFromSource(GetActualSourceParent(source_parent, qobject_cast<QAbstractItemModel*>(sender()))), start, end);
}

void MergedProxyModel::RowsRemoved(const QModelIndex &source_parent, const int start, const int end) {

  Q_UNUSED(source_parent)
  Q_UNUSED(start)
  Q_UNUSED(end)

  endRemoveRows();

}

QModelIndex MergedProxyModel::mapToSource(const QModelIndex &proxy_index) const {

  if (!proxy_index.isValid()) return QModelIndex();

  Mapping *mapping = static_cast<Mapping*>(proxy_index.internalPointer());
  if (p_->mappings_.get<tag_by_pointer>().find(mapping) == p_->mappings_.get<tag_by_pointer>().end())
    return QModelIndex();
  if (mapping->source_index.model() == resetting_model_) return QModelIndex();

  return mapping->source_index;

}

QModelIndex MergedProxyModel::mapFromSource(const QModelIndex &source_index) const {

  if (!source_index.isValid()) return QModelIndex();
  if (source_index.model() == resetting_model_) return QModelIndex();

  // Add a mapping if we don't have one already
  const auto &it = p_->mappings_.get<tag_by_source>().find(source_index);
  Mapping *mapping = nullptr;
  if (it != p_->mappings_.get<tag_by_source>().end()) {
    mapping = *it;
  }
  else {
    mapping = new Mapping(source_index);
    const_cast<MergedProxyModel*>(this)->p_->mappings_.insert(mapping);
  }

  return createIndex(source_index.row(), source_index.column(), mapping);

}

QModelIndex MergedProxyModel::index(const int row, const int column, const QModelIndex &parent) const {

  QModelIndex source_index;

  if (!parent.isValid()) {
    source_index = sourceModel()->index(row, column, QModelIndex());
  }
  else {
    QModelIndex source_parent = mapToSource(parent);
    const QAbstractItemModel *child_model = merge_points_.key(source_parent);

    if (child_model) {
      source_index = child_model->index(row, column, QModelIndex());
    }
    else {
      source_index = source_parent.model()->index(row, column, source_parent);
    }
  }

  return mapFromSource(source_index);

}

QModelIndex MergedProxyModel::parent(const QModelIndex &child) const {

  QModelIndex source_child = mapToSource(child);
  if (source_child.model() == sourceModel()) {
    return mapFromSource(source_child.parent());
  }

  if (!IsKnownModel(source_child.model())) return QModelIndex();

  if (!source_child.parent().isValid()) {
    return mapFromSource(merge_points_.value(GetModel(source_child)));
  }

  return mapFromSource(source_child.parent());

}

int MergedProxyModel::rowCount(const QModelIndex &parent) const {

  if (!parent.isValid()) return sourceModel()->rowCount(QModelIndex());

  QModelIndex source_parent = mapToSource(parent);
  if (!IsKnownModel(source_parent.model())) return 0;

  const QAbstractItemModel *child_model = merge_points_.key(source_parent);
  if (child_model) {
    // Query the source model but disregard what it says, so it gets a chance to lazy load
    source_parent.model()->rowCount(source_parent);

    return child_model->rowCount(QModelIndex());
  }

  return source_parent.model()->rowCount(source_parent);

}

int MergedProxyModel::columnCount(const QModelIndex &parent) const {

  if (!parent.isValid()) return sourceModel()->columnCount(QModelIndex());

  QModelIndex source_parent = mapToSource(parent);
  if (!IsKnownModel(source_parent.model())) return 0;

  const QAbstractItemModel *child_model = merge_points_.key(source_parent);
  if (child_model) return child_model->columnCount(QModelIndex());
  return source_parent.model()->columnCount(source_parent);

}

bool MergedProxyModel::hasChildren(const QModelIndex &parent) const {

  if (!parent.isValid()) return sourceModel()->hasChildren(QModelIndex());

  QModelIndex source_parent = mapToSource(parent);
  if (!IsKnownModel(source_parent.model())) return false;

  const QAbstractItemModel *child_model = merge_points_.key(source_parent);

  if (child_model) return child_model->hasChildren(QModelIndex()) || source_parent.model()->hasChildren(source_parent);
  return source_parent.model()->hasChildren(source_parent);

}

QVariant MergedProxyModel::data(const QModelIndex &proxy_index, const int role) const {

  QModelIndex source_index = mapToSource(proxy_index);
  if (!IsKnownModel(source_index.model())) return QVariant();

  return source_index.model()->data(source_index, role);

}

QMap<int, QVariant> MergedProxyModel::itemData(const QModelIndex &proxy_index) const {

  QModelIndex source_index = mapToSource(proxy_index);

  if (!source_index.isValid()) return sourceModel()->itemData(QModelIndex());
  return source_index.model()->itemData(source_index);

}

Qt::ItemFlags MergedProxyModel::flags(const QModelIndex &idx) const {

  QModelIndex source_index = mapToSource(idx);

  if (!source_index.isValid()) return sourceModel()->flags(QModelIndex());
  return source_index.model()->flags(source_index);

}

bool MergedProxyModel::setData(const QModelIndex &idx, const QVariant &value, const int role) {

  QModelIndex source_index = mapToSource(idx);

  if (!source_index.isValid()) {
    return sourceModel()->setData(idx, value, role);
  }

  return GetModel(idx)->setData(idx, value, role);

}

QStringList MergedProxyModel::mimeTypes() const {

  QStringList ret;
  ret << sourceModel()->mimeTypes();

  const QList<QAbstractItemModel*> models = merge_points_.keys();
  for (const QAbstractItemModel *model : models) {
    ret << model->mimeTypes();
  }

  return ret;

}

QMimeData *MergedProxyModel::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  // Only ask the first index's model
  const QAbstractItemModel *model = mapToSource(indexes[0]).model();
  if (!model) {
    return nullptr;
  }

  // Only ask about the indexes that are actually in that model
  QModelIndexList indexes_in_model;

  for (const QModelIndex &proxy_index : indexes) {
    QModelIndex source_index = mapToSource(proxy_index);
    if (source_index.model() != model) continue;
    indexes_in_model << source_index;
  }

  return model->mimeData(indexes_in_model);

}

bool MergedProxyModel::dropMimeData(const QMimeData *data, Qt::DropAction action, const int row, const int column, const QModelIndex &parent) {

  if (!parent.isValid()) {
    return false;
  }

  return sourceModel()->dropMimeData(data, action, row, column, parent);

}

QModelIndex MergedProxyModel::FindSourceParent(const QModelIndex &proxy_index) const {

  if (!proxy_index.isValid()) return QModelIndex();

  QModelIndex source_index = mapToSource(proxy_index);
  if (source_index.model() == sourceModel()) return source_index;
  return merge_points_.value(GetModel(source_index));

}

bool MergedProxyModel::canFetchMore(const QModelIndex &parent) const {

  QModelIndex source_index = mapToSource(parent);

  if (!source_index.isValid()) {
    return sourceModel()->canFetchMore(QModelIndex());
  }

  return source_index.model()->canFetchMore(source_index);

}

void MergedProxyModel::fetchMore(const QModelIndex &parent) {

  QModelIndex source_index = mapToSource(parent);

  if (!source_index.isValid()) {
    sourceModel()->fetchMore(QModelIndex());
  }
  else {
    GetModel(source_index)->fetchMore(source_index);
  }

}

QAbstractItemModel *MergedProxyModel::GetModel(const QModelIndex &source_index) const {

  // This is essentially const_cast<QAbstractItemModel*>(source_index.model()), but without the const_cast
  const QAbstractItemModel *const_model = source_index.model();
  if (const_model == sourceModel()) return sourceModel();
  const QList<QAbstractItemModel*> submodels = merge_points_.keys();
  for (QAbstractItemModel *submodel : submodels) {
    if (submodel == const_model) return submodel;
  }

  return nullptr;

}

void MergedProxyModel::DataChanged(const QModelIndex &top_left, const QModelIndex &bottom_right) {
  Q_EMIT dataChanged(mapFromSource(top_left), mapFromSource(bottom_right));
}

void MergedProxyModel::LayoutAboutToBeChanged() {

  old_merge_points_.clear();
  const QList<QAbstractItemModel*> models = merge_points_.keys();
  for (QAbstractItemModel *model : models) {
    old_merge_points_[model] = merge_points_.value(model);
  }

}

void MergedProxyModel::LayoutChanged() {

  const QList<QAbstractItemModel*> models = merge_points_.keys();
  for (QAbstractItemModel *model : models) {
    if (!old_merge_points_.contains(model)) continue;

    const int old_row = old_merge_points_.value(model).row();
    const int new_row = merge_points_.value(model).row();

    if (old_row != new_row) {
      beginResetModel();
      endResetModel();
      return;
    }
  }

}

bool MergedProxyModel::IsKnownModel(const QAbstractItemModel *model) const {

  return (model == this || model == sourceModel() || merge_points_.contains(const_cast<QAbstractItemModel*>(model)));

}

QModelIndexList MergedProxyModel::mapFromSource(const QModelIndexList &source_indexes) const {

  QModelIndexList ret;
  ret.reserve(source_indexes.count());
  for (const QModelIndex &idx : source_indexes) {
    ret << mapFromSource(idx);
  }
  return ret;

}

QModelIndexList MergedProxyModel::mapToSource(const QModelIndexList &proxy_indexes) const {

  QModelIndexList ret;
  ret.reserve(proxy_indexes.count());
  for (const QModelIndex &idx : proxy_indexes) {
    ret << mapToSource(idx);
  }
  return ret;

}
