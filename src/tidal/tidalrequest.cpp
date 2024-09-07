/*
 * Strawberry Music Player
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

#include <QObject>
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

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/application.h"
#include "utilities/timeconstants.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalbaserequest.h"
#include "tidalrequest.h"

using namespace Qt::StringLiterals;

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

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, Application *app, SharedPtr<NetworkAccessManager> network, Type query_type, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
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
      album_covers_requests_received_(0),
      need_login_(false) {

  timer_flush_requests_->setInterval(kFlushRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &TidalRequest::FlushRequests);

}

TidalRequest::~TidalRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  while (!album_cover_replies_.isEmpty()) {
    QNetworkReply *reply = album_cover_replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void TidalRequest::LoginComplete(const bool success, const QString &error) {

  if (!need_login_) return;
  need_login_ = false;

  if (!success) {
    Error(error);
    return;
  }

  Process();

}

void TidalRequest::Process() {

  if (!service_->authenticated()) {
    Q_EMIT UpdateStatus(query_id_, tr("Authenticating..."));
    need_login_ = true;
    service_->TryLogin();
    return;
  }

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
      Error(QStringLiteral("Invalid query type."));
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

    Request request = artists_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchArtists) parameters << Param(QStringLiteral("query"), search_text_);
    if (request.limit > 0) parameters << Param(QStringLiteral("limit"), QString::number(request.limit));
    if (request.offset > 0) parameters << Param(QStringLiteral("offset"), QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteArtists) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/artists").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchArtists) {
      reply = CreateRequest(QStringLiteral("search/artists"), parameters);
    }
    if (!reply) continue;
    replies_ << reply;
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

    Request request = albums_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchAlbums) parameters << Param(QStringLiteral("query"), search_text_);
    if (request.limit > 0) parameters << Param(QStringLiteral("limit"), QString::number(request.limit));
    if (request.offset > 0) parameters << Param(QStringLiteral("offset"), QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteAlbums) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/albums").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchAlbums) {
      reply = CreateRequest(QStringLiteral("search/albums"), parameters);
    }
    if (!reply) continue;
    replies_ << reply;
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

    Request request = songs_requests_queue_.dequeue();

    ParamList parameters;
    if (query_type_ == Type::SearchSongs) parameters << Param(QStringLiteral("query"), search_text_);
    if (request.limit > 0) parameters << Param(QStringLiteral("limit"), QString::number(request.limit));
    if (request.offset > 0) parameters << Param(QStringLiteral("offset"), QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == Type::FavouriteSongs) {
      reply = CreateRequest(QStringLiteral("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
    }
    if (query_type_ == Type::SearchSongs) {
      reply = CreateRequest(QStringLiteral("search/tracks"), parameters);
    }
    if (!reply) continue;
    replies_ << reply;
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

  QByteArray data = GetReplyData(reply, (offset_requested == 0));

  --artists_requests_active_;
  ++artists_requests_received_;

  if (finished_) return;

  if (data.isEmpty()) {
    ArtistsFinishCheck();
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    ArtistsFinishCheck();
    return;
  }

  if (!json_obj.contains("limit"_L1) ||
      !json_obj.contains("offset"_L1) ||
      !json_obj.contains("totalNumberOfItems"_L1) ||
      !json_obj.contains("items"_L1)) {
    Error(QStringLiteral("Json object missing values."), json_obj);
    ArtistsFinishCheck();
    return;
  }
  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"_L1].toInt();
  int artists_total = json_obj["totalNumberOfItems"_L1].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QStringLiteral("totalNumberOfItems returned does not match previous totalNumberOfItems! %1 != %2").arg(artists_total).arg(artists_total_));
    ArtistsFinishCheck();
    return;
  }

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    ArtistsFinishCheck();
    return;
  }

  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));
  }

  QJsonValue value_items = ExtractItems(json_obj);
  if (!value_items.isArray()) {
    ArtistsFinishCheck();
    return;
  }

  const QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {  // Empty array means no results
    ArtistsFinishCheck();
    return;
  }

  int artists_received = 0;
  for (const QJsonValue &value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error(QStringLiteral("Invalid Json reply, item in array is not a object."));
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item"_L1)) {
      QJsonValue json_item = obj_item["item"_L1];
      if (!json_item.isObject()) {
        Error(QStringLiteral("Invalid Json reply, item in array is not a object."), json_item);
        continue;
      }
      obj_item = json_item.toObject();
    }

    if (!obj_item.contains("id"_L1) || !obj_item.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, item missing id or album."), obj_item);
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

  ArtistsFinishCheck(limit_requested, offset, artists_received);

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
  AlbumsReceived(reply, Artist(), limit_requested, offset_requested, offset_requested == 0);

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
    if (request.offset > 0) parameters << Param(QStringLiteral("offset"), QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("artists/%1/albums").arg(request.artist.artist_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });
    replies_ << reply;

    ++artist_albums_requests_active_;

  }

}

void TidalRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  Q_EMIT UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
  AlbumsReceived(reply, artist, 0, offset_requested, false);

}

void TidalRequest::AlbumsReceived(QNetworkReply *reply, const Artist &artist_requested, const int limit_requested, const int offset_requested, const bool auto_login) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply, auto_login);

  if (finished_) return;

  if (data.isEmpty()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }

  if (!json_obj.contains("limit"_L1) ||
      !json_obj.contains("offset"_L1) ||
      !json_obj.contains("totalNumberOfItems"_L1) ||
      !json_obj.contains("items"_L1)) {
    Error(QStringLiteral("Json object missing values."), json_obj);
    AlbumsFinishCheck(artist_requested);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"_L1].toInt();
  int albums_total = json_obj["totalNumberOfItems"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    AlbumsFinishCheck(artist_requested);
    return;
  }

  QJsonValue value_items = ExtractItems(json_obj);
  if (!value_items.isArray()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }
  const QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value_item : array_items) {

    ++albums_received;

    if (!value_item.isObject()) {
      Error(QStringLiteral("Invalid Json reply, item in array is not a object."));
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item"_L1)) {
      QJsonValue json_item = obj_item["item"_L1];
      if (!json_item.isObject()) {
        Error(QStringLiteral("Invalid Json reply, item in array is not a object."), json_item);
        continue;
      }
      obj_item = json_item.toObject();
    }

    Album album;
    if (obj_item.contains("type"_L1)) {  // This was an albums request or search
      if (!obj_item.contains("id"_L1) || !obj_item.contains("title"_L1)) {
        Error(QStringLiteral("Invalid Json reply, item is missing ID or title."), obj_item);
        continue;
      }
      if (obj_item["id"_L1].isString()) {
        album.album_id = obj_item["id"_L1].toString();
      }
      else {
        album.album_id = QString::number(obj_item["id"_L1].toInt());
      }
      album.album = obj_item["title"_L1].toString();
      if (service_->album_explicit() && obj_item.contains("explicit"_L1)) {
        album.album_explicit = obj_item["explicit"_L1].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)"_L1);
        }
      }
    }
    else if (obj_item.contains("album"_L1)) {  // This was a tracks request or search
      QJsonValue value_album = obj_item["album"_L1];
      if (!value_album.isObject()) {
        Error(QStringLiteral("Invalid Json reply, item album is not a object."), value_album);
        continue;
      }
      QJsonObject obj_album = value_album.toObject();
      if (!obj_album.contains("id"_L1) || !obj_album.contains("title"_L1)) {
        Error(QStringLiteral("Invalid Json reply, item album is missing ID or title."), obj_album);
        continue;
      }
      if (obj_album["id"_L1].isString()) {
        album.album_id = obj_album["id"_L1].toString();
      }
      else {
        album.album_id = QString::number(obj_album["id"_L1].toInt());
      }
      album.album = obj_album["title"_L1].toString();
      if (service_->album_explicit() && obj_album.contains("explicit"_L1)) {
        album.album_explicit = obj_album["explicit"_L1].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)"_L1);
        }
      }
    }
    else {
      Error(QStringLiteral("Invalid Json reply, item missing type or album."), obj_item);
      continue;
    }

    if (album_songs_requests_pending_.contains(album.album_id)) continue;

    if (!obj_item.contains("artist"_L1) || !obj_item.contains("title"_L1) || !obj_item.contains("audioQuality"_L1)) {
      Error(QStringLiteral("Invalid Json reply, item missing artist, title or audioQuality."), obj_item);
      continue;
    }
    QJsonValue value_artist = obj_item["artist"_L1];
    if (!value_artist.isObject()) {
      Error(QStringLiteral("Invalid Json reply, item artist is not a object."), value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, item artist missing id or name."), obj_artist);
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

  AlbumsFinishCheck(artist_requested, limit_requested, offset, albums_total, albums_received);

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
    AlbumsReceived(reply, Artist(), limit_requested, offset_requested, offset_requested == 0);
  }
  else {
    SongsReceived(reply, Artist(), Album(), limit_requested, offset_requested, offset_requested == 0);
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
    if (request.offset > 0) parameters << Param(QStringLiteral("offset"), QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("albums/%1/tracks").arg(request.album.album_id), parameters);
    replies_ << reply;
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
  SongsReceived(reply, artist, album, 0, offset_requested, false);

}

void TidalRequest::SongsReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int limit_requested, const int offset_requested, const bool auto_login) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply, auto_login);

  if (finished_) return;

  if (data.isEmpty()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested);
    return;
  }

  if (!json_obj.contains("limit"_L1) ||
      !json_obj.contains("offset"_L1) ||
      !json_obj.contains("totalNumberOfItems"_L1) ||
      !json_obj.contains("items"_L1)) {
    Error(QStringLiteral("Json object missing values."), json_obj);
    SongsFinishCheck(artist, album, limit_requested, offset_requested);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"_L1].toInt();
  int songs_total = json_obj["totalNumberOfItems"_L1].toInt();

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
    return;
  }

  QJsonValue json_value = ExtractItems(json_obj);
  if (!json_value.isArray()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
    return;
  }

  const QJsonArray array_items = json_value.toArray();
  if (array_items.isEmpty()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(QStringLiteral("Invalid Json reply, track is not a object."));
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item"_L1)) {
      QJsonValue item = obj_item["item"_L1];
      if (!item.isObject()) {
        Error(QStringLiteral("Invalid Json reply, item is not a object."), item);
        continue;
      }
      obj_item = item.toObject();
    }

    ++songs_received;
    Song song(Song::Source::Tidal);
    ParseSong(song, obj_item, artist, album);
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

  SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, songs_received);

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
    Error(QStringLiteral("Invalid Json reply, track is missing one or more values."), json_obj);
    return;
  }

  QJsonValue value_artist = json_obj["artist"_L1];
  QJsonValue value_album = json_obj["album"_L1];
  QJsonValue json_duration = json_obj["duration"_L1];
  //QJsonArray array_artists = json_obj["artists"].toArray();

  QString song_id;
  if (json_obj["id"_L1].isString()) {
    song_id = json_obj["id"_L1].toString();
  }
  else {
    song_id = QString::number(json_obj["id"_L1].toInt());
  }

  QString title = json_obj["title"_L1].toString();
  //QString urlstr = json_obj["url"].toString();
  int track = json_obj["trackNumber"_L1].toInt();
  int disc = json_obj["volumeNumber"_L1].toInt();
  bool allow_streaming = json_obj["allowStreaming"_L1].toBool();
  bool stream_ready = json_obj["streamReady"_L1].toBool();
  QString copyright = json_obj["copyright"_L1].toString();

  if (!value_artist.isObject()) {
    Error(QStringLiteral("Invalid Json reply, track artist is not a object."), value_artist);
    return;
  }
  QJsonObject obj_artist = value_artist.toObject();
  if (!obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
    Error(QStringLiteral("Invalid Json reply, track artist is missing id or name."), obj_artist);
    return;
  }
  QString artist_id;
  if (obj_artist["id"_L1].isString()) {
    artist_id = obj_artist["id"_L1].toString();
  }
  else {
    artist_id = QString::number(obj_artist["id"_L1].toInt());
  }
  QString artist = obj_artist["name"_L1].toString();

  if (!value_album.isObject()) {
    Error(QStringLiteral("Invalid Json reply, track album is not a object."), value_album);
    return;
  }
  QJsonObject obj_album = value_album.toObject();
  if (!obj_album.contains("id"_L1) || !obj_album.contains("title"_L1)) {
    Error(QStringLiteral("Invalid Json reply, track album is missing ID or title."), obj_album);
    return;
  }
  QString album_id;
  if (obj_album["id"_L1].isString()) {
    album_id = obj_album["id"_L1].toString();
  }
  else {
    album_id = QString::number(obj_album["id"_L1].toInt());
  }
  if (!album.album_id.isEmpty() && album.album_id != album_id) {
    Error(QStringLiteral("Invalid Json reply, track album id is wrong."), obj_album);
    return;
  }
  QString album_title = obj_album["title"_L1].toString();
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
    Error(QStringLiteral("Invalid duration for song."), json_duration);
    return;
  }

  QUrl cover_url;
  if (obj_album.contains("cover"_L1)) {
    const QString cover = obj_album["cover"_L1].toString().replace(u'-', u'/');
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

    QNetworkRequest req(request.url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.album_id, request.url, request.filename); });

    ++album_covers_requests_active_;

  }

}

void TidalRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

  if (album_cover_replies_.contains(reply)) {
    album_cover_replies_.removeAll(reply);
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

  Q_EMIT UpdateProgress(query_id_, GetProgress(album_covers_requests_received_, album_covers_requests_total_));

  if (!album_covers_requests_sent_.contains(album_id)) {
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
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

  AlbumCoverFinishCheck();

}

void TidalRequest::AlbumCoverFinishCheck() {

  FinishCheck();

}

void TidalRequest::FinishCheck() {

  if (
      !finished_ &&
      !need_login_ &&
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
      if (errors_.isEmpty()) {
        if (IsSearch()) {
          Q_EMIT Results(query_id_, SongMap(), tr("No match."));
        }
        else {
          Q_EMIT Results(query_id_);
        }
      }
      else {
        Q_EMIT Results(query_id_, SongMap(), ErrorsToHTML(errors_));
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

void TidalRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    errors_ << error;
    qLog(Error) << "Tidal:" << error;
  }

  if (debug.isValid()) qLog(Debug) << debug;

  FinishCheck();

}

void TidalRequest::Warn(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
