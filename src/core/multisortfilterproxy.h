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

#ifndef MULTISORTFILTERPROXY_H
#define MULTISORTFILTERPROXY_H

#include "config.h"


#include <QObject>
#include <QSortFilterProxyModel>
#include <QList>
#include <QPair>
#include <QVariant>

class MultiSortFilterProxy : public QSortFilterProxyModel {
 public:
  MultiSortFilterProxy(QObject *parent = nullptr);

  void AddSortSpec(int role, Qt::SortOrder order = Qt::AscendingOrder);

 protected:
  bool lessThan(const QModelIndex &left, const QModelIndex &right) const;

 private:
  int Compare(const QVariant &left, const QVariant &right) const;

  typedef QPair<int, Qt::SortOrder> SortSpec;
  QList<SortSpec> sorting_;
};

#endif  // MULTISORTFILTERPROXY_H

