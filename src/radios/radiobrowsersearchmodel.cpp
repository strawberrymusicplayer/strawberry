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
#include <QModelIndex>

#include "radiobrowsersearchmodel.h"
#include "radiochannel.h"
#include "radiomimedata.h"

using namespace Qt::Literals::StringLiterals;

RadioBrowserSearchModel::RadioBrowserSearchModel(QObject *parent) : QStandardItemModel(parent) {}

QMimeData *RadioBrowserSearchModel::mimeData(const QModelIndexList &indexes) const {

  RadioMimeData *mimedata = new RadioMimeData;
  QList<QUrl> urls;
  for (const QModelIndex &idx : indexes) {
    if (idx.column() != 0) continue;
    const RadioChannel channel = ChannelForRow(idx.row());
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

void RadioBrowserSearchModel::AddChannel(int row, const RadioChannel &channel) {
  channels_.insert(row, channel);
}

RadioChannel RadioBrowserSearchModel::ChannelForRow(int row) const {
  return channels_.value(row);
}

void RadioBrowserSearchModel::ClearChannels() {
  channels_.clear();
}
