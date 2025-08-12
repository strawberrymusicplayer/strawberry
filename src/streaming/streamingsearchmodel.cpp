/*
 * Strawberry Music Player
 * This code was part of Clementine (GlobalSearch)
 * Copyright 2012, David Sansome <me@davidsansome.com>
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

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMimeData>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QSize>

#include "core/iconloader.h"
#include "core/mimedata.h"
#include "streamsongmimedata.h"
#include "streamingservice.h"
#include "streamingsearchmodel.h"
#include "streamingsearchview.h"

using namespace Qt::Literals::StringLiterals;

StreamingSearchModel::StreamingSearchModel(StreamingServicePtr service, QObject *parent)
    : QStandardItemModel(parent),
      service_(service),
      proxy_(nullptr),
      use_pretty_covers_(true),
      artist_icon_(IconLoader::Load(u"folder-sound"_s)),
      album_icon_(IconLoader::Load(u"cdcase"_s)) {

  group_by_[0] = CollectionModel::GroupBy::AlbumArtist;
  group_by_[1] = CollectionModel::GroupBy::AlbumDisc;
  group_by_[2] = CollectionModel::GroupBy::None;

  QList<QSize> nocover_sizes = album_icon_.availableSizes();
  no_cover_icon_ = album_icon_.pixmap(nocover_sizes.last()).scaled(CollectionModel::kPrettyCoverSize, CollectionModel::kPrettyCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

}

void StreamingSearchModel::AddResults(const StreamingSearchView::ResultList &results) {

  for (const StreamingSearchView::Result &result : results) {
    QStandardItem *parent = invisibleRootItem();

    // Find (or create) the container nodes for this result if we can.
    ContainerKey key;
    parent = BuildContainers(result.metadata_, parent, &key);

    // Create the item
    QStandardItem *item = new QStandardItem;
    item->setText(result.metadata_.TitleWithCompilationArtist());
    item->setData(QVariant::fromValue(result), Role_Result);

    parent->appendRow(item);

  }

}

QStandardItem *StreamingSearchModel::BuildContainers(const Song &s, QStandardItem *parent, ContainerKey *key, const int level) {

  if (level >= 3) {
    return parent;
  }

  bool has_artist_icon = false;
  bool has_album_icon = false;
  QString display_text;
  QString sort_text;
  QString unique_tag;

  switch (group_by_[level]) {

    case CollectionModel::GroupBy::AlbumArtist:
      if (s.is_compilation()) {
        display_text = tr("Various artists");
        sort_text = "aaaaaa"_L1;
      }
      else {
        display_text = CollectionModel::TextOrUnknown(s.effective_albumartist());
        sort_text = CollectionModel::SortTextForName(s.effective_albumartistsort(), true);
      }
      has_artist_icon = true;
      break;

    case CollectionModel::GroupBy::Artist:
      if (s.is_compilation()) {
        display_text = tr("Various artists");
        sort_text = "aaaaaa"_L1;
      }
      else {
        display_text = CollectionModel::TextOrUnknown(s.artist());
        sort_text = CollectionModel::SortTextForName(s.effective_artistsort(), true);
      }
      has_artist_icon = true;
      break;

    case CollectionModel::GroupBy::Album:
      display_text = CollectionModel::TextOrUnknown(s.album());
      sort_text = CollectionModel::SortTextForName(s.effective_albumsort(), false);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::AlbumDisc:{
      const int disc = std::max(0, s.disc());
      display_text = CollectionModel::PrettyAlbumDisc(s.album(), disc);
      sort_text = CollectionModel::SortTextForName(s.effective_albumsort(), false) + CollectionModel::SortTextForNumber(disc);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;
    }

    case CollectionModel::GroupBy::YearAlbum:{
      const int year = std::max(0, s.year());
      display_text = CollectionModel::PrettyYearAlbum(year, s.album());
      sort_text = CollectionModel::SortTextForNumber(year) + CollectionModel::SortTextForName(s.effective_albumsort(), false);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;
    }

    case CollectionModel::GroupBy::YearAlbumDisc:{
      const int year = std::max(0, s.year());
      const int disc = std::max(0, s.disc());
      display_text = CollectionModel::PrettyYearAlbumDisc(year, s.album(), disc);
      sort_text = CollectionModel::SortTextForNumber(year) + CollectionModel::SortTextForName(s.effective_albumsort(), false) + CollectionModel::SortTextForNumber(disc);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;
    }

    case CollectionModel::GroupBy::OriginalYearAlbum:{
      const int year = std::max(0, s.effective_originalyear());
      display_text = CollectionModel::PrettyYearAlbum(year, s.album());
      sort_text = CollectionModel::SortTextForNumber(year) + CollectionModel::SortTextForName(s.effective_albumsort(), false);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;
    }

    case CollectionModel::GroupBy::OriginalYearAlbumDisc:{
      const int year = std::max(0, s.effective_originalyear());
      const int disc = std::max(0, s.disc());
      display_text = CollectionModel::PrettyYearAlbumDisc(year, s.album(), disc);
      sort_text = CollectionModel::SortTextForNumber(year) + CollectionModel::SortTextForName(s.effective_albumsort(), false) + CollectionModel::SortTextForNumber(disc);
      unique_tag = s.album_id();
      has_album_icon = true;
      break;
    }

    case CollectionModel::GroupBy::Disc:
      display_text = CollectionModel::PrettyDisc(s.disc());
      sort_text = CollectionModel::SortText(display_text);
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::Year:{
      const int year = qMax(0, s.year());
      display_text = QString::number(year);
      sort_text = CollectionModel::SortTextForNumber(year) + QLatin1Char(' ');
      break;
    }

    case CollectionModel::GroupBy::OriginalYear:{
      const int year = qMax(0, s.effective_originalyear());
      display_text = QString::number(year);
      sort_text = CollectionModel::SortTextForNumber(year) + QLatin1Char(' ');
      break;
    }

    case CollectionModel::GroupBy::Genre:
      display_text = CollectionModel::TextOrUnknown(s.genre());
      sort_text = CollectionModel::SortText(s.genre());
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::Composer:
      display_text = CollectionModel::TextOrUnknown(s.composer());
      sort_text = CollectionModel::SortTextForName(s.composer(), true);
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::Performer:
      display_text = CollectionModel::TextOrUnknown(s.performer());
      sort_text = CollectionModel::SortTextForName(s.performer(), true);
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::Grouping:
      display_text = CollectionModel::TextOrUnknown(s.grouping());
      sort_text = CollectionModel::SortText(s.grouping());
      has_album_icon = true;
      break;

    case CollectionModel::GroupBy::FileType:
      display_text = s.TextForFiletype();
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy::Format:
      if (s.samplerate() <= 0) {
        display_text = s.TextForFiletype();
      }
      else {
        if (s.bitdepth() <= 0) {
          display_text = QStringLiteral("%1 (%2)").arg(s.TextForFiletype(), QString::number(s.samplerate() / 1000.0, 'G', 5));
        }
        else {
          display_text = QStringLiteral("%1 (%2/%3)").arg(s.TextForFiletype(), QString::number(s.samplerate() / 1000.0, 'G', 5), QString::number(s.bitdepth()));
        }
      }
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy::Samplerate:
      display_text = QString::number(s.samplerate());
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy::Bitdepth:
      display_text = QString::number(s.bitdepth());
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy::Bitrate:
      display_text = QString::number(s.bitrate());
      sort_text = display_text;
      break;

    case CollectionModel::GroupBy::None:
    case CollectionModel::GroupBy::GroupByCount:
      return parent;
  }

  if (display_text.isEmpty() || sort_text.isEmpty()) {
    display_text = "Unknown"_L1;
  }

  // Find a container for this level
  key->group_[level] = display_text + unique_tag;
  QStandardItem *container = nullptr;
  if (containers_.contains(*key)) {
    container = containers_.value(*key);
  }
  else {
    container = new QStandardItem(display_text);
    container->setData(sort_text, CollectionModel::Role_SortText);
    container->setData(static_cast<int>(group_by_[level]), CollectionModel::Role_ContainerType);

    if (has_artist_icon) {
      container->setIcon(artist_icon_);
    }
    else if (has_album_icon) {
      if (use_pretty_covers_) {
        container->setData(no_cover_icon_, Qt::DecorationRole);
      }
      else {
        container->setIcon(album_icon_);
      }
    }

    parent->appendRow(container);
    containers_[*key] = container;
  }

  // Create the container for the next level.
  return BuildContainers(s, container, key, level + 1);

}

void StreamingSearchModel::Clear() {

  containers_.clear();
  clear();

}

StreamingSearchView::ResultList StreamingSearchModel::GetChildResults(const QModelIndexList &indexes) const {

  QList<QStandardItem*> items;
  items.reserve(indexes.count());
  for (const QModelIndex &idx : indexes) {
    items << itemFromIndex(idx);
  }
  return GetChildResults(items);

}

StreamingSearchView::ResultList StreamingSearchModel::GetChildResults(const QList<QStandardItem*> &items) const {

  StreamingSearchView::ResultList results;
  QSet<const QStandardItem*> visited;

  for (QStandardItem *item : items) {
    GetChildResults(item, &results, &visited);
  }

  return results;

}

void StreamingSearchModel::GetChildResults(const QStandardItem *item, StreamingSearchView::ResultList *results, QSet<const QStandardItem*> *visited) const {

  if (visited->contains(item)) {
    return;
  }
  visited->insert(item);

  // Does this item have children?
  if (item->rowCount() > 0) {
    const QModelIndex parent_proxy_index = proxy_->mapFromSource(item->index());

    // Yes - visit all the children, but do so through the proxy, so we get them in the right order.
    for (int i = 0; i < item->rowCount(); ++i) {
      const QModelIndex proxy_index = parent_proxy_index.model()->index(i, 0, parent_proxy_index);
      const QModelIndex idx = proxy_->mapToSource(proxy_index);
      GetChildResults(itemFromIndex(idx), results, visited);
    }
  }
  else {
    // No - maybe it's a song, add its result if valid
    QVariant result = item->data(Role_Result);
    if (result.isValid()) {
      results->append(result.value<StreamingSearchView::Result>());
    }
  }

}

QMimeData *StreamingSearchModel::mimeData(const QModelIndexList &indexes) const {

  return LoadTracks(GetChildResults(indexes));

}

namespace {

void GatherResults(const QStandardItem *parent, StreamingSearchView::ResultList *results) {

  QVariant result_variant = parent->data(StreamingSearchModel::Role_Result);
  if (result_variant.isValid()) {
    StreamingSearchView::Result result = result_variant.value<StreamingSearchView::Result>();
    (*results).append(result);
  }

  for (int i = 0; i < parent->rowCount(); ++i) {
    GatherResults(parent->child(i), results);
  }

}

}  // namespace

void StreamingSearchModel::SetGroupBy(const CollectionModel::Grouping grouping, const bool regroup_now) {

  const CollectionModel::Grouping old_group_by = group_by_;
  group_by_ = grouping;

  if (regroup_now && group_by_ != old_group_by) {
    // Walk the tree gathering the results we have already
    StreamingSearchView::ResultList results;
    GatherResults(invisibleRootItem(), &results);

    // Reset the model and re-add all the results using the new grouping.
    Clear();
    AddResults(results);
  }

}

MimeData *StreamingSearchModel::LoadTracks(const StreamingSearchView::ResultList &results) const {

  if (results.isEmpty()) {
    return nullptr;
  }

  SongList songs;
  QList<QUrl> urls;
  songs.reserve(results.count());
  urls.reserve(results.count());
  for (const StreamingSearchView::Result &result : results) {
    songs.append(result.metadata_);
    urls << result.metadata_.url();
  }

  StreamSongMimeData *streaming_song_mime_data = new StreamSongMimeData(service_);
  streaming_song_mime_data->songs = songs;
  MimeData *mime_data = streaming_song_mime_data;

  mime_data->setUrls(urls);

  return mime_data;

}
