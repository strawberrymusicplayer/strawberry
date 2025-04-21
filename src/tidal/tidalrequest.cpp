/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QImageReader>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "constants/timeconstants.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalbaserequest.h"
#include "tidalrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kResourcesUrl[] = "https://resources.tidal.com";
constexpr int kMaxConcurrentArtistsRequests = 3;
constexpr int kMaxConcurrentAlbumsRequests = 3;
constexpr int kMaxConcurrentSongsRequests = 3;
constexpr int kMaxConcurrentArtistAlbumsRequests = 3;
constexpr int kMaxConcurrentAlbumSongsRequests = 3;
constexpr int kMaxConcurrentAlbumCoverRequests = 1;
constexpr int kFlushRequestsDelay = 200;
}  // namespace

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      network_(network),
      timer_flush_requests_(new QTimer(this)),
      query_type_(query_type),
      fetchalbums_(service->fetchalbums()),
      coversize_(service->coversize()),
      query_id_(-1),
      finished_(false),
      artists_requests_total_(0),
      artists_requests_active_(0),
      artists_requests_received_(0),
      artists_total_(0),
      artists_received_(0),
      albums_requests_total_(0),
      albums_requests_active_(0),
      albums_requests_received_(0),
      albums_total_(0),
      albums_received_(0),
      songs_requests_total_(0),
      songs_requests_active_(0),
      songs_requests_received_(0),
      songs_total_(0),
      songs_received_(0),
      artist_albums_requests_total_(),
      artist_albums_requests_active_(0),
      artist_albums_requests_received_(0),
      artist_albums_total_(0),
      artist_albums_received_(0),
      album_songs_requests_active_(0),
      album_songs_requests_received_(0),
      album_songs_requests_total_(0),
      album_songs_total_(0),
      album_songs_received_(0),
      album_covers_requests_total_(0),
      album_covers_requests_active_(0),
      album_covers_requests_received_(0) {

  timer_flush_requests_->setInterval(kFlushRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &TidalRequest::FlushRequests);

}

void TidalRequest::Process() {

  switch (query_type_) {
    case Type::FavouriteArtists:
      GetArtists();
      break;
    case Type::FavouriteAlbums:
      GetAlbums();
      break;
    case Type::FavouriteSongs:
      GetSongs();
      break;
    case Type::SearchArtists:
      ArtistsSearch();
      break;
    case Type::SearchAlbums:
      AlbumsSearch();
      break;
    case Type::SearchSongs:
      SongsSearch();
      break;
    default:
      Error(u"Invalid query type."_s);
      break;
  }

}

void TidalRequest::StartRequests() {

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void TidalRequest::FlushRequests() {

  if (!artists_requests_queue_.isEmpty()) {
    FlushArtistsRequests();
    return;
  }

  if (!albums_requests_queue_.isEmpty()) {
    FlushAlbumsRequests();
    return;
  }

  if (!artist_albums_requests_queue_.isEmpty()) {
    FlushArtistAlbumsRequests();
    return;
  }

  if (!album_songs_requests_queue_.isEmpty()) {
    FlushAlbumSongsRequests();
    return;
  }

  if (!songs_requests_queue_.isEmpty()) {
    FlushSongsRequests();
    return;
  }

  if (!album_cover_requests_queue_.isEmpty()) {
    FlushAlbumCoverRequests();
    return;
  }

  timer_flush_requests_->stop();

}

void TidalRequest::Search(const int query_id, const QString &search_text) {
  query_id_ = query_id;
  search_text_ = search_text;
}

void TidalRequest::GetArtists() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving artists..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void TidalRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);

  ++artists_requests_total_;

  StartRequests();

}

void TidalRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    const Request request = artists_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchArtists) parameters << Param(u"query"_s, search_text_);
    if (request.limit > 0) parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteArtists) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/artists").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchArtists) {
      reply = CreateRequest(u"search/artists"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

    ++artists_requests_active_;

  }

}

