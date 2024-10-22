/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QMimeData>
#include <QPixmap>
#include <QPixmapCache>
#include <QRegularExpression>

#include "core/song.h"
#include "core/simpletreemodel.h"
#include "covermanager/albumcoverloader.h"
#include "covermanager/albumcoverloaderresult.h"
#include "radiomodel.h"
#include "radioservices.h"
#include "radioservice.h"
#include "radiomimedata.h"
#include "radiochannel.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kTreeIconSize = 22;
}

RadioModel::RadioModel(const SharedPtr<AlbumCoverLoader> albumcover_loader, const SharedPtr<RadioServices> radio_services, QObject *parent)
    : SimpleTreeModel<RadioItem>(new RadioItem(this), parent),
      albumcover_loader_(albumcover_loader),
      radio_services_(radio_services) {

  if (albumcover_loader_) {
    QObject::connect(&*albumcover_loader, &AlbumCoverLoader::AlbumCoverLoaded, this, &RadioModel::AlbumCoverLoaded);
  }

}

RadioModel::~RadioModel() {
  delete root_;
}

Qt::ItemFlags RadioModel::flags(const QModelIndex &idx) const {

  switch (IndexToItem(idx)->type) {
    case RadioItem::Type::Service:
    case RadioItem::Type::Channel:
      return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled;
    case RadioItem::Type::Root:
    case RadioItem::Type::LoadingIndicator:
    default:
      return Qt::ItemIsEnabled;
  }

}

QVariant RadioModel::data(const QModelIndex &idx, int role) const {

  if (!idx.isValid()) return QVariant();

  const RadioItem *item = IndexToItem(idx);
  if (!item) return QVariant();

  if (role == Qt::DecorationRole && item->type == RadioItem::Type::Channel) {
    return const_cast<RadioModel*>(this)->ChannelIcon(idx);
  }

  return data(item, role);

}

QVariant RadioModel::data(const RadioItem *item, int role) const {

  switch (role) {
    case Qt::DecorationRole:
      if (item->type == RadioItem::Type::Service) {
        return Song::IconForSource(item->source);
      }
      break;
    case Qt::DisplayRole:
      return item->DisplayText();
      break;
    case Role_Type:
      return QVariant::fromValue(item->type);
      break;
    case Role_SortText:
      return item->SortText();
      break;
    case Role_Source:
      return QVariant::fromValue(item->source);
      break;
    case Role_Homepage:{
      RadioService *service = radio_services_->ServiceBySource(item->source);
      if (service) return service->Homepage();
      break;
    }
    case Role_Donate:{
      RadioService *service = radio_services_->ServiceBySource(item->source);
      if (service) return service->Donate();
      break;
    }
    default:
      return QVariant();
  }

  return QVariant();

}

QStringList RadioModel::mimeTypes() const {
  return QStringList() << u"text/uri-list"_s;
}

QMimeData *RadioModel::mimeData(const QModelIndexList &indexes) const {

  if (indexes.isEmpty()) return nullptr;

  RadioMimeData *data = new RadioMimeData;
  QList<QUrl> urls;
  for (const QModelIndex &idx : indexes) {
    GetChildSongs(IndexToItem(idx), &urls, &data->songs);
  }

  data->setUrls(urls);
  data->name_for_new_playlist_ = Song::GetNameForNewPlaylist(data->songs);

  return data;

}

void RadioModel::Reset() {

  beginResetModel();
  container_nodes_.clear();
  items_.clear();
  pending_art_.clear();
  pending_cache_keys_.clear();
  delete root_;
  root_ = new RadioItem(this);
  endResetModel();

}

void RadioModel::AddChannels(const RadioChannelList &channels) {

  for (const RadioChannel &channel : channels) {
    RadioItem *container = nullptr;
    if (container_nodes_.contains(channel.source)) {
      container = container_nodes_.value(channel.source);
    }
    else {
      beginInsertRows(ItemToIndex(root_), static_cast<int>(root_->children.count()), static_cast<int>(root_->children.count()));
      RadioItem *item = new RadioItem(RadioItem::Type::Service, root_);
      item->source = channel.source;
      item->display_text = Song::DescriptionForSource(channel.source);
      item->sort_text = SortText(Song::TextForSource(channel.source));
      container_nodes_.insert(channel.source, item);
      endInsertRows();
      container = item;
    }
    beginInsertRows(ItemToIndex(container), static_cast<int>(container->children.count()), static_cast<int>(container->children.count()));
    RadioItem *item = new RadioItem(RadioItem::Type::Channel, container);
    item->source = channel.source;
    item->display_text = channel.name;
    item->sort_text = SortText(Song::TextForSource(channel.source) + " - "_L1 + channel.name);
    item->channel = channel;
    items_ << item;
    endInsertRows();
  }

}

