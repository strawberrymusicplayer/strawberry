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

#include "config.h"

#include <QObject>
#include <QSortFilterProxyModel>
#include <QAbstractItemModel>

#include "devicemanager.h"
#include "devicestatefiltermodel.h"

DeviceStateFilterModel::DeviceStateFilterModel(QObject *parent, const DeviceManager::State state)
    : QSortFilterProxyModel(parent),
      state_(state) {

  QObject::connect(this, &DeviceStateFilterModel::rowsInserted, this, &DeviceStateFilterModel::ProxyRowCountChanged);
  QObject::connect(this, &DeviceStateFilterModel::rowsRemoved, this, &DeviceStateFilterModel::ProxyRowCountChanged);
  QObject::connect(this, &DeviceStateFilterModel::modelReset, this, &DeviceStateFilterModel::ProxyReset);

}

bool DeviceStateFilterModel::filterAcceptsRow(const int row, const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return sourceModel()->index(row, 0).data(DeviceManager::Role_State).value<DeviceManager::State>() != state_ && sourceModel()->index(row, 0).data(DeviceManager::Role_CopyMusic).toBool();
}

void DeviceStateFilterModel::ProxyRowCountChanged(const QModelIndex &idx, const int first, const int last) {

  Q_UNUSED(idx)
  Q_UNUSED(first);
  Q_UNUSED(last);

  Q_EMIT IsEmptyChanged(rowCount() == 0);

}

void DeviceStateFilterModel::ProxyReset() {

  Q_EMIT IsEmptyChanged(rowCount() == 0);

}

void DeviceStateFilterModel::setSourceModel(QAbstractItemModel *sourceModel) {

  QSortFilterProxyModel::setSourceModel(sourceModel);
  setDynamicSortFilter(true);
  setSortCaseSensitivity(Qt::CaseInsensitive);
  sort(0);

}
