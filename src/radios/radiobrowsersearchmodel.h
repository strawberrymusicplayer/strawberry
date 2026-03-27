/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#ifndef RADIOBROWSERSEARCHMODEL_H
#define RADIOBROWSERSEARCHMODEL_H

#include <QAbstractTableModel>
#include <QMimeData>
#include <QStringList>
#include <QVariant>

#include "radiochannel.h"

class RadioBrowserSearchModel : public QAbstractTableModel {
  Q_OBJECT

 public:
  explicit RadioBrowserSearchModel(QObject *parent = nullptr);

  enum class Column {
    Name = 0,
    Country,
    Tags,
    Codec,
    ColumnCount
  };

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &idx, const int role = Qt::DisplayRole) const override;
  QVariant headerData(const int section, const Qt::Orientation orientation, const int role = Qt::DisplayRole) const override;
  Qt::ItemFlags flags(const QModelIndex &idx) const override;

  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  QStringList mimeTypes() const override;

  void AddChannels(const RadioChannelList &channels);
  RadioChannel ChannelForRow(const int row) const;
  void Clear();

 private:
  RadioChannelList channels_;
};

#endif  // RADIOBROWSERSEARCHMODEL_H
