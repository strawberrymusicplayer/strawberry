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
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include <QMetaType>
#include <QDateTime>
#include <QVariant>
#include <QString>

#include "multisortfilterproxy.h"

MultiSortFilterProxy::MultiSortFilterProxy(QObject *parent)
    : QSortFilterProxyModel(parent) {}

void MultiSortFilterProxy::AddSortSpec(int role, Qt::SortOrder order) {
  sorting_ << SortSpec(role, order);
}

bool MultiSortFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const {

  for (const SortSpec &spec : sorting_) {
    const int ret = Compare(left.data(spec.first), right.data(spec.first));

    if (ret < 0) {
      return spec.second == Qt::AscendingOrder;
    }
    if (ret > 0) {
      return spec.second != Qt::AscendingOrder;
    }
  }

  return left.row() < right.row();

}

template<typename T>
static inline int DoCompare(T left, T right) {

  if (left < right) return -1;
  if (left > right) return 1;
  return 0;

}

int MultiSortFilterProxy::Compare(const QVariant &left, const QVariant &right) const {

  // Copied from the QSortFilterProxyModel::lessThan implementation, but returns -1, 0 or 1 instead of true or false.
  switch (left.userType()) {
    case QMetaType::UnknownType:   return (right.metaType().id() != QMetaType::UnknownType) ? -1 : 0;
    case QMetaType::Int:           return DoCompare(left.toInt(), right.toInt());
    case QMetaType::UInt:          return DoCompare(left.toUInt(), right.toUInt());
    case QMetaType::LongLong:      return DoCompare(left.toLongLong(), right.toLongLong());
    case QMetaType::ULongLong:     return DoCompare(left.toULongLong(), right.toULongLong());
    case QMetaType::Float:         return DoCompare(left.toFloat(), right.toFloat());
    case QMetaType::Double:        return DoCompare(left.toDouble(), right.toDouble());
    case QMetaType::Char:          return DoCompare(left.toChar(), right.toChar());
    case QMetaType::QDate:         return DoCompare(left.toDate(), right.toDate());
    case QMetaType::QTime:         return DoCompare(left.toTime(), right.toTime());
    case QMetaType::QDateTime:     return DoCompare(left.toDateTime(), right.toDateTime());
    case QMetaType::QString:
    default:
      if (isSortLocaleAware()) {
        return left.toString().localeAwareCompare(right.toString());
      }
      else {
        return left.toString().compare(right.toString(), sortCaseSensitivity());
      }
  }

}