void TidalRequest::GetAlbums() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving albums..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void TidalRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);

  ++albums_requests_total_;

  StartRequests();

}

void TidalRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    const Request request = albums_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchAlbums) parameters << Param(u"query"_s, search_text_);
    if (request.limit > 0) parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteAlbums) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/albums").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchAlbums) {
      reply = CreateRequest(u"search/albums"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

    ++albums_requests_active_;

  }

}

void TidalRequest::GetSongs() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving songs..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void TidalRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);

  ++songs_requests_total_;

  StartRequests();

}

void TidalRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    const Request request = songs_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchSongs) parameters << Param(u"query"_s, search_text_);
    if (request.limit > 0) parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteSongs) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchSongs) {
      reply = CreateRequest(u"search/tracks"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

    ++songs_requests_active_;

  }

}

void TidalRequest::ArtistsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void TidalRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void TidalRequest::AlbumsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void TidalRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void TidalRequest::SongsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsSearchRequest();

}

void TidalRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void TidalRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  --artists_requests_active_;
  ++artists_requests_received_;

  if (finished_) return;

  int offset = 0;
  int artists_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, limit_requested, &offset, &artists_received]() { ArtistsFinishCheck(limit_requested, offset, artists_received); });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;

  if (!json_object.contains("limit"_L1) ||
      !json_object.contains("offset"_L1) ||
      !json_object.contains("totalNumberOfItems"_L1) ||
      !json_object.contains("items"_L1)) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }
  //int limit = json_object["limit"].toInt();
  offset = json_object["offset"_L1].toInt();
  const int artists_total = json_object["totalNumberOfItems"_L1].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QStringLiteral("totalNumberOfItems returned does not match previous totalNumberOfItems! %1 != %2").arg(artists_total).arg(artists_total_));
    return;
  }

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));
  }

  const JsonArrayResult json_array_result = GetJsonArray(json_object, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {  // Empty array means no results
    return;
  }

  for (const QJsonValue &value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item in array is not a object."_s);
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1)) {
      const QJsonValue json_item = object_item["item"_L1];
      if (!json_item.isObject()) {
        Error(u"Invalid Json reply, item in array is not a object."_s, json_item);
        continue;
      }
      object_item = json_item.toObject();
    }

    if (!object_item.contains("id"_L1) || !object_item.contains("name"_L1)) {
      Error(u"Invalid Json reply, item missing id or album."_s, object_item);
      continue;
    }

    Artist artist;
    if (object_item["id"_L1].isString()) {
      artist.artist_id = object_item["id"_L1].toString();
    }
    else {
      artist.artist_id = QString::number(object_item["id"_L1].toInt());
    }
    artist.artist = object_item["name"_L1].toString();

    if (artist_albums_requests_pending_.contains(artist.artist_id)) continue;

    ArtistAlbumsRequest request;
    request.artist = artist;
    artist_albums_requests_pending_.insert(artist.artist_id, request);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));

}

void TidalRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if (finished_) return;

  if ((limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (query_type_ == Type::FavouriteArtists) AddArtistsRequest(offset_next);
      else if (query_type_ == Type::SearchArtists) AddArtistsSearchRequest(offset_next);
    }
  }

  if (artists_requests_queue_.isEmpty() && artists_requests_active_ <= 0) {  // Artist query is finished, get all albums for all artists.

    // Get artist albums
    const QList<ArtistAlbumsRequest> requests = artist_albums_requests_pending_.values();
    for (const ArtistAlbumsRequest &request : requests) {
      AddArtistAlbumsRequest(request.artist);
    }
    artist_albums_requests_pending_.clear();

    if (artist_albums_requests_total_ > 0) {
      if (artist_albums_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving albums for %1 artist...").arg(artist_albums_requests_total_));
      else Q_EMIT UpdateStatus(query_id_, tr("Receiving albums for %1 artists...").arg(artist_albums_requests_total_));
      Q_EMIT UpdateProgress(query_id_, 0);
    }

  }

  FinishCheck();

}

void TidalRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --albums_requests_active_;
  ++albums_requests_received_;
  AlbumsReceived(reply, Artist(), limit_requested, offset_requested);

}

void TidalRequest::AddArtistAlbumsRequest(const Artist &artist, const int offset) {

  ArtistAlbumsRequest request;
  request.artist = artist;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);

  ++artist_albums_requests_total_;

  StartRequests();

}

void TidalRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    const ArtistAlbumsRequest request = artist_albums_requests_queue_.dequeue();

    ParamList parameters;
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("artists/%1/albums").arg(request.artist.artist_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });

    ++artist_albums_requests_active_;

  }

}

void TidalRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  Q_EMIT UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
  AlbumsReceived(reply, artist, 0, offset_requested);

}

void TidalRequest::AlbumsReceived(QNetworkReply *reply, const Artist &artist_requested, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int offset = 0;
  int albums_total = 0;
  int albums_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist_requested, limit_requested, &offset, &albums_total, &albums_received]() { AlbumsFinishCheck(artist_requested, limit_requested, offset, albums_total, albums_received); });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    Error(json_object_result.error_message);
    return;
  }

  if (!json_object.contains("limit"_L1) ||
      !json_object.contains("offset"_L1) ||
      !json_object.contains("totalNumberOfItems"_L1) ||
      !json_object.contains("items"_L1)) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  offset = json_object["offset"_L1].toInt();
  albums_total = json_object["totalNumberOfItems"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  const JsonArrayResult json_array_result = GetJsonArray(json_object, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {
    return;
  }

  for (const QJsonValue &value_item : array_items) {

    ++albums_received;

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item in array is not a object."_s);
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1)) {
      const QJsonValue json_item = object_item["item"_L1];
      if (!json_item.isObject()) {
        Error(u"Invalid Json reply, item in array is not a object."_s, json_item);
        continue;
      }
      object_item = json_item.toObject();
    }

    Album album;
    if (object_item.contains("type"_L1)) {  // This was an albums request or search
      if (!object_item.contains("id"_L1) || !object_item.contains("title"_L1)) {
        Error(u"Invalid Json reply, item is missing ID or title."_s, object_item);
        continue;
      }
      if (object_item["id"_L1].isString()) {
        album.album_id = object_item["id"_L1].toString();
      }
      else {
        album.album_id = QString::number(object_item["id"_L1].toInt());
      }
      album.album = object_item["title"_L1].toString();
      if (service_->album_explicit() && object_item.contains("explicit"_L1)) {
        album.album_explicit = object_item["explicit"_L1].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)"_L1);
        }
      }
    }
    else if (object_item.contains("album"_L1)) {  // This was a tracks request or search
      const QJsonValue value_album = object_item["album"_L1];
      if (!value_album.isObject()) {
        Error(u"Invalid Json reply, item album is not a object."_s, value_album);
        continue;
      }
      const QJsonObject object_album = value_album.toObject();
      if (!object_album.contains("id"_L1) || !object_album.contains("title"_L1)) {
        Error(u"Invalid Json reply, item album is missing ID or title."_s, object_album);
        continue;
      }
      if (object_album["id"_L1].isString()) {
        album.album_id = object_album["id"_L1].toString();
      }
      else {
        album.album_id = QString::number(object_album["id"_L1].toInt());
      }
      album.album = object_album["title"_L1].toString();
      if (service_->album_explicit() && object_album.contains("explicit"_L1)) {
        album.album_explicit = object_album["explicit"_L1].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)"_L1);
        }
      }
    }
    else {
      Error(u"Invalid Json reply, item missing type or album."_s, object_item);
      continue;
    }

    if (album_songs_requests_pending_.contains(album.album_id)) continue;

    if (!object_item.contains("artist"_L1) || !object_item.contains("title"_L1) || !object_item.contains("audioQuality"_L1)) {
      Error(u"Invalid Json reply, item missing artist, title or audioQuality."_s, object_item);
      continue;
    }
    const QJsonValue value_artist = object_item["artist"_L1];
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, item artist is not a object."_s, value_artist);
      continue;
    }
    const QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, item artist missing id or name."_s, obj_artist);
      continue;
    }

    Artist album_artist;
    if (obj_artist["id"_L1].isString()) {
      album_artist.artist_id = obj_artist["id"_L1].toString();
    }
    else {
      album_artist.artist_id = QString::number(obj_artist["id"_L1].toInt());
    }
    album_artist.artist = obj_artist["name"_L1].toString();

    //QString quality = obj_item["audioQuality"].toString();
    //QString copyright = obj_item["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    AlbumSongsRequest request;
    request.artist = album_artist;
    request.album = album;
    album_songs_requests_pending_.insert(album.album_id, request);

  }

  if (query_type_ == Type::FavouriteAlbums || query_type_ == Type::SearchAlbums) {
    albums_received_ += albums_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(albums_received_, albums_total_));
  }

}

