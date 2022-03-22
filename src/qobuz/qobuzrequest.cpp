/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QImageReader>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QtDebug>

#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/application.h"
#include "core/imageutils.h"
#include "covermanager/albumcoverloader.h"
#include "qobuzservice.h"
#include "qobuzurlhandler.h"
#include "qobuzbaserequest.h"
#include "qobuzrequest.h"

const int QobuzRequest::kMaxConcurrentArtistsRequests = 3;
const int QobuzRequest::kMaxConcurrentAlbumsRequests = 3;
const int QobuzRequest::kMaxConcurrentSongsRequests = 3;
const int QobuzRequest::kMaxConcurrentArtistAlbumsRequests = 3;
const int QobuzRequest::kMaxConcurrentAlbumSongsRequests = 3;
const int QobuzRequest::kMaxConcurrentAlbumCoverRequests = 1;

QobuzRequest::QobuzRequest(QobuzService *service, QobuzUrlHandler *url_handler, Application *app, NetworkAccessManager *network, QueryType type, QObject *parent)
    : QobuzBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
      network_(network),
      type_(type),
      query_id_(-1),
      finished_(false),
      artists_requests_active_(0),
      artists_total_(0),
      artists_received_(0),
      albums_requests_active_(0),
      songs_requests_active_(0),
      artist_albums_requests_active_(0),
      artist_albums_requested_(0),
      artist_albums_received_(0),
      album_songs_requests_active_(0),
      album_songs_requested_(0),
      album_songs_received_(0),
      album_covers_requests_active_(),
      album_covers_requested_(0),
      album_covers_received_(0),
      no_results_(false) {}

QobuzRequest::~QobuzRequest() {

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

void QobuzRequest::Process() {

  switch (type_) {
    case QueryType::QueryType_Artists:
      GetArtists();
      break;
    case QueryType::QueryType_Albums:
      GetAlbums();
      break;
    case QueryType::QueryType_Songs:
      GetSongs();
      break;
    case QueryType::QueryType_SearchArtists:
      ArtistsSearch();
      break;
    case QueryType::QueryType_SearchAlbums:
      AlbumsSearch();
      break;
    case QueryType::QueryType_SearchSongs:
      SongsSearch();
      break;
    default:
      Error("Invalid query type.");
      break;
  }

}

void QobuzRequest::Search(const int query_id, const QString &search_text) {
  query_id_ = query_id;
  search_text_ = search_text;
}

void QobuzRequest::GetArtists() {

  emit UpdateStatus(query_id_, tr("Retrieving artists..."));
  emit UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void QobuzRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);
  if (artists_requests_active_ < kMaxConcurrentArtistsRequests) FlushArtistsRequests();

}

void QobuzRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    Request request = artists_requests_queue_.dequeue();
    ++artists_requests_active_;

    ParamList params;
    if (type_ == QueryType_Artists) {
      params << Param("type", "artists");
      params << Param("user_auth_token", user_auth_token());
    }
    else if (type_ == QueryType_SearchArtists) params << Param("query", search_text_);
    if (request.limit > 0) params << Param("limit", QString::number(request.limit));
    if (request.offset > 0) params << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (type_ == QueryType_Artists) {
      reply = CreateRequest(QString("favorite/getUserFavorites"), params);
    }
    else if (type_ == QueryType_SearchArtists) {
      reply = CreateRequest("artist/search", params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

  }

}

void QobuzRequest::GetAlbums() {

  emit UpdateStatus(query_id_, tr("Retrieving albums..."));
  emit UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void QobuzRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);
  if (albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();

}

void QobuzRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    Request request = albums_requests_queue_.dequeue();
    ++albums_requests_active_;

    ParamList params;
    if (type_ == QueryType_Albums) {
      params << Param("type", "albums");
      params << Param("user_auth_token", user_auth_token());
    }
    else if (type_ == QueryType_SearchAlbums) params << Param("query", search_text_);
    if (request.limit > 0) params << Param("limit", QString::number(request.limit));
    if (request.offset > 0) params << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (type_ == QueryType_Albums) {
      reply = CreateRequest(QString("favorite/getUserFavorites"), params);
    }
    else if (type_ == QueryType_SearchAlbums) {
      reply = CreateRequest("album/search", params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

  }

}

