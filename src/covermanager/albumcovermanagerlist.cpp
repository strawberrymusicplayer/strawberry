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

#include <utility>

#include <QWidget>
#include <QStringList>
#include <QUrl>
#include <QList>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QDropEvent>

#include "includes/scoped_ptr.h"
#include "core/song.h"
#include "core/songmimedata.h"
#include "collection/collectionbackend.h"
#include "albumcovermanager.h"
#include "albumcovermanagerlist.h"

AlbumCoverManagerList::AlbumCoverManagerList(QWidget *parent) : QListWidget(parent), manager_(nullptr) {}

QMimeData *AlbumCoverManagerList::mimeData(const QList<QListWidgetItem*> &items) const {

  // Get songs
  SongList songs;
  for (QListWidgetItem *item : items) {
    songs << manager_->GetSongsInAlbum(indexFromItem(item));
  }

  if (songs.isEmpty()) return nullptr;

  // Get URLs from the songs
  QList<QUrl> urls;
  urls.reserve(songs.count());
  for (const Song &song : std::as_const(songs)) {
    urls << song.url();
  }

  // Get the QAbstractItemModel data so the picture works
  ScopedPtr<QMimeData> orig_data(QListWidget::mimeData(items));

  SongMimeData *mime_data = new SongMimeData;
  mime_data->backend = manager_->collection_backend();
  mime_data->songs = songs;
  mime_data->setUrls(urls);
  mime_data->setData(orig_data->formats()[0], orig_data->data(orig_data->formats()[0]));

  return mime_data;

}