void TidalRequest::AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (finished_) return;

  if (limit == 0 || limit > albums_received) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      switch (query_type_) {
        case Type::FavouriteAlbums:
          AddAlbumsRequest(offset_next);
          break;
        case Type::SearchAlbums:
          AddAlbumsSearchRequest(offset_next);
          break;
        case Type::FavouriteArtists:
        case Type::SearchArtists:
          AddArtistAlbumsRequest(artist, offset_next);
          break;
        default:
          break;
      }
    }
  }

  if (
      artists_requests_queue_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_queue_.isEmpty() &&
      albums_requests_active_ <= 0 &&
      artist_albums_requests_queue_.isEmpty() &&
      artist_albums_requests_active_ <= 0
      ) { // Artist albums query is finished, get all songs for all albums.

    // Get songs for all the albums.

    for (QHash<QString, AlbumSongsRequest>::const_iterator it = album_songs_requests_pending_.constBegin(); it != album_songs_requests_pending_.constEnd(); ++it) {
      const AlbumSongsRequest &request = it.value();
      AddAlbumSongsRequest(request.artist, request.album);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requests_total_ > 0) {
      if (album_songs_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving songs for %1 album...").arg(album_songs_requests_total_));
      else Q_EMIT UpdateStatus(query_id_, tr("Receiving songs for %1 albums...").arg(album_songs_requests_total_));
      Q_EMIT UpdateProgress(query_id_, 0);
    }
  }

  GetAlbumCoversCheck();
  FinishCheck();

}

void TidalRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  ++songs_requests_received_;
  if (query_type_ == Type::SearchSongs && fetchalbums_) {
    AlbumsReceived(reply, Artist(), limit_requested, offset_requested);
  }
  else {
    SongsReceived(reply, Artist(), Album(), limit_requested, offset_requested);
  }

}

void TidalRequest::AddAlbumSongsRequest(const Artist &artist, const Album &album, const int offset) {

  AlbumSongsRequest request;
  request.artist = artist;
  request.album = album;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);

  ++album_songs_requests_total_;

  StartRequests();

}

void TidalRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {

    AlbumSongsRequest request = album_songs_requests_queue_.dequeue();
    ParamList parameters;
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("albums/%1/tracks").arg(request.album.album_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist, request.album, request.offset); });

    ++album_songs_requests_active_;

  }

}

void TidalRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int offset_requested) {

  --album_songs_requests_active_;
  ++album_songs_requests_received_;
  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(album_songs_requests_received_, album_songs_requests_total_));
  }
  SongsReceived(reply, artist, album, 0, offset_requested);

}

