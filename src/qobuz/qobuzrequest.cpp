/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/song.h"
#include "core/networkaccessmanager.h"
#include "constants/timeconstants.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "qobuzservice.h"
#include "qobuzurlhandler.h"
#include "qobuzbaserequest.h"
#include "qobuzrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMaxConcurrentArtistsRequests = 3;
constexpr int kMaxConcurrentAlbumsRequests = 3;
constexpr int kMaxConcurrentSongsRequests = 3;
constexpr int kMaxConcurrentArtistAlbumsRequests = 3;
constexpr int kMaxConcurrentAlbumSongsRequests = 3;
constexpr int kMaxConcurrentAlbumCoverRequests = 1;
constexpr int kFlushRequestsDelay = 200;
}  // namespace

QobuzRequest::QobuzRequest(QobuzService *service, QobuzUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      url_handler_(url_handler),
      timer_flush_requests_(new QTimer(this)),
      query_type_(query_type),
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
      album_covers_requests_received_(0),
      no_results_(false) {

  timer_flush_requests_->setInterval(kFlushRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &QobuzRequest::FlushRequests);

}

void QobuzRequest::Process() {

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

void QobuzRequest::StartRequests() {

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void QobuzRequest::FlushRequests() {

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

void QobuzRequest::Search(const int query_id, const QString &search_text) {
  query_id_ = query_id;
  search_text_ = search_text;
}

void QobuzRequest::GetArtists() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving artists..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void QobuzRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);

  ++artists_requests_total_;

  StartRequests();

}

void QobuzRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    const Request request = artists_requests_queue_.dequeue();

    ParamList params;
    if (query_type_ == Type::FavouriteArtists) {
      params << Param(u"type"_s, u"artists"_s);
      params << Param(u"user_auth_token"_s, service_->user_auth_token());
    }
    else if (query_type_ == Type::SearchArtists) params << Param(u"query"_s, search_text_);
    if (request.limit > 0) params << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) params << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteArtists) {
      reply = CreateRequest(u"favorite/getUserFavorites"_s, params);
    }
    else if (query_type_ == Type::SearchArtists) {
      reply = CreateRequest(u"artist/search"_s, params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

    ++artists_requests_active_;

  }

}

void QobuzRequest::GetAlbums() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving albums..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void QobuzRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);

  ++albums_requests_total_;

  StartRequests();

}

void QobuzRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    const Request request = albums_requests_queue_.dequeue();

    ParamList params;
    if (query_type_ == Type::FavouriteAlbums) {
      params << Param(u"type"_s, u"albums"_s);
      params << Param(u"user_auth_token"_s, service_->user_auth_token());
    }
    else if (query_type_ == Type::SearchAlbums) params << Param(u"query"_s, search_text_);
    if (request.limit > 0) params << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) params << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteAlbums) {
      reply = CreateRequest(u"favorite/getUserFavorites"_s, params);
    }
    else if (query_type_ == Type::SearchAlbums) {
      reply = CreateRequest(u"album/search"_s, params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

    ++albums_requests_active_;

  }

}

void QobuzRequest::GetSongs() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving songs..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void QobuzRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);

  ++songs_requests_total_;

  StartRequests();

}

void QobuzRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    const Request request = songs_requests_queue_.dequeue();

    ParamList params;
    if (query_type_ == Type::FavouriteSongs) {
      params << Param(u"type"_s, u"tracks"_s);
      params << Param(u"user_auth_token"_s, service_->user_auth_token());
    }
    else if (query_type_ == Type::SearchSongs) params << Param(u"query"_s, search_text_);
    if (request.limit > 0) params << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) params << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteSongs) {
      reply = CreateRequest(u"favorite/getUserFavorites"_s, params);
    }
    else if (query_type_ == Type::SearchSongs) {
      reply = CreateRequest(u"track/search"_s, params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

    ++songs_requests_active_;

  }

}

void QobuzRequest::ArtistsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void QobuzRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void QobuzRequest::AlbumsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void QobuzRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void QobuzRequest::SongsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsSearchRequest();

}

void QobuzRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void QobuzRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

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
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("artists"_L1)) {
    Error(u"Json object is missing artists."_s, json_object);
    return;
  }
  const QJsonValue value_artists = json_object["artists"_L1];
  if (!value_artists.isObject()) {
    Error(u"Json artists is not an object."_s, json_object);
    return;
  }
  const QJsonObject object_artists = value_artists.toObject();

  if (!object_artists.contains("limit"_L1) ||
      !object_artists.contains("offset"_L1) ||
      !object_artists.contains("total"_L1) ||
      !object_artists.contains("items"_L1)) {
    Error(u"Json artists object is missing values."_s, json_object);
    return;
  }
  //int limit = obj_artists["limit"].toInt();
  offset = object_artists["offset"_L1].toInt();
  int artists_total = object_artists["total"_L1].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QStringLiteral("total returned does not match previous total! %1 != %2").arg(artists_total).arg(artists_total_));
    return;
  }

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));
  }

  const JsonArrayResult json_array_result = GetJsonArray(object_artists, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {  // Empty array means no results
    if (offset_requested == 0) no_results_ = true;
    return;
  }

  for (const QJsonValue &value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item not a object."_s);
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item"_L1)) {
      QJsonValue json_item = obj_item["item"_L1];
      if (!json_item.isObject()) {
        Error(u"Invalid Json reply, item not a object."_s, json_item);
        continue;
      }
      obj_item = json_item.toObject();
    }

    if (!obj_item.contains("id"_L1) || !obj_item.contains("name"_L1)) {
      Error(u"Invalid Json reply, item missing id or album."_s, obj_item);
      continue;
    }

    Artist artist;
    if (obj_item["id"_L1].isString()) {
      artist.artist_id = obj_item["id"_L1].toString();
    }
    else {
      artist.artist_id = QString::number(obj_item["id"_L1].toInt());
    }
    artist.artist = obj_item["name"_L1].toString();

    if (artist_albums_requests_pending_.contains(artist.artist_id)) continue;

    ArtistAlbumsRequest request;
    request.artist = artist;
    artist_albums_requests_pending_.insert(artist.artist_id, request);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));

}

void QobuzRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

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

void QobuzRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --albums_requests_active_;
  ++albums_requests_received_;
  AlbumsReceived(reply, Artist(), limit_requested, offset_requested);

}

void QobuzRequest::AddArtistAlbumsRequest(const Artist &artist, const int offset) {

  ArtistAlbumsRequest request;
  request.artist = artist;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);

  ++artist_albums_requests_total_;

  StartRequests();

}

void QobuzRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    const ArtistAlbumsRequest request = artist_albums_requests_queue_.dequeue();

    ParamList params = ParamList() << Param(u"artist_id"_s, request.artist.artist_id)
                                   << Param(u"extra"_s, u"albums"_s);

    if (request.offset > 0) params << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(u"artist/get"_s, params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });
    replies_ << reply;

    ++artist_albums_requests_active_;

  }

}

void QobuzRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  Q_EMIT UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
  AlbumsReceived(reply, artist, 0, offset_requested);

}

void QobuzRequest::AlbumsReceived(QNetworkReply *reply, const Artist &artist_requested, const int limit_requested, const int offset_requested) {

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
    return;
  }

  Artist artist = artist_requested;

  if (json_object.contains("id"_L1) && json_object.contains("name"_L1)) {
    if (json_object["id"_L1].isString()) {
      artist.artist_id = json_object["id"_L1].toString();
    }
    else {
      artist.artist_id = QString::number(json_object["id"_L1].toInt());
    }
    artist.artist = json_object["name"_L1].toString();
  }

  if (artist.artist_id != artist_requested.artist_id) {
    Error(u"Artist ID returned does not match artist ID requested."_s, json_object);
    return;
  }

  if (!json_object.contains("albums"_L1)) {
    Error(u"Json object is missing albums."_s, json_object);
    return;
  }
  const QJsonValue value_albums = json_object["albums"_L1];
  if (!value_albums.isObject()) {
    Error(u"Json albums is not an object."_s, json_object);
    return;
  }
  const QJsonObject object_albums = value_albums.toObject();

  if (!object_albums.contains("limit"_L1) ||
      !object_albums.contains("offset"_L1) ||
      !object_albums.contains("total"_L1) ||
      !object_albums.contains("items"_L1)) {
    Error(u"Json albums object is missing values."_s, json_object);
    return;
  }

  //int limit = obj_albums["limit"].toInt();
  offset = object_albums["offset"_L1].toInt();
  albums_total = object_albums["total"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  const JsonArrayResult json_array_result = GetJsonArray(object_albums, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {
    if ((query_type_ == Type::FavouriteAlbums || query_type_ == Type::SearchAlbums) && offset_requested == 0) {
      no_results_ = true;
    }
    return;
  }

  for (const QJsonValue &value_item : array_items) {

    ++albums_received;

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item in array is not a object."_s);
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (!obj_item.contains("artist"_L1) || !obj_item.contains("title"_L1) || !obj_item.contains("id"_L1)) {
      Error(u"Invalid Json reply, item missing artist, title or id."_s, obj_item);
      continue;
    }

    Album album;
    if (obj_item["id"_L1].isString()) {
      album.album_id = obj_item["id"_L1].toString();
    }
    else {
      album.album_id = QString::number(obj_item["id"_L1].toInt());
    }
    album.album = obj_item["title"_L1].toString();

    if (album_songs_requests_pending_.contains(album.album_id)) continue;

    QJsonValue value_artist = obj_item["artist"_L1];
    if (!value_artist.isObject()) {
      Error(QStringLiteral("Invalid Json reply, item artist is not a object."), value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
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

    if (!artist_requested.artist_id.isEmpty() && album_artist.artist_id != artist_requested.artist_id) {
      qLog(Debug) << "Skipping artist" << album_artist.artist << album_artist.artist_id << "does not match album artist" << artist_requested.artist_id << artist_requested.artist;
      continue;
    }

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

void QobuzRequest::AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received) {

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

void QobuzRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  ++songs_requests_received_;
  SongsReceived(reply, Artist(), Album(), limit_requested, offset_requested);

}

void QobuzRequest::AddAlbumSongsRequest(const Artist &artist, const Album &album, const int offset) {

  AlbumSongsRequest request;
  request.artist = artist;
  request.album = album;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);

  ++album_songs_requests_total_;

  StartRequests();

}

void QobuzRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {

    const AlbumSongsRequest request = album_songs_requests_queue_.dequeue();
    ParamList params = ParamList() << Param(u"album_id"_s, request.album.album_id);
    if (request.offset > 0) params << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(u"album/get"_s, params);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist, request.album, request.offset); });

    ++album_songs_requests_active_;

  }

}

void QobuzRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int offset_requested) {

  --album_songs_requests_active_;
  ++album_songs_requests_received_;
  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(album_songs_requests_received_, album_songs_requests_total_));
  }
  SongsReceived(reply, artist, album, 0, offset_requested);

}

void QobuzRequest::SongsReceived(QNetworkReply *reply, const Artist &artist_requested, const Album &album_requested, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  Artist album_artist;
  Album album;
  int songs_total = 0;
  int songs_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, &album_artist, &album, limit_requested, offset_requested, &songs_total, &songs_received]() { SongsFinishCheck(album_artist, album, limit_requested, offset_requested, songs_total, songs_received); });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("tracks"_L1)) {
    Error(u"Json object is missing tracks."_s, json_object);
    return;
  }

  album_artist = artist_requested;
  album = album_requested;

  if (json_object.contains("id"_L1) && json_object.contains("title"_L1)) {
    if (json_object["id"_L1].isString()) {
      album.album_id = json_object["id"_L1].toString();
    }
    else {
      album.album_id = QString::number(json_object["id"_L1].toInt());
    }
    album.album = json_object["title"_L1].toString();
  }

  if (json_object.contains("artist"_L1)) {
    QJsonValue value_artist = json_object["artist"_L1];
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, album artist is not a object."_s, value_artist);
      return;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, album artist is missing id or name."_s, obj_artist);
      return;
    }
    if (obj_artist["id"_L1].isString()) {
      album_artist.artist_id = obj_artist["id"_L1].toString();
    }
    else {
      album_artist.artist_id = QString::number(obj_artist["id"_L1].toInt());
    }
    album_artist.artist = obj_artist["name"_L1].toString();
  }

  if (json_object.contains("image"_L1)) {
    QJsonValue value_image = json_object["image"_L1];
    if (!value_image.isObject()) {
      Error(u"Invalid Json reply, album image is not a object."_s, value_image);
      return;
    }
    QJsonObject obj_image = value_image.toObject();
    if (!obj_image.contains("large"_L1)) {
      Error(u"Invalid Json reply, album image is missing large."_s, obj_image);
      return;
    }
    QString album_image = obj_image["large"_L1].toString();
    if (!album_image.isEmpty()) {
      album.cover_url = QUrl(album_image);
    }
  }

  QJsonValue value_tracks = json_object["tracks"_L1];
  if (!value_tracks.isObject()) {
    Error(u"Json tracks is not an object."_s, json_object);
    return;
  }
  QJsonObject obj_tracks = value_tracks.toObject();

  if (!obj_tracks.contains("limit"_L1) ||
      !obj_tracks.contains("offset"_L1) ||
      !obj_tracks.contains("total"_L1) ||
      !obj_tracks.contains("items"_L1)) {
    Error(u"Json songs object is missing values."_s, json_object);
    return;
  }

  //int limit = obj_tracks["limit"].toInt();
  const int offset = obj_tracks["offset"_L1].toInt();
  songs_total = obj_tracks["total"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  const JsonArrayResult json_array_result = GetJsonArray(obj_tracks, u"items"_s);
  if (!json_array_result.success()) {
    Error(json_array_result.error_message);
    return;
  }

  const QJsonArray &array_items = json_array_result.json_array;
  if (array_items.isEmpty()) {
    if ((query_type_ == Type::FavouriteSongs || query_type_ == Type::SearchSongs) && offset_requested == 0) {
      no_results_ = true;
    }
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
    const QJsonObject object_item = value_item.toObject();

    ++songs_received;
    Song song(Song::Source::Qobuz);
    ParseSong(song, object_item, album_artist, album);
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

void QobuzRequest::SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (query_type_) {
        case Type::FavouriteSongs:
          AddSongsRequest(offset_next);
          break;
        case Type::SearchSongs:
          AddSongsSearchRequest(offset_next);
          break;
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

void QobuzRequest::ParseSong(Song &song, const QJsonObject &json_obj, const Artist &album_artist, const Album &album) {

  if (
      !json_obj.contains("id"_L1) ||
      !json_obj.contains("title"_L1) ||
      !json_obj.contains("track_number"_L1) ||
      !json_obj.contains("duration"_L1) ||
      !json_obj.contains("copyright"_L1) ||
      !json_obj.contains("streamable"_L1)
    ) {
    Error(u"Invalid Json reply, track is missing one or more values."_s, json_obj);
    return;
  }

  QString song_id;
  if (json_obj["id"_L1].isString()) {
    song_id = json_obj["id"_L1].toString();
  }
  else {
    song_id = QString::number(json_obj["id"_L1].toInt());
  }

  QString title = json_obj["title"_L1].toString();
  int track = json_obj["track_number"_L1].toInt();
  int disc = 0;
  QString copyright = json_obj["copyright"_L1].toString();
  qint64 duration = json_obj["duration"_L1].toInt() * kNsecPerSec;
  //bool streamable = json_obj["streamable"].toBool();
  QString composer;
  QString performer;

  if (json_obj.contains("media_number"_L1)) {
    disc = json_obj["media_number"_L1].toInt();
  }

  Artist song_artist = album_artist;
  Album song_album = album;
  if (json_obj.contains("album"_L1)) {

    QJsonValue value_album = json_obj["album"_L1];
    if (!value_album.isObject()) {
      Error(u"Invalid Json reply, album is not an object."_s, value_album);
      return;
    }
    QJsonObject obj_album = value_album.toObject();

    if (obj_album.contains("id"_L1)) {
      if (obj_album["id"_L1].isString()) {
        song_album.album_id = obj_album["id"_L1].toString();
      }
      else {
        song_album.album_id = QString::number(obj_album["id"_L1].toInt());
      }
    }

    if (obj_album.contains("title"_L1)) {
      song_album.album = obj_album["title"_L1].toString();
    }

    if (obj_album.contains("artist"_L1)) {
      QJsonValue value_artist = obj_album["artist"_L1];
      if (!value_artist.isObject()) {
        Error(u"Invalid Json reply, album artist is not a object."_s, value_artist);
        return;
      }
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
        Error(u"Invalid Json reply, album artist is missing id or name."_s, obj_artist);
        return;
      }
      if (obj_artist["id"_L1].isString()) {
        song_artist.artist_id = obj_artist["id"_L1].toString();
      }
      else {
        song_artist.artist_id = QString::number(obj_artist["id"_L1].toInt());
      }
      song_artist.artist = obj_artist["name"_L1].toString();
    }

    if (obj_album.contains("image"_L1)) {
      QJsonValue value_image = obj_album["image"_L1];
      if (!value_image.isObject()) {
        Error(u"Invalid Json reply, album image is not a object."_s, value_image);
        return;
      }
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("large"_L1)) {
        Error(u"Invalid Json reply, album image is missing large."_s, obj_image);
        return;
      }
      QString album_image = obj_image["large"_L1].toString();
      if (!album_image.isEmpty()) {
        song_album.cover_url.setUrl(album_image);
      }
    }
  }

  if (json_obj.contains("composer"_L1)) {
    QJsonValue value_composer = json_obj["composer"_L1];
    if (!value_composer.isObject()) {
      Error(u"Invalid Json reply, track composer is not a object."_s, value_composer);
      return;
    }
    QJsonObject obj_composer = value_composer.toObject();
    if (!obj_composer.contains("id"_L1) || !obj_composer.contains("name"_L1)) {
      Error(u"Invalid Json reply, track composer is missing id or name."_s, obj_composer);
      return;
    }
    composer = obj_composer["name"_L1].toString();
  }

  if (json_obj.contains("performer"_L1)) {
    QJsonValue value_performer = json_obj["performer"_L1];
    if (!value_performer.isObject()) {
      Error(u"Invalid Json reply, track performer is not a object."_s, value_performer);
      return;
    }
    QJsonObject obj_performer = value_performer.toObject();
    if (!obj_performer.contains("id"_L1) || !obj_performer.contains("name"_L1)) {
      Error(u"Invalid Json reply, track performer is missing id or name."_s, obj_performer);
      return;
    }
    performer = obj_performer["name"_L1].toString();
  }

  //if (!streamable) {
  //Warn(QString("Song %1 %2 %3 is not streamable").arg(album_artist).arg(album).arg(title));
  //}

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  title = Song::TitleRemoveMisc(title);

  //qLog(Debug) << "id" << song_id << "track" << track << "title" << title << "album" << album << "album artist" << album_artist << cover_url << streamable << url;

  song.set_source(Song::Source::Qobuz);
  song.set_song_id(song_id);
  song.set_album_id(song_album.album_id);
  song.set_artist_id(song_artist.artist_id);
  song.set_album(song_album.album);
  song.set_artist(song_artist.artist);
  song.set_disc(disc);
  if (!album_artist.artist.isEmpty() && album_artist.artist != song_artist.artist) {
    song.set_albumartist(album_artist.artist);
  }
  song.set_title(title);
  song.set_track(track);
  song.set_url(url);
  song.set_length_nanosec(duration);
  song.set_art_automatic(song_album.cover_url);
  song.set_performer(performer);
  song.set_composer(composer);
  song.set_comment(copyright);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType::Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

}

void QobuzRequest::GetAlbumCoversCheck() {

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

void QobuzRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    AddAlbumCoverRequest(song);
  }

  if (album_covers_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving album cover for %1 album...").arg(album_covers_requests_total_));
  else Q_EMIT UpdateStatus(query_id_, tr("Receiving album covers for %1 albums...").arg(album_covers_requests_total_));
  Q_EMIT UpdateProgress(query_id_, 0);

  StartRequests();

}

void QobuzRequest::AddAlbumCoverRequest(const Song &song) {

  QUrl cover_url = song.art_automatic();
  if (!cover_url.isValid()) return;

  if (album_covers_requests_sent_.contains(cover_url)) {
    album_covers_requests_sent_.insert(cover_url, song.song_id());
    return;
  }

  AlbumCoverRequest request;
  request.url = cover_url;
  request.filename = CoverUtils::CoverFilePath(CoverOptions(), song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), cover_url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(cover_url, song.song_id());
  ++album_covers_requests_total_;

  album_cover_requests_queue_.enqueue(request);

}

void QobuzRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {
    const AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    QNetworkReply *reply = CreateGetRequest(request.url);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.url, request.filename); });
    ++album_covers_requests_active_;
  }

}