void QobuzRequest::GetSongs() {

  emit UpdateStatus(query_id_, tr("Retrieving songs..."));
  emit UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void QobuzRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);
  if (songs_requests_active_ < kMaxConcurrentSongsRequests) FlushSongsRequests();

}

void QobuzRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    Request request = songs_requests_queue_.dequeue();
    ++songs_requests_active_;

    ParamList params;
    if (type_ == QueryType_Songs) {
      params << Param("type", "tracks");
      params << Param("user_auth_token", user_auth_token());
    }
    else if (type_ == QueryType_SearchSongs) params << Param("query", search_text_);
    if (request.limit > 0) params << Param("limit", QString::number(request.limit));
    if (request.offset > 0) params << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (type_ == QueryType_Songs) {
      reply = CreateRequest(QString("favorite/getUserFavorites"), params);
    }
    else if (type_ == QueryType_SearchSongs) {
      reply = CreateRequest("track/search", params);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

  }

}

void QobuzRequest::ArtistsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void QobuzRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void QobuzRequest::AlbumsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void QobuzRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void QobuzRequest::SongsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
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

  QByteArray data = GetReplyData(reply);

  --artists_requests_active_;

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

  if (!json_obj.contains("artists")) {
    ArtistsFinishCheck();
    Error("Json object is missing artists.", json_obj);
    return;
  }
  QJsonValue value_artists = json_obj["artists"];
  if (!value_artists.isObject()) {
    Error("Json artists is not an object.", json_obj);
    ArtistsFinishCheck();
    return;
  }
  QJsonObject obj_artists = value_artists.toObject();

  if (!obj_artists.contains("limit") ||
      !obj_artists.contains("offset") ||
      !obj_artists.contains("total") ||
      !obj_artists.contains("items")) {
    ArtistsFinishCheck();
    Error("Json artists object is missing values.", json_obj);
    return;
  }
  //int limit = obj_artists["limit"].toInt();
  int offset = obj_artists["offset"].toInt();
  int artists_total = obj_artists["total"].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QString("total returned does not match previous total! %1 != %2").arg(artists_total).arg(artists_total_));
    ArtistsFinishCheck();
    return;
  }

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    ArtistsFinishCheck();
    return;
  }

  if (offset_requested == 0) {
    emit ProgressSetMaximum(query_id_, artists_total_);
    emit UpdateProgress(query_id_, artists_received_);
  }

  QJsonValue value_items = ExtractItems(obj_artists);
  if (!value_items.isArray()) {
    ArtistsFinishCheck();
    return;
  }

  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {  // Empty array means no results
    if (offset_requested == 0) no_results_ = true;
    ArtistsFinishCheck();
    return;
  }

  int artists_received = 0;
  for (const QJsonValueRef value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error("Invalid Json reply, item not a object.");
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item")) {
      QJsonValue json_item = obj_item["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item not a object.", json_item);
        continue;
      }
      obj_item = json_item.toObject();
    }

    if (!obj_item.contains("id") || !obj_item.contains("name")) {
      Error("Invalid Json reply, item missing id or album.", obj_item);
      continue;
    }

    QString artist_id;
    if (obj_item["id"].isString()) {
      artist_id = obj_item["id"].toString();
    }
    else {
      artist_id = QString::number(obj_item["id"].toInt());
    }
    if (artist_albums_requests_pending_.contains(artist_id)) continue;
    artist_albums_requests_pending_.append(artist_id);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) emit UpdateProgress(query_id_, artists_received_);

  ArtistsFinishCheck(limit_requested, offset, artists_received);

}

void QobuzRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if (finished_) return;

  if ((limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (type_ == QueryType_Artists) AddArtistsRequest(offset_next);
      else if (type_ == QueryType_SearchArtists) AddArtistsSearchRequest(offset_next);
    }
  }

  if (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) FlushArtistsRequests();

  if (artists_requests_queue_.isEmpty() && artists_requests_active_ <= 0) {  // Artist query is finished, get all albums for all artists.

    // Get artist albums
    for (const QString &artist_id : artist_albums_requests_pending_) {
      AddArtistAlbumsRequest(artist_id);
      ++artist_albums_requested_;
    }
    artist_albums_requests_pending_.clear();

    if (artist_albums_requested_ > 0) {
      if (artist_albums_requested_ == 1) emit UpdateStatus(query_id_, tr("Retrieving albums for %1 artist...").arg(artist_albums_requested_));
      else emit UpdateStatus(query_id_, tr("Retrieving albums for %1 artists...").arg(artist_albums_requested_));
      emit ProgressSetMaximum(query_id_, artist_albums_requested_);
      emit UpdateProgress(query_id_, 0);
    }

  }

  FinishCheck();

}

void QobuzRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {
  --albums_requests_active_;
  AlbumsReceived(reply, QString(), limit_requested, offset_requested);
  if (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();
}

void QobuzRequest::AddArtistAlbumsRequest(const QString &artist_id, const int offset) {

  Request request;
  request.artist_id = artist_id;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);
  if (artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) FlushArtistAlbumsRequests();

}

void QobuzRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    Request request = artist_albums_requests_queue_.dequeue();
    ++artist_albums_requests_active_;

    ParamList params = ParamList() << Param("artist_id", request.artist_id)
                                   << Param("extra", "albums");

    if (request.offset > 0) params << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("artist/get"), params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist_id, request.offset); });
    replies_ << reply;

  }

}

void QobuzRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const QString &artist_id, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_received_;
  emit UpdateProgress(query_id_, artist_albums_received_);
  AlbumsReceived(reply, artist_id, 0, offset_requested);
  if (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) FlushArtistAlbumsRequests();

}

