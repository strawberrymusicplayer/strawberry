/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
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

#ifndef INTERNETSEARCHSORTMODEL_H
#define INTERNETSEARCHSORTMODEL_H

#include <QSortFilterProxyModel>

class QObject;
class QModelIndex;

class InternetSearchSortModel : public QSortFilterProxyModel {
 public:
  InternetSearchSortModel(QObject *parent = nullptr);

 protected:
  bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

#endif  // INTERNETSEARCHSORTMODEL_H