void TidalRequest::SongsReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int songs_total = 0;
  int songs_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist, album, limit_requested, offset_requested, &songs_total, &songs_received]() { SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, songs_received); });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("limit"_L1) ||
      !json_object.contains("offset"_L1) ||
      !json_object.contains("totalNumberOfItems"_L1) ||
      !json_object.contains("items"_L1)) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  const int offset = json_object["offset"_L1].toInt();
  songs_total = json_object["totalNumberOfItems"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  const JsonArrayResult json_array_result = GetJsonArray(json_object, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, track is not a object."_s);
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1)) {
      const QJsonValue item = object_item["item"_L1];
      if (!item.isObject()) {
        Error(u"Invalid Json reply, item is not a object."_s, item);
        continue;
      }
      object_item = item.toObject();
    }

    ++songs_received;
    Song song(Song::Source::Tidal);
    ParseSong(song, object_item, artist, album);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
    songs << song;
  }

  for (Song song : std::as_const(songs)) {
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) song.set_disc(0);
    songs_.insert(song.song_id(), song);
  }

  if (query_type_ == Type::FavouriteSongs || query_type_ == Type::SearchSongs) {
    songs_received_ += songs_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(songs_received_, songs_total_));
  }

}

void TidalRequest::SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (query_type_) {
        case Type::FavouriteSongs:
          AddSongsRequest(offset_next);
          break;
        case Type::SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So fallthrough.
          if (artist.artist_id.isEmpty() && album.album_id.isEmpty()) {
            AddSongsSearchRequest(offset_next);
            break;
          }
          [[fallthrough]];
        case Type::FavouriteArtists:
        case Type::SearchArtists:
        case Type::FavouriteAlbums:
        case Type::SearchAlbums:
          AddAlbumSongsRequest(artist, album, offset_next);
          break;
        default:
          break;
      }
    }
  }

  GetAlbumCoversCheck();
  FinishCheck();

}