void QobuzRequest::AlbumsReceived(QNetworkReply *reply, const QString &artist_id_requested, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);

  if (finished_) return;

  if (data.isEmpty()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  QString album_artist_id = artist_id_requested;
  if (json_obj.contains("id")) {
    if (json_obj["id"].isString()) {
      album_artist_id = json_obj["id"].toString();
    }
    else {
      album_artist_id = QString::number(json_obj["id"].toInt());
    }
  }
  QString album_artist;
  if (json_obj.contains("name")) {
    album_artist = json_obj["name"].toString();
  }

  if (album_artist_id != artist_id_requested) {
    AlbumsFinishCheck(artist_id_requested);
    Error("Artist id returned does not match artist id requested.", json_obj);
    return;
  }

  if (!json_obj.contains("albums")) {
    AlbumsFinishCheck(artist_id_requested);
    Error("Json object is missing albums.", json_obj);
    return;
  }
  QJsonValue value_albums = json_obj["albums"];
  if (!value_albums.isObject()) {
    Error("Json albums is not an object.", json_obj);
    AlbumsFinishCheck(artist_id_requested);
    return;
  }
  QJsonObject obj_albums = value_albums.toObject();

  if (!obj_albums.contains("limit") ||
      !obj_albums.contains("offset") ||
      !obj_albums.contains("total") ||
      !obj_albums.contains("items")) {
    AlbumsFinishCheck(artist_id_requested);
    Error("Json albums object is missing values.", json_obj);
    return;
  }

  //int limit = obj_albums["limit"].toInt();
  int offset = obj_albums["offset"].toInt();
  int albums_total = obj_albums["total"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  QJsonValue value_items = ExtractItems(obj_albums);
  if (!value_items.isArray()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }
  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    if ((type_ == QueryType_Albums || type_ == QueryType_SearchAlbums) && offset_requested == 0) {
      no_results_ = true;
    }
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValueRef value : array_items) {

    ++albums_received;

    if (!value.isObject()) {
      Error("Invalid Json reply, item not a object.");
      continue;
    }
    QJsonObject obj_item = value.toObject();

    if (!obj_item.contains("artist") || !obj_item.contains("title") || !obj_item.contains("id")) {
      Error("Invalid Json reply, item missing artist, title or id.", obj_item);
      continue;
    }

    QString album_id;
    if (obj_item["id"].isString()) {
      album_id = obj_item["id"].toString();
    }
    else {
      album_id = QString::number(obj_item["id"].toInt());
    }

    if (album_songs_requests_pending_.contains(album_id)) continue;

    QString album = obj_item["title"].toString();

    QJsonValue value_artist = obj_item["artist"];
    if (!value_artist.isObject()) {
      Error("Invalid Json reply, item artist is not a object.", value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("id") || !obj_artist.contains("name")) {
      Error("Invalid Json reply, item artist missing id or name.", obj_artist);
      continue;
    }

    QString artist_id;
    if (obj_artist["id"].isString()) {
      artist_id = obj_artist["id"].toString();
    }
    else {
      artist_id = QString::number(obj_artist["id"].toInt());
    }

    QString artist = obj_artist["name"].toString();
    if (!artist_id_requested.isEmpty() && artist_id != artist_id_requested) {
      qLog(Debug) << "Skipping artist" << "artist" << artist << artist_id << "does not match album artist" << album_artist_id << album_artist;
      continue;
    }

    Request request;
    request.artist_id = artist_id;
    request.album_id = album_id;
    request.album_artist = artist;
    request.album = album;
    album_songs_requests_pending_.insert(album_id, request);

  }

  AlbumsFinishCheck(artist_id_requested, limit_requested, offset, albums_total, albums_received);

}

void QobuzRequest::AlbumsFinishCheck(const QString &artist_id, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (finished_) return;

  if (limit == 0 || limit > albums_received) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      switch (type_) {
        case QueryType_Albums:
          AddAlbumsRequest(offset_next);
          break;
        case QueryType_SearchAlbums:
          AddAlbumsSearchRequest(offset_next);
          break;
        case QueryType_Artists:
        case QueryType_SearchArtists:
          AddArtistAlbumsRequest(artist_id, offset_next);
          break;
        default:
          break;
      }
    }
  }

  if (
      albums_requests_queue_.isEmpty() &&
      albums_requests_active_ <= 0 &&
      artist_albums_requests_queue_.isEmpty() &&
      artist_albums_requests_active_ <= 0
      ) { // Artist albums query is finished, get all songs for all albums.

    // Get songs for all the albums.

    QHash<QString, Request>::iterator it;
    for (it = album_songs_requests_pending_.begin(); it != album_songs_requests_pending_.end(); ++it) {
      Request request = it.value();
      AddAlbumSongsRequest(request.artist_id, request.album_id, request.album_artist, request.album);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requested_ > 0) {
      if (album_songs_requested_ == 1) emit UpdateStatus(query_id_, tr("Retrieving songs for %1 album...").arg(album_songs_requested_));
      else emit UpdateStatus(query_id_, tr("Retrieving songs for %1 albums...").arg(album_songs_requested_));
      emit ProgressSetMaximum(query_id_, album_songs_requested_);
      emit UpdateProgress(query_id_, 0);
    }
  }

  FinishCheck();

}

void QobuzRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  SongsReceived(reply, QString(), QString(), limit_requested, offset_requested);

}

void QobuzRequest::AddAlbumSongsRequest(const QString &artist_id, const QString &album_id, const QString &album_artist, const QString &album, const int offset) {

  Request request;
  request.artist_id = artist_id;
  request.album_id = album_id;
  request.album_artist = album_artist;
  request.album = album;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);
  ++album_songs_requested_;
  if (album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();

}

void QobuzRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {

    Request request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    ParamList params = ParamList() << Param("album_id", request.album_id);
    if (request.offset > 0) params << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("album/get"), params);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist_id, request.album_id, request.offset, request.album_artist, request.album); });

  }

}

void QobuzRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const QString &artist_id, const QString &album_id, const int offset_requested, const QString &album_artist, const QString &album) {

  --album_songs_requests_active_;
  ++album_songs_received_;
  if (offset_requested == 0) {
    emit UpdateProgress(query_id_, album_songs_received_);
  }
  SongsReceived(reply, artist_id, album_id, 0, offset_requested, album_artist, album);

}