void QobuzRequest::AlbumCoverReceived(QNetworkReply *reply, const QUrl &cover_url, const QString &filename) {

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

  if (!album_covers_requests_sent_.contains(cover_url)) {
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
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
      while (album_covers_requests_sent_.contains(cover_url)) {
        const QString song_id = album_covers_requests_sent_.take(cover_url);
        if (songs_.contains(song_id)) {
          songs_[song_id].set_art_automatic(QUrl::fromLocalFile(filename));
        }
      }
    }
    else {
      Error(QStringLiteral("Error saving image data to %1").arg(filename));
      if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    }
  }
  else {
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    Error(QStringLiteral("Error decoding image data from %1").arg(cover_url.toString()));
  }

}

void QobuzRequest::AlbumCoverFinishCheck() {

  FinishCheck();

}

void QobuzRequest::FinishCheck() {

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
    if (no_results_ && songs_.isEmpty()) {
      if (IsSearch()) {
        Q_EMIT Results(query_id_, SongMap(), tr("No match."));
      }
      else {
        Q_EMIT Results(query_id_, SongMap(), QString());
      }
    }
    else {
      if (songs_.isEmpty() && error_.isEmpty()) {
        Q_EMIT Results(query_id_, songs_, tr("Unknown error"));
      }
      else {
        Q_EMIT Results(query_id_, songs_, error_);
      }
    }
  }

}

int QobuzRequest::GetProgress(const int count, const int total) {

  return static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0F);

}

void QobuzRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Qobuz:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = QStringLiteral("Qobuz: %1").arg(error_message);

}

void QobuzRequest::Warn(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Qobuz:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

}
