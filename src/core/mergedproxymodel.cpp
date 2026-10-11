/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QObject>
#include <QMimeData>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QHash>
#include <QSet>
#include <QtAlgorithms>
#include <QAbstractItemModel>
#include <QAbstractProxyModel>
#include <QPersistentModelIndex>

#include "mergedproxymodel.h"

namespace {

struct Mapping {
  explicit Mapping(const QModelIndex &_source_index) : model(_source_index.model()), source_index(_source_index) {}

  // The model is kept separately, since the persistent index is invalidated when the model is deleted.
  const QAbstractItemModel *model;
  // Persistent, so the mapping follows the item when rows are inserted, removed or moved in the source model.
  QPersistentModelIndex source_index;
};

}  // namespace

class MergedProxyModelPrivate {
 public:
  Mapping *MappingForSourceIndex(const QModelIndex &source_index) const {
    Mapping *mapping = mappings_by_source_index_.value(source_index);
    if (mapping && mapping->source_index != source_index) return nullptr;
    return mapping;
  }

  // Whether the mapping exists, the internal pointer of a proxy index can point to a mapping that has been deleted.
  bool ContainsMapping(const Mapping *mapping) const { return mappings_.contains(const_cast<Mapping*>(mapping)); }

  void AddMapping(Mapping *mapping) {
    mappings_by_source_index_.insert(mapping->source_index, mapping);
    mappings_.insert(mapping);
  }

  // Rekeys the mappings after their persistent source indexes moved, and deletes the mappings for removed items.
  void UpdateMappings() {
    mappings_by_source_index_.clear();
    for (QSet<Mapping*>::iterator it = mappings_.begin(); it != mappings_.end();) {
      Mapping *mapping = *it;
      if (mapping->source_index.isValid()) {
        mappings_by_source_index_.insert(mapping->source_index, mapping);
        ++it;
      }
      else {
        delete mapping;
        it = mappings_.erase(it);
      }
    }
  }

  void DeleteMappings(const QAbstractItemModel *model) {
    for (QSet<Mapping*>::iterator it = mappings_.begin(); it != mappings_.end();) {
      Mapping *mapping = *it;
      if (mapping->model == model) {
        delete mapping;
        it = mappings_.erase(it);
      }
      else {
        ++it;
      }
    }
    UpdateMappings();
  }

  void DeleteAllMappings() {
    qDeleteAll(mappings_);
    mappings_by_source_index_.clear();
    mappings_.clear();
  }

 private:
  QHash<QModelIndex, Mapping*> mappings_by_source_index_;
  QSet<Mapping*> mappings_;
};

MergedProxyModel::MergedProxyModel(QObject *parent)
    : QAbstractProxyModel(parent),
      resetting_model_(nullptr),
      p_(new MergedProxyModelPrivate) {}

MergedProxyModel::~MergedProxyModel() { DeleteAllMappings(); }

void MergedProxyModel::DeleteAllMappings() {
  p_->DeleteAllMappings();
}

void MergedProxyModel::AddSubModel(const QModelIndex &source_parent, QAbstractItemModel *submodel) {

  QList<QMetaObject::Connection> &connections = submodel_connections_[submodel];
  connections << QObject::connect(submodel, &QAbstractItemModel::modelAboutToBeReset, this, &MergedProxyModel::SubModelAboutToBeReset);
  connections << QObject::connect(submodel, &QAbstractItemModel::modelReset, this, &MergedProxyModel::SubModelResetSlot);
  connections << QObject::connect(submodel, &QAbstractItemModel::rowsAboutToBeInserted, this, &MergedProxyModel::RowsAboutToBeInserted);
  connections << QObject::connect(submodel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &MergedProxyModel::RowsAboutToBeRemoved);
  connections << QObject::connect(submodel, &QAbstractItemModel::rowsInserted, this, &MergedProxyModel::RowsInserted);
  connections << QObject::connect(submodel, &QAbstractItemModel::rowsRemoved, this, &MergedProxyModel::RowsRemoved);
  connections << QObject::connect(submodel, &QAbstractItemModel::dataChanged, this, &MergedProxyModel::DataChanged);
  connections << QObject::connect(submodel, &QAbstractItemModel::layoutAboutToBeChanged, this, &MergedProxyModel::LayoutAboutToBeChanged);
  connections << QObject::connect(submodel, &QAbstractItemModel::layoutChanged, this, &MergedProxyModel::LayoutChanged);
  // The submodel can be deleted before RemoveSubModel() is called, e.g. together with a disconnected device's collection model.
  connections << QObject::connect(submodel, &QObject::destroyed, this, [this, submodel]() { ForgetSubModel(submodel, true); });

  QModelIndex proxy_parent = mapFromSource(source_parent);
  const int rows = submodel->rowCount();

  if (rows > 0) beginInsertRows(proxy_parent, 0, rows - 1);

  merge_points_.insert(submodel, source_parent);
  submodel_row_counts_.insert(submodel, rows);

  if (rows > 0) endInsertRows();

}

void MergedProxyModel::RemoveSubModel(const QModelIndex &source_parent) {

  // Find the submodel that the parent corresponded to
  QAbstractItemModel *submodel = merge_points_.key(source_parent);
  if (!submodel) return;

  // The submodel is still alive here, if it was deleted ForgetSubModel() was already called from its destroyed signal.
  ForgetSubModel(submodel, false);

}