void TidalRequest::ParseSong(Song &song, const QJsonObject &json_obj, const Artist &album_artist, const Album &album) {

  if (
      !json_obj.contains("album"_L1) ||
      !json_obj.contains("allowStreaming"_L1) ||
      !json_obj.contains("artist"_L1) ||
      !json_obj.contains("artists"_L1) ||
      !json_obj.contains("audioQuality"_L1) ||
      !json_obj.contains("duration"_L1) ||
      !json_obj.contains("id"_L1) ||
      !json_obj.contains("streamReady"_L1) ||
      !json_obj.contains("title"_L1) ||
      !json_obj.contains("trackNumber"_L1) ||
      !json_obj.contains("url"_L1) ||
      !json_obj.contains("volumeNumber"_L1) ||
      !json_obj.contains("copyright"_L1)
    ) {
    Error(u"Invalid Json reply, track is missing one or more values."_s, json_obj);
    return;
  }

  const QJsonValue value_artist = json_obj["artist"_L1];
  const QJsonValue value_album = json_obj["album"_L1];
  const QJsonValue json_duration = json_obj["duration"_L1];
  //const QJsonArray array_artists = json_obj["artists"].toArray();

  QString song_id;
  if (json_obj["id"_L1].isString()) {
    song_id = json_obj["id"_L1].toString();
  }
  else {
    song_id = QString::number(json_obj["id"_L1].toInt());
  }

  QString title = json_obj["title"_L1].toString();
  //const QString urlstr = json_obj["url"].toString();
  const int track = json_obj["trackNumber"_L1].toInt();
  const int disc = json_obj["volumeNumber"_L1].toInt();
  const bool allow_streaming = json_obj["allowStreaming"_L1].toBool();
  const bool stream_ready = json_obj["streamReady"_L1].toBool();
  const QString copyright = json_obj["copyright"_L1].toString();

  if (!value_artist.isObject()) {
    Error(u"Invalid Json reply, track artist is not a object."_s, value_artist);
    return;
  }
  const QJsonObject object_artist = value_artist.toObject();
  if (!object_artist.contains("id"_L1) || !object_artist.contains("name"_L1)) {
    Error(u"Invalid Json reply, track artist is missing id or name."_s, object_artist);
    return;
  }
  QString artist_id;
  if (object_artist["id"_L1].isString()) {
    artist_id = object_artist["id"_L1].toString();
  }
  else {
    artist_id = QString::number(object_artist["id"_L1].toInt());
  }
  QString artist = object_artist["name"_L1].toString();

  if (!value_album.isObject()) {
    Error(u"Invalid Json reply, track album is not a object."_s, value_album);
    return;
  }
  const QJsonObject object_album = value_album.toObject();
  if (!object_album.contains("id"_L1) || !object_album.contains("title"_L1)) {
    Error(u"Invalid Json reply, track album is missing ID or title."_s, object_album);
    return;
  }
  QString album_id;
  if (object_album["id"_L1].isString()) {
    album_id = object_album["id"_L1].toString();
  }
  else {
    album_id = QString::number(object_album["id"_L1].toInt());
  }
  if (!album.album_id.isEmpty() && album.album_id != album_id) {
    Error(u"Invalid Json reply, track album id is wrong."_s, object_album);
    return;
  }
  QString album_title = object_album["title"_L1].toString();
  if (album.album_explicit) album_title.append(" (Explicit)"_L1);

  if (!allow_streaming) {
    Warn(QStringLiteral("Song %1 %2 %3 is not allowStreaming").arg(artist, album_title, title));
    return;
  }

  if (!stream_ready) {
    Warn(QStringLiteral("Song %1 %2 %3 is not streamReady").arg(artist, album_title, title));
    return;
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  QVariant q_duration = json_duration.toVariant();
  qint64 duration = 0;
  if (q_duration.isValid()) {
    duration = q_duration.toLongLong() * kNsecPerSec;
  }
  else {
    Error(u"Invalid duration for song."_s, json_duration);
    return;
  }

  QUrl cover_url;
  if (object_album.contains("cover"_L1)) {
    const QString cover = object_album["cover"_L1].toString().replace(u'-', u'/');
    if (!cover.isEmpty()) {
      cover_url.setUrl(QStringLiteral("%1/images/%2/%3.jpg").arg(QLatin1String(kResourcesUrl), cover, coversize_));
    }
  }

  title = Song::TitleRemoveMisc(title);

  //qLog(Debug) << "id" << song_id << "track" << track << "disc" << disc << "title" << title << "album" << album << "album artist" << album_artist << "artist" << artist << cover << allow_streaming << url;

  song.set_source(Song::Source::Tidal);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  if (album_artist.artist != artist) song.set_albumartist(album_artist.artist);
  song.set_album(album_title);
  song.set_artist(artist);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);
  song.set_url(url);
  song.set_length_nanosec(duration);
  if (cover_url.isValid()) {
    song.set_art_automatic(cover_url);
  }
  song.set_comment(copyright);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType::Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

}

void TidalRequest::GetAlbumCoversCheck() {

  if (
      !finished_ &&
      service_->download_album_covers() &&
      IsQuery() &&
      artists_requests_queue_.isEmpty() &&
      albums_requests_queue_.isEmpty() &&
      songs_requests_queue_.isEmpty() &&
      artist_albums_requests_queue_.isEmpty() &&
      album_songs_requests_queue_.isEmpty() &&
      album_cover_requests_queue_.isEmpty() &&
      artist_albums_requests_pending_.isEmpty() &&
      album_songs_requests_pending_.isEmpty() &&
      album_covers_requests_sent_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_active_ <= 0 &&
      songs_requests_active_ <= 0 &&
      artist_albums_requests_active_ <= 0 &&
      album_songs_requests_active_ <= 0 &&
      album_covers_requests_active_ <= 0
  ) {
    GetAlbumCovers();
  }

}

void TidalRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    AddAlbumCoverRequest(song);
  }

  if (album_covers_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving album cover for %1 album...").arg(album_covers_requests_total_));
  else Q_EMIT UpdateStatus(query_id_, tr("Receiving album covers for %1 albums...").arg(album_covers_requests_total_));
  Q_EMIT UpdateProgress(query_id_, 0);

  StartRequests();

}

void TidalRequest::AddAlbumCoverRequest(const Song &song) {

  if (album_covers_requests_sent_.contains(song.album_id())) {
    album_covers_requests_sent_.insert(song.album_id(), song.song_id());
    return;
  }

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = song.art_automatic();
  request.filename = CoverUtils::CoverFilePath(CoverOptions(), song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), request.url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(song.album_id(), song.song_id());
  ++album_covers_requests_total_;

  album_cover_requests_queue_.enqueue(request);

}

void TidalRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    QNetworkReply *reply = CreateGetRequest(request.url);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.album_id, request.url, request.filename); });

    ++album_covers_requests_active_;

  }

}

void TidalRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
  }
  else {
    AlbumCoverFinishCheck();
    return;
  }

  --album_covers_requests_active_;
  ++album_covers_requests_received_;

  if (finished_) return;

  const QScopeGuard finish_check = qScopeGuard([this]() { AlbumCoverFinishCheck(); });

  Q_EMIT UpdateProgress(query_id_, GetProgress(album_covers_requests_received_, album_covers_requests_total_));

  if (!album_covers_requests_sent_.contains(album_id)) {
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QByteArrayList format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list[0].data();
  }

  QImage image;
  if (image.loadFromData(data, format)) {
    if (image.save(filename, format)) {
      while (album_covers_requests_sent_.contains(album_id)) {
        const QString song_id = album_covers_requests_sent_.take(album_id);
        if (songs_.contains(song_id)) {
          songs_[song_id].set_art_automatic(QUrl::fromLocalFile(filename));
        }
      }
    }
    else {
      Error(QStringLiteral("Error saving image data to %1").arg(filename));
      if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    }
  }
  else {
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    Error(QStringLiteral("Error decoding image data from %1").arg(url.toString()));
  }

}

void TidalRequest::AlbumCoverFinishCheck() {

  FinishCheck();

}

void TidalRequest::FinishCheck() {

  if (
      !finished_ &&
      artists_requests_queue_.isEmpty() &&
      albums_requests_queue_.isEmpty() &&
      songs_requests_queue_.isEmpty() &&
      artist_albums_requests_queue_.isEmpty() &&
      album_songs_requests_queue_.isEmpty() &&
      album_cover_requests_queue_.isEmpty() &&
      artist_albums_requests_pending_.isEmpty() &&
      album_songs_requests_pending_.isEmpty() &&
      album_covers_requests_sent_.isEmpty() &&
      artists_requests_active_ <= 0 &&
      albums_requests_active_ <= 0 &&
      songs_requests_active_ <= 0 &&
      artist_albums_requests_active_ <= 0 &&
      album_songs_requests_active_ <= 0 &&
      album_covers_requests_active_ <= 0
      ) {
    if (timer_flush_requests_->isActive()) {
      timer_flush_requests_->stop();
    }
    finished_ = true;
    if (songs_.isEmpty()) {
      if (error_.isEmpty()) {
        if (IsSearch()) {
          Q_EMIT Results(query_id_, SongMap(), tr("No match."));
        }
        else {
          Q_EMIT Results(query_id_);
        }
      }
      else {
        Q_EMIT Results(query_id_, SongMap(), error_);
      }
    }
    else {
      Q_EMIT Results(query_id_, songs_);
    }
  }

}

int TidalRequest::GetProgress(const int count, const int total) {

  return static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0F);

}

void TidalRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Tidal:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = error_message;

}

void TidalRequest::Warn(const QString &error_message, const QVariant &debug) {

  qLog(Warning) << "Tidal:" << error_message;
  if (debug.isValid()) qLog(Debug) << debug;

}
