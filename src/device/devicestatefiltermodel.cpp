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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QAbstractItemModel>
#include <QVariant>

#include "devicemanager.h"
#include "devicestatefiltermodel.h"

DeviceStateFilterModel::DeviceStateFilterModel(QObject *parent, DeviceManager::State state)
  : QSortFilterProxyModel(parent),
    state_(state) {

  connect(this, SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(ProxyRowCountChanged(QModelIndex, int, int)));
  connect(this, SIGNAL(rowsRemoved(QModelIndex, int, int)), this, SLOT(ProxyRowCountChanged(QModelIndex, int, int)));
  connect(this, SIGNAL(modelReset()), this, SLOT(ProxyRowCountChanged()));

}

bool DeviceStateFilterModel::filterAcceptsRow(int row, const QModelIndex&) const {
  return sourceModel()->index(row, 0).data(DeviceManager::Role_State).toInt() != state_ && sourceModel()->index(row, 0).data(DeviceManager::Role_CopyMusic).toBool();
}

void DeviceStateFilterModel::ProxyRowCountChanged() {
  emit IsEmptyChanged(rowCount() == 0);
}

void DeviceStateFilterModel::ProxyRowCountChanged(QModelIndex index, int first, int last) {

  Q_UNUSED(index);
  Q_UNUSED(first);
  Q_UNUSED(last);
  emit IsEmptyChanged(rowCount() == 0);

}

void DeviceStateFilterModel::setSourceModel(QAbstractItemModel *sourceModel) {

  QSortFilterProxyModel::setSourceModel(sourceModel);
  setDynamicSortFilter(true);
  setSortCaseSensitivity(Qt::CaseInsensitive);
  sort(0);

}