void MergedProxyModel::ForgetSubModel(QAbstractItemModel *submodel, const bool destroyed) {

  if (!merge_points_.contains(submodel)) return;

  const QList<QMetaObject::Connection> connections = submodel_connections_.take(submodel);
  for (const QMetaObject::Connection &connection : connections) {
    QObject::disconnect(connection);
  }

  const QModelIndex source_parent = merge_points_.value(submodel);
  const QModelIndex proxy_parent = mapFromSource(source_parent);
  const int tracked_row_count = submodel_row_counts_.take(submodel);
  const int row_count = destroyed ? tracked_row_count : submodel->rowCount();

  if (destroyed) {
    // The submodel can't be asked for its rows or the parents of its items anymore, so the rows can't be removed one level at a time.
    if (row_count > 0) beginResetModel();
    merge_points_.remove(submodel);
    p_->DeleteMappings(submodel);
    if (row_count > 0) endResetModel();
    return;
  }

  // Remove the rows while the merge point still exists, so the parents of the submodel's items can be found, then remove the merge point before the views ask for the new row count.
  if (proxy_parent.isValid() && row_count > 0) {
    beginRemoveRows(proxy_parent, 0, row_count - 1);
    merge_points_.remove(submodel);
    p_->DeleteMappings(submodel);
    endRemoveRows();
  }
  else {
    merge_points_.remove(submodel);
    p_->DeleteMappings(submodel);
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

  // Reset the proxy
  beginResetModel();

  // Delete the mappings within the reset, before it they're still used by the proxy indexes.
  DeleteAllMappings();
  merge_points_.clear();

  // The merge points are gone, so stop forwarding the submodels' signals, which would otherwise be mapped to the top level.
  for (const QList<QMetaObject::Connection> &connections : std::as_const(submodel_connections_)) {
    for (const QMetaObject::Connection &connection : connections) {
      QObject::disconnect(connection);
    }
  }
  submodel_connections_.clear();
  submodel_row_counts_.clear();

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

  submodel_row_counts_[submodel] = 0;

  // Delete all the mappings that reference the submodel
  p_->DeleteMappings(submodel);

}

void MergedProxyModel::SubModelResetSlot() {

  QAbstractItemModel *submodel = static_cast<QAbstractItemModel*>(sender());

  QModelIndex source_parent = merge_points_.value(submodel);
  QModelIndex proxy_parent = mapFromSource(source_parent);

  // "Insert" items from the newly reset submodel
  int count = submodel->rowCount();
  if (count > 0) {
    beginInsertRows(proxy_parent, 0, count - 1);
    submodel_row_counts_[submodel] = count;
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

  QAbstractItemModel *model = qobject_cast<QAbstractItemModel*>(sender());
  if (!source_parent.isValid() && submodel_row_counts_.contains(model)) {
    submodel_row_counts_[model] += end - start + 1;
  }

  // The rows after the inserted rows moved, so rekey the mappings before the views map indexes again.
  p_->UpdateMappings();

  endInsertRows();

}

void MergedProxyModel::RowsAboutToBeRemoved(const QModelIndex &source_parent, const int start, const int end) {
  beginRemoveRows(mapFromSource(GetActualSourceParent(source_parent, qobject_cast<QAbstractItemModel*>(sender()))), start, end);
}

void MergedProxyModel::RowsRemoved(const QModelIndex &source_parent, const int start, const int end) {

  QAbstractItemModel *model = qobject_cast<QAbstractItemModel*>(sender());
  if (!source_parent.isValid() && submodel_row_counts_.contains(model)) {
    submodel_row_counts_[model] -= end - start + 1;
  }

  // The rows after the removed rows moved, so rekey the mappings, and delete the mappings for the removed rows, before the views map indexes again.
  p_->UpdateMappings();

  endRemoveRows();

}

QModelIndex MergedProxyModel::mapToSource(const QModelIndex &proxy_index) const {

  if (!proxy_index.isValid()) return QModelIndex();

  const Mapping *mapping = static_cast<const Mapping*>(proxy_index.internalPointer());
  if (!p_->ContainsMapping(mapping)) return QModelIndex();
  if (mapping->model == resetting_model_) return QModelIndex();

  return mapping->source_index;

}

QModelIndex MergedProxyModel::mapFromSource(const QModelIndex &source_index) const {

  if (!source_index.isValid()) return QModelIndex();
  if (source_index.model() == resetting_model_) return QModelIndex();

  // Add a mapping if we don't have one already
  Mapping *mapping = p_->MappingForSourceIndex(source_index);
  if (!mapping) {
    mapping = new Mapping(source_index);
    p_->AddMapping(mapping);
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

  const QModelIndex source_index = mapToSource(idx);
  if (!source_index.isValid()) {
    if (sourceModel()) {
      return sourceModel()->setData(QModelIndex(), value, role);
    }
    return false;
  }

  QAbstractItemModel *model = GetModel(source_index);
  if (!model) return false;

  return model->setData(source_index, value, role);

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

  Q_EMIT layoutAboutToBeChanged();

}

void MergedProxyModel::LayoutChanged() {

  // The source indexes of the mappings are persistent, so the model that changed its layout has already moved them.
  // Rekey the mappings, and move the proxy's persistent indexes to the new rows of their mappings.
  p_->UpdateMappings();

  const QModelIndexList old_indexes = persistentIndexList();
  QModelIndexList new_indexes;
  new_indexes.reserve(old_indexes.count());
  for (const QModelIndex &old_index : old_indexes) {
    Mapping *mapping = static_cast<Mapping*>(old_index.internalPointer());
    if (p_->ContainsMapping(mapping) && mapping->source_index.isValid()) {
      new_indexes << createIndex(mapping->source_index.row(), mapping->source_index.column(), mapping);
    }
    else {
      new_indexes << QModelIndex();
    }
  }
  changePersistentIndexList(old_indexes, new_indexes);

  Q_EMIT layoutChanged();

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