bool RadioModel::IsPlayable(const QModelIndex &idx) const {

  return idx.data(Role_Type).value<RadioItem::Type>() == RadioItem::Type::Channel;

}

bool RadioModel::CompareItems(const RadioItem *a, const RadioItem *b) const {

  QVariant left(data(a, RadioModel::Role_SortText));
  QVariant right(data(b, RadioModel::Role_SortText));

  if (left.metaType().id() == QMetaType::Int)
    return left.toInt() < right.toInt();
  else return left.toString() < right.toString();

}

void RadioModel::GetChildSongs(RadioItem *item, QList<QUrl> *urls, SongList *songs) const {

  switch (item->type) {
    case RadioItem::Type::Service:{
      QList<RadioItem*> children = item->children;
      std::sort(children.begin(), children.end(), std::bind(&RadioModel::CompareItems, this, std::placeholders::_1, std::placeholders::_2));
      for (RadioItem *child : children) {
        GetChildSongs(child, urls, songs);
      }
      break;
    }
    case RadioItem::Type::Channel:
      if (!urls->contains(item->channel.url)) {
        urls->append(item->channel.url);
        songs->append(item->channel.ToSong());
      }
      break;
    default:
      break;
  }

}

SongList RadioModel::GetChildSongs(const QModelIndexList &indexes) const {

  QList<QUrl> urls;
  SongList songs;
  for (const QModelIndex &idx : indexes) {
    GetChildSongs(IndexToItem(idx), &urls, &songs);
  }
  return songs;

}

SongList RadioModel::GetChildSongs(const QModelIndex &idx) const {
  return GetChildSongs(QModelIndexList() << idx);
}

QString RadioModel::ChannelIconPixmapCacheKey(const QModelIndex &idx) const {

  QStringList path;
  QModelIndex idx_copy = idx;
  while (idx_copy.isValid()) {
    path.prepend(idx_copy.data().toString());
    idx_copy = idx_copy.parent();
  }

  return path.join(u'/');

}

QPixmap RadioModel::ServiceIcon(const QModelIndex &idx) const {
  return Song::IconForSource(static_cast<Song::Source>(idx.data(Role_Source).toInt())).pixmap(kTreeIconSize, kTreeIconSize);
}

QPixmap RadioModel::ServiceIcon(RadioItem *item) const {
  return Song::IconForSource(item->source).pixmap(kTreeIconSize, kTreeIconSize);
}

QPixmap RadioModel::ChannelIcon(const QModelIndex &idx) {

  if (!idx.isValid()) return QPixmap();

  RadioItem *item = IndexToItem(idx);
  if (!item) return ServiceIcon(idx);

  const QString cache_key = ChannelIconPixmapCacheKey(idx);

  QPixmap cached_pixmap;
  if (QPixmapCache::find(cache_key, &cached_pixmap)) {
    return cached_pixmap;
  }

  if (pending_cache_keys_.contains(cache_key)) {
    return ServiceIcon(idx);
  }

  SongList songs = GetChildSongs(idx);
  if (!songs.isEmpty()) {
    Song song = songs.first();
    song.set_art_automatic(item->channel.thumbnail_url);
    const quint64 id = albumcover_loader_->LoadImageAsync(AlbumCoverLoaderOptions(AlbumCoverLoaderOptions::Option::ScaledImage | AlbumCoverLoaderOptions::Option::PadScaledImage, QSize(kTreeIconSize, kTreeIconSize)), song);
    pending_art_[id] = ItemAndCacheKey(item, cache_key);
    pending_cache_keys_.insert(cache_key);
  }

  return ServiceIcon(idx);

}

void RadioModel::AlbumCoverLoaded(const quint64 id, const AlbumCoverLoaderResult &result) {

  if (!pending_art_.contains(id)) return;

  ItemAndCacheKey item_and_cache_key = pending_art_.take(id);
  RadioItem *item = item_and_cache_key.first;
  if (!item) return;

  const QString &cache_key = item_and_cache_key.second;

  pending_cache_keys_.remove(cache_key);

  if (!result.success || result.image_scaled.isNull() || result.type == AlbumCoverLoaderResult::Type::Unset) {
    QPixmapCache::insert(cache_key, ServiceIcon(item));
  }
  else {
    QPixmapCache::insert(cache_key, QPixmap::fromImage(result.image_scaled));
  }

  const QModelIndex idx = ItemToIndex(item);
  if (!idx.isValid()) return;

  Q_EMIT dataChanged(idx, idx);

}

QString RadioModel::SortText(QString text) {

  if (text.isEmpty()) {
    text = " unknown"_L1;
  }
  else {
    text = text.toLower();
  }
  static const QRegularExpression regex_words_and_whitespaces(u"[^\\w ]"_s, QRegularExpression::UseUnicodePropertiesOption);
  text = text.remove(regex_words_and_whitespaces);

  return text;

}