void QobuzRequest::SongsReceived(QNetworkReply *reply, const QString &artist_id_requested, const QString &album_id_requested, const int limit_requested, const int offset_requested, const QString &album_artist_requested, const QString &album_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);

  if (finished_) return;

  if (data.isEmpty()) {
    SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist_requested, album_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist_requested, album_requested);
    return;
  }

  if (!json_obj.contains("tracks")) {
    Error("Json object is missing tracks.", json_obj);
    SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist_requested, album_requested);
    return;
  }

  QString artist_id = artist_id_requested;
  QString album_artist = album_artist_requested;
  QString album_id = album_id_requested;
  QString album = album_requested;
  QUrl cover_url;

  if (json_obj.contains("id")) {
    if (json_obj["id"].isString()) {
      album_id = json_obj["id"].toString();
    }
    else {
      album_id = QString::number(json_obj["id"].toInt());
    }
  }

  if (json_obj.contains("title")) {
    album = json_obj["title"].toString();
  }

  if (json_obj.contains("artist")) {
    QJsonValue value_artist = json_obj["artist"];
    if (!value_artist.isObject()) {
      Error("Invalid Json reply, album artist is not a object.", value_artist);
      SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
      return;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("id") || !obj_artist.contains("name")) {
      Error("Invalid Json reply, album artist is missing id or name.", obj_artist);
      SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
      return;
    }
    if (obj_artist["id"].isString()) {
      artist_id = obj_artist["id"].toString();
    }
    else {
      artist_id = QString::number(obj_artist["id"].toInt());
    }
    album_artist = obj_artist["name"].toString();
  }

  if (json_obj.contains("image")) {
    QJsonValue value_image = json_obj["image"];
    if (!value_image.isObject()) {
      Error("Invalid Json reply, album image is not a object.", value_image);
      SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
      return;
    }
    QJsonObject obj_image = value_image.toObject();
    if (!obj_image.contains("large")) {
      Error("Invalid Json reply, album image is missing large.", obj_image);
      SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
      return;
    }
    QString album_image = obj_image["large"].toString();
    if (!album_image.isEmpty()) {
      cover_url = QUrl(album_image);
    }
  }

  QJsonValue value_tracks = json_obj["tracks"];
  if (!value_tracks.isObject()) {
    Error("Json tracks is not an object.", json_obj);
    SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
    return;
  }
  QJsonObject obj_tracks = value_tracks.toObject();

  if (!obj_tracks.contains("limit") ||
      !obj_tracks.contains("offset") ||
      !obj_tracks.contains("total") ||
      !obj_tracks.contains("items")) {
    SongsFinishCheck(artist_id_requested, album_id_requested, limit_requested, offset_requested, 0, 0, album_artist, album);
    Error("Json songs object is missing values.", json_obj);
    return;
  }

  //int limit = obj_tracks["limit"].toInt();
  int offset = obj_tracks["offset"].toInt();
  int songs_total = obj_tracks["total"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist, album);
    return;
  }

  QJsonValue value_items = ExtractItems(obj_tracks);
  if (!value_items.isArray()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist, album);
    return;
  }

  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    if ((type_ == QueryType_Songs || type_ == QueryType_SearchSongs) && offset_requested == 0) {
      no_results_ = true;
    }
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist, album);
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValueRef value_item : array_items) {

    if (!value_item.isObject()) {
      Error("Invalid Json reply, track is not a object.");
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    ++songs_received;
    Song song(Song::Source_Qobuz);
    ParseSong(song, obj_item, artist_id, album_id, album_artist, album, cover_url);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
    songs << song;
  }

  for (Song song : songs) {
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) song.set_disc(0);
    songs_.insert(song.song_id(), song);
  }

  SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, songs_received, album_artist, album);

}

