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

#include <QStandardItemModel>
#include <QMimeData>
#include <QStringList>
#include <QMap>

#include "radiochannel.h"

class RadioBrowserSearchModel : public QStandardItemModel {
 public:
  explicit RadioBrowserSearchModel(QObject *parent = nullptr);

  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  QStringList mimeTypes() const override;

  void AddChannel(int row, const RadioChannel &channel);
  RadioChannel ChannelForRow(int row) const;
  void ClearChannels();

 private:
  QMap<int, RadioChannel> channels_;
};

#endif  // RADIOBROWSERSEARCHMODEL_H
