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

#include <QList>
#include <QUrl>

#include "radiobrowsersearchmodel.h"
#include "radiochannel.h"
#include "radiomimedata.h"

using namespace Qt::Literals::StringLiterals;

RadioBrowserSearchModel::RadioBrowserSearchModel(QObject *parent) : QAbstractTableModel(parent) {}

int RadioBrowserSearchModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(channels_.size());
}

int RadioBrowserSearchModel::columnCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : static_cast<int>(Column::ColumnCount);
}

QVariant RadioBrowserSearchModel::data(const QModelIndex &idx, const int role) const {

  if (!idx.isValid() || idx.row() >= channels_.size()) return QVariant();

  const Column column = static_cast<Column>(idx.column());
  const RadioChannel &channel = channels_.at(idx.row());

  if (role == Qt::DisplayRole) {
    switch (column) {
      case Column::Name: return channel.name;
      case Column::Country: return channel.country;
      case Column::Tags: return channel.tags;
      case Column::Codec: return channel.codec;
      default: return QVariant();
    }
  }
  else if (role == Qt::ToolTipRole && column == Column::Name) {
    return channel.name;
  }

  return QVariant();

}

QVariant RadioBrowserSearchModel::headerData(const int section, const Qt::Orientation orientation, const int role) const {

  if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return QVariant();

  const Column column = static_cast<Column>(section);
  switch (column) {
    case Column::Name: return tr("Name");
    case Column::Country: return tr("Country");
    case Column::Tags: return tr("Tags");
    case Column::Codec: return tr("Codec");
    default: return QVariant();
  }

}

Qt::ItemFlags RadioBrowserSearchModel::flags(const QModelIndex &idx) const {

  if (!idx.isValid()) return Qt::NoItemFlags;
  return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;

}

QMimeData *RadioBrowserSearchModel::mimeData(const QModelIndexList &indexes) const {

  RadioMimeData *mimedata = new RadioMimeData;
  QList<QUrl> urls;
  for (const QModelIndex &idx : indexes) {
    if (idx.column() != 0) continue;
    const RadioChannel &channel = channels_.at(idx.row());
    if (!channel.url.isEmpty()) {
      Song song = channel.ToSong();
      urls << song.url();
      mimedata->songs << song;
    }
  }
  if (mimedata->songs.isEmpty()) {
    delete mimedata;
    return nullptr;
  }
  mimedata->setUrls(urls);
  return mimedata;

}

QStringList RadioBrowserSearchModel::mimeTypes() const {
  return QStringList() << u"text/uri-list"_s;
}

void RadioBrowserSearchModel::AddChannels(const RadioChannelList &channels) {

  if (channels.isEmpty()) return;

  const int first = static_cast<int>(channels_.size());
  const int last = first + static_cast<int>(channels.size()) - 1;
  beginInsertRows(QModelIndex(), first, last);
  channels_.append(channels);
  endInsertRows();

}

RadioChannel RadioBrowserSearchModel::ChannelForRow(const int row) const {
  if (row < 0 || row >= channels_.size()) return RadioChannel();
  return channels_.at(row);
}

void RadioBrowserSearchModel::Clear() {

  if (channels_.isEmpty()) return;

  beginResetModel();
  channels_.clear();
  endResetModel();

}