void QobuzRequest::SongsFinishCheck(const QString &artist_id, const QString &album_id, const int limit, const int offset, const int songs_total, const int songs_received, const QString &album_artist, const QString &album) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (type_) {
        case QueryType_Songs:
          AddSongsRequest(offset_next);
          break;
        case QueryType_SearchSongs:
          AddSongsSearchRequest(offset_next);
          break;
        case QueryType_Artists:
        case QueryType_SearchArtists:
        case QueryType_Albums:
        case QueryType_SearchAlbums:
          AddAlbumSongsRequest(artist_id, album_id, album_artist, album, offset_next);
          break;
        default:
          break;
      }
    }
  }

  if (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();
  if (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();

  if (
      service_->download_album_covers() &&
      IsQuery() &&
      songs_requests_queue_.isEmpty() &&
      songs_requests_active_ <= 0 &&
      album_songs_requests_queue_.isEmpty() &&
      album_songs_requests_active_ <= 0 &&
      album_cover_requests_queue_.isEmpty() &&
      album_covers_received_ <= 0 &&
      album_covers_requests_sent_.isEmpty() &&
      album_songs_received_ >= album_songs_requested_
  ) {
    GetAlbumCovers();
  }

  FinishCheck();

}

QString QobuzRequest::ParseSong(Song &song, const QJsonObject &json_obj, QString artist_id, QString album_id, QString album_artist, QString album, QUrl cover_url) {

  if (
      !json_obj.contains("id") ||
      !json_obj.contains("title") ||
      !json_obj.contains("track_number") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("copyright") ||
      !json_obj.contains("streamable")
    ) {
    Error("Invalid Json reply, track is missing one or more values.", json_obj);
    return QString();
  }

  QString song_id;
  if (json_obj["id"].isString()) {
    song_id = json_obj["id"].toString();
  }
  else {
    song_id = QString::number(json_obj["id"].toInt());
  }

  QString title = json_obj["title"].toString();
  int track = json_obj["track_number"].toInt();
  QString copyright = json_obj["copyright"].toString();
  qint64 duration = json_obj["duration"].toInt() * kNsecPerSec;
  //bool streamable = json_obj["streamable"].toBool();
  QString composer;
  QString performer;

  if (json_obj.contains("album")) {

    QJsonValue value_album = json_obj["album"];
    if (!value_album.isObject()) {
      Error("Invalid Json reply, album is not an object.", value_album);
      return QString();
    }
    QJsonObject obj_album = value_album.toObject();

    if (obj_album.contains("id")) {
      if (obj_album["id"].isString()) {
        album_id = obj_album["id"].toString();
      }
      else {
        album_id = QString::number(obj_album["id"].toInt());
      }
    }

    if (obj_album.contains("title")) {
      album = obj_album["title"].toString();
    }

    if (obj_album.contains("artist")) {
      QJsonValue value_artist = obj_album["artist"];
      if (!value_artist.isObject()) {
        Error("Invalid Json reply, album artist is not a object.", value_artist);
        return QString();
      }
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("id") || !obj_artist.contains("name")) {
        Error("Invalid Json reply, album artist is missing id or name.", obj_artist);
        return QString();
      }
      if (obj_artist["id"].isString()) {
        artist_id = obj_artist["id"].toString();
      }
      else {
        artist_id = QString::number(obj_artist["id"].toInt());
      }
      album_artist = obj_artist["name"].toString();
    }

    if (obj_album.contains("image")) {
      QJsonValue value_image = obj_album["image"];
      if (!value_image.isObject()) {
        Error("Invalid Json reply, album image is not a object.", value_image);
        return QString();
      }
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("large")) {
        Error("Invalid Json reply, album image is missing large.", obj_image);
        return QString();
      }
      QString album_image = obj_image["large"].toString();
      if (!album_image.isEmpty()) {
        cover_url = QUrl(album_image);
      }
    }
  }

  if (json_obj.contains("composer")) {
    QJsonValue value_composer = json_obj["composer"];
    if (!value_composer.isObject()) {
      Error("Invalid Json reply, track composer is not a object.", value_composer);
      return QString();
    }
    QJsonObject obj_composer = value_composer.toObject();
    if (!obj_composer.contains("id") || !obj_composer.contains("name")) {
      Error("Invalid Json reply, track composer is missing id or name.", obj_composer);
      return QString();
    }
    composer = obj_composer["name"].toString();
  }

  if (json_obj.contains("performer")) {
    QJsonValue value_performer = json_obj["performer"];
    if (!value_performer.isObject()) {
      Error("Invalid Json reply, track performer is not a object.", value_performer);
      return QString();
    }
    QJsonObject obj_performer = value_performer.toObject();
    if (!obj_performer.contains("id") || !obj_performer.contains("name")) {
      Error("Invalid Json reply, track performer is missing id or name.", obj_performer);
      return QString();
    }
    performer = obj_performer["name"].toString();
  }

  //if (!streamable) {
  //Warn(QString("Song %1 %2 %3 is not streamable").arg(album_artist).arg(album).arg(title));
  //}

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  title.remove(Song::kTitleRemoveMisc);

  //qLog(Debug) << "id" << song_id << "track" << track << "title" << title << "album" << album << "album artist" << album_artist << cover_url << streamable << url;

  song.set_source(Song::Source_Qobuz);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  song.set_album(album);
  song.set_artist(album_artist);
  song.set_title(title);
  song.set_track(track);
  song.set_url(url);
  song.set_length_nanosec(duration);
  song.set_art_automatic(cover_url);
  song.set_performer(performer);
  song.set_composer(composer);
  song.set_comment(copyright);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType_Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

  return song_id;

}

void QobuzRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ == 1) emit UpdateStatus(query_id_, tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else emit UpdateStatus(query_id_, tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  emit ProgressSetMaximum(query_id_, album_covers_requested_);
  emit UpdateProgress(query_id_, 0);

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
  request.filename = app_->album_cover_loader()->CoverFilePath(song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), cover_url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(cover_url, song.song_id());
  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void QobuzRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.url, request.filename); });

  }

}

void QobuzRequest::AlbumCoverReceived(QNetworkReply *reply, const QUrl &cover_url, const QString &filename) {

  if (album_cover_replies_.contains(reply)) {
    album_cover_replies_.removeAll(reply);
    reply->deleteLater();
  }
  else {
    AlbumCoverFinishCheck();
    return;
  }

  --album_covers_requests_active_;
  ++album_covers_received_;

  if (finished_) return;

  emit UpdateProgress(query_id_, album_covers_received_);

  if (!album_covers_requests_sent_.contains(cover_url)) {
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    AlbumCoverFinishCheck();
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(';')) {
    mimetype = mimetype.left(mimetype.indexOf(';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QString("Unsupported mimetype for image reader %1 for %2").arg(mimetype, cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QString("Received empty image data for %1").arg(cover_url.toString()));
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    AlbumCoverFinishCheck();
    return;
  }

  QList<QByteArray> format_list = ImageUtils::ImageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list.first().data();
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
      Error(QString("Error saving image data to %1").arg(filename));
      if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    }
  }
  else {
    if (album_covers_requests_sent_.contains(cover_url)) album_covers_requests_sent_.remove(cover_url);
    Error(QString("Error decoding image data from %1").arg(cover_url.toString()));
  }

  AlbumCoverFinishCheck();

}

void QobuzRequest::AlbumCoverFinishCheck() {

  if (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests)
    FlushAlbumCoverRequests();

  FinishCheck();

}

void QobuzRequest::FinishCheck() {

  if (
      !finished_ &&
      albums_requests_queue_.isEmpty() &&
      artists_requests_queue_.isEmpty() &&
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
      artist_albums_received_ >= artist_albums_requested_ &&
      album_songs_requests_active_ <= 0 &&
      album_songs_received_ >= album_songs_requested_ &&
      album_covers_requested_ <= album_covers_received_ &&
      album_covers_requests_active_ <= 0 &&
      album_covers_received_ >= album_covers_requested_
  ) {
    finished_ = true;
    if (no_results_ && songs_.isEmpty()) {
      if (IsSearch())
        emit Results(query_id_, SongMap(), tr("No match."));
      else
        emit Results(query_id_, SongMap(), QString());
    }
    else {
      if (songs_.isEmpty() && errors_.isEmpty())
        emit Results(query_id_, songs_, tr("Unknown error"));
      else
        emit Results(query_id_, songs_, ErrorsToHTML(errors_));
    }
  }

}

void QobuzRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    errors_ << error;
    qLog(Error) << "Qobuz:" << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;
  FinishCheck();

}

void QobuzRequest::Warn(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
