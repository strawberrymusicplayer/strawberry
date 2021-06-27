/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COLLECTIONFILTER_H
#define COLLECTIONFILTER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "core/song.h"

class CollectionItem;

class CollectionFilter : public QSortFilterProxyModel {
  Q_OBJECT

 public:
  explicit CollectionFilter(QObject *parent = nullptr);

 protected:
  bool filterAcceptsRow(const int source_row, const QModelIndex &source_parent) const override;

 private:
  static const QStringList Operators;
  struct Filter {
   public:
    Filter(const QString &_field = QString(), const QVariant &_value = QVariant(), const QString &_foperator = QString()) : field(_field), value(_value), foperator(_foperator) {}
    QString field;
    QVariant value;
    QString foperator;
  };
  using FilterList = QMap<QString, Filter>;
  static bool ItemMatchesFilters(CollectionItem *item, const FilterList &filters, const QString &filter_text);
  static bool ItemMetadataMatchesFilters(const Song &metadata, const FilterList &filters, const QString &filter_text);
  static bool ItemMetadataMatchesFilterText(const Song &metadata, const QString &filter_text);
  static QVariant DataFromField(const QString &field, const Song &metadata);
  static bool FieldValueMatchesData(const QVariant &value, const QVariant &data, const QString &foperator);
  template<typename T>
  static bool FieldNumericalValueMatchesData(const T value, const QString &foperator, const T data);
  static bool FieldIntValueMatchesData(const int value, const QString &foperator, const int data);
  static bool FieldUIntValueMatchesData(const uint value, const QString &foperator, const uint data);
  static bool FieldLongLongValueMatchesData(const qint64 value, const QString &foperator, const qint64 data);
  static bool FieldFloatValueMatchesData(const float value, const QString &foperator, const float data);
  static bool ContainsOperators(const QString &token);
};

#endif  // COLLECTIONFILTER_H
