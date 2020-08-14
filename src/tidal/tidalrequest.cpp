/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/application.h"
#include "core/utilities.h"
#include "covermanager/albumcoverloader.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalbaserequest.h"
#include "tidalrequest.h"

const char *TidalRequest::kResourcesUrl = "https://resources.tidal.com";
const int TidalRequest::kMaxConcurrentArtistsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumsRequests = 3;
const int TidalRequest::kMaxConcurrentSongsRequests = 3;
const int TidalRequest::kMaxConcurrentArtistAlbumsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumSongsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumCoverRequests = 1;

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, Application *app, NetworkAccessManager *network, QueryType type, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
      network_(network),
      type_(type),
      fetchalbums_(service->fetchalbums()),
      coversize_(service_->coversize()),
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
      need_login_(false),
      no_results_(false) {}

TidalRequest::~TidalRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  while (!album_cover_replies_.isEmpty()) {
    QNetworkReply *reply = album_cover_replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void TidalRequest::LoginComplete(const bool success, QString error) {

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
    emit UpdateStatus(query_id_, tr("Authenticating..."));
    need_login_ = true;
    service_->TryLogin();
    return;
  }

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

void TidalRequest::Search(const int query_id, const QString &search_text) {
  query_id_ = query_id;
  search_text_ = search_text;
}

void TidalRequest::GetArtists() {

  emit UpdateStatus(query_id_, tr("Retrieving artists..."));
  emit UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void TidalRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);
  if (artists_requests_active_ < kMaxConcurrentArtistsRequests) FlushArtistsRequests();

}

void TidalRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    Request request = artists_requests_queue_.dequeue();
    ++artists_requests_active_;

    ParamList parameters;
    if (type_ == QueryType_SearchArtists) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply(nullptr);
    if (type_ == QueryType_Artists) {
      reply = CreateRequest(QString("users/%1/favorites/artists").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchArtists) {
      reply = CreateRequest("search/artists", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { ArtistsReplyReceived(reply, request.limit, request.offset); });

  }

}

void TidalRequest::GetAlbums() {

  emit UpdateStatus(query_id_, tr("Retrieving albums..."));
  emit UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void TidalRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);
  if (albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();

}

void TidalRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    Request request = albums_requests_queue_.dequeue();
    ++albums_requests_active_;

    ParamList parameters;
    if (type_ == QueryType_SearchAlbums) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply(nullptr);
    if (type_ == QueryType_Albums) {
      reply = CreateRequest(QString("users/%1/favorites/albums").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchAlbums) {
      reply = CreateRequest("search/albums", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumsReplyReceived(reply, request.limit, request.offset); });

  }

}

void TidalRequest::GetSongs() {

  emit UpdateStatus(query_id_, tr("Retrieving songs..."));
  emit UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void TidalRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);
  if (songs_requests_active_ < kMaxConcurrentSongsRequests) FlushSongsRequests();

}

void TidalRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    Request request = songs_requests_queue_.dequeue();
    ++songs_requests_active_;

    ParamList parameters;
    if (type_ == QueryType_SearchSongs) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply(nullptr);
    if (type_ == QueryType_Songs) {
      reply = CreateRequest(QString("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchSongs) {
      reply = CreateRequest("search/tracks", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { SongsReplyReceived(reply, request.limit, request.offset); });

  }

}

void TidalRequest::ArtistsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void TidalRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void TidalRequest::AlbumsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void TidalRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void TidalRequest::SongsSearch() {

  emit UpdateStatus(query_id_, tr("Searching..."));
  emit UpdateProgress(query_id_, 0);
  AddSongsSearchRequest();

}

void TidalRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void TidalRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply, (offset_requested == 0));

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

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    ArtistsFinishCheck();
    Error("Json object missing values.", json_obj);
    return;
  }
  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int artists_total = json_obj["totalNumberOfItems"].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QString("totalNumberOfItems returned does not match previous totalNumberOfItems! %1 != %2").arg(artists_total).arg(artists_total_));
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

  QJsonValue value_items = ExtractItems(json_obj);
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
  for (const QJsonValue &value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error("Invalid Json reply, item in array is not a object.", value_item);
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item")) {
      QJsonValue json_item = obj_item["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item in array is not a object.", json_item);
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

void TidalRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

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

void TidalRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {
  --albums_requests_active_;
  AlbumsReceived(reply, QString(), limit_requested, offset_requested, (offset_requested == 0));
  if (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();
}

void TidalRequest::AddArtistAlbumsRequest(const QString &artist_id, const int offset) {

  Request request;
  request.artist_id = artist_id;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);
  if (artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) FlushArtistAlbumsRequests();

}

void TidalRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    Request request = artist_albums_requests_queue_.dequeue();
    ++artist_albums_requests_active_;

    ParamList parameters;
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("artists/%1/albums").arg(request.artist_id), parameters);
    connect(reply, &QNetworkReply::finished, [=] { ArtistAlbumsReplyReceived(reply, request.artist_id, request.offset); });
    replies_ << reply;

  }

}

void TidalRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const QString &artist_id, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_received_;
  emit UpdateProgress(query_id_, artist_albums_received_);
  AlbumsReceived(reply, artist_id, 0, offset_requested, false);
  if (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) FlushArtistAlbumsRequests();

}

void TidalRequest::AlbumsReceived(QNetworkReply *reply, const QString &artist_id_requested, const int limit_requested, const int offset_requested, const bool auto_login) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply, auto_login);

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

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int albums_total = json_obj["totalNumberOfItems"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  QJsonValue value_items = ExtractItems(json_obj);
  if (!value_items.isArray()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }
  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    if ((type_ == QueryType_Albums || type_ == QueryType_SearchAlbums || (type_ == QueryType_SearchSongs && fetchalbums_)) && offset_requested == 0) {
      no_results_ = true;
    }
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value_item : array_items) {

    ++albums_received;

    if (!value_item.isObject()) {
      Error("Invalid Json reply, item in array is not a object.", value_item);
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item")) {
      QJsonValue json_item = obj_item["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item in array is not a object.", json_item);
        continue;
      }
      obj_item = json_item.toObject();
    }

    QString album_id;
    QString album;
    if (obj_item.contains("type")) {  // This was a albums request or search
      if (!obj_item.contains("id") || !obj_item.contains("title")) {
        Error("Invalid Json reply, item is missing ID or title.", obj_item);
        continue;
      }
      if (obj_item["id"].isString()) {
        album_id = obj_item["id"].toString();
      }
      else {
        album_id = QString::number(obj_item["id"].toInt());
      }
      album = obj_item["title"].toString();
    }
    else if (obj_item.contains("album")) {  // This was a tracks request or search
      QJsonValue value_album = obj_item["album"];
      if (!value_album.isObject()) {
        Error("Invalid Json reply, item album is not a object.", value_album);
        continue;
      }
      QJsonObject obj_album = value_album.toObject();
      if (!obj_album.contains("id") || !obj_album.contains("title")) {
        Error("Invalid Json reply, item album is missing ID or title.", obj_album);
        continue;
      }
      if (obj_album["id"].isString()) {
        album_id = obj_album["id"].toString();
      }
      else {
        album_id = QString::number(obj_album["id"].toInt());
      }
      album = obj_album["title"].toString();

    }
    else {
      Error("Invalid Json reply, item missing type or album.", obj_item);
      continue;
    }

    if (album_songs_requests_pending_.contains(album_id)) continue;

    if (!obj_item.contains("artist") || !obj_item.contains("title") || !obj_item.contains("audioQuality")) {
      Error("Invalid Json reply, item missing artist, title or audioQuality.", obj_item);
      continue;
    }
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

    QString quality = obj_item["audioQuality"].toString();
    QString copyright = obj_item["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    Request request;
    if (artist_id_requested.isEmpty()) {
      request.artist_id = artist_id;
    }
    else {
      request.artist_id = artist_id_requested;
    }
    request.album_id = album_id;
    request.album_artist = artist;
    album_songs_requests_pending_.insert(album_id, request);

  }

  AlbumsFinishCheck(artist_id_requested, limit_requested, offset, albums_total, albums_received);

}

void TidalRequest::AlbumsFinishCheck(const QString &artist_id, const int limit, const int offset, const int albums_total, const int albums_received) {

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

    QHash<QString, Request> ::iterator i;
    for (i = album_songs_requests_pending_.begin() ; i != album_songs_requests_pending_.end() ; ++i) {
      Request request = i.value();
      AddAlbumSongsRequest(request.artist_id, request.album_id, request.album_artist);
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

void TidalRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  if (type_ == QueryType_SearchSongs && fetchalbums_) {
    AlbumsReceived(reply, QString(), limit_requested, offset_requested, (offset_requested == 0));
  }
  else {
    SongsReceived(reply, QString(), QString(), limit_requested, offset_requested, (offset_requested == 0));
  }

}

void TidalRequest::AddAlbumSongsRequest(const QString &artist_id, const QString &album_id, const QString &album_artist, const int offset) {

  Request request;
  request.artist_id = artist_id;
  request.album_id = album_id;
  request.album_artist = album_artist;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);
  ++album_songs_requested_;
  if (album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();

}

void TidalRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {

    Request request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    ParamList parameters;
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(request.album_id), parameters);
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumSongsReplyReceived(reply, request.artist_id, request.album_id, request.offset, request.album_artist); });

  }

}

void TidalRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const QString &artist_id, const QString &album_id, const int offset_requested, const QString &album_artist) {

  --album_songs_requests_active_;
  ++album_songs_received_;
  if (offset_requested == 0) {
    emit UpdateProgress(query_id_, album_songs_received_);
  }
  SongsReceived(reply, artist_id, album_id, 0, offset_requested, false, album_artist);

}

void TidalRequest::SongsReceived(QNetworkReply *reply, const QString &artist_id, const QString &album_id, const int limit_requested, const int offset_requested, const bool auto_login, const QString &album_artist) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply, auto_login);

  if (finished_) return;

  if (data.isEmpty()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, 0, 0, album_artist);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, 0, 0, album_artist);
    return;
  }

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, 0, 0, album_artist);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int songs_total = json_obj["totalNumberOfItems"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist);
    return;
  }

  QJsonValue json_value = ExtractItems(json_obj);
  if (!json_value.isArray()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist);
    return;
  }

  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    if ((type_ == QueryType_Songs || type_ == QueryType_SearchSongs) && offset_requested == 0) {
      no_results_ = true;
    }
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist);
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValue &value_item : json_items) {

    if (!value_item.isObject()) {
      Error("Invalid Json reply, track is not a object.", value_item);
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    if (obj_item.contains("item")) {
      QJsonValue item = obj_item["item"];
      if (!item.isObject()) {
        Error("Invalid Json reply, item is not a object.", item);
        continue;
      }
      obj_item = item.toObject();
    }

    ++songs_received;
    Song song(Song::Source_Tidal);
    ParseSong(song, obj_item, artist_id, album_id, album_artist);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
    songs << song;
  }

  for (Song &song : songs) {
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) {
      song.set_disc(0);
    }
    songs_ << song;
  }

  SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, songs_received, album_artist);

}

void TidalRequest::SongsFinishCheck(const QString &artist_id, const QString &album_id, const int limit, const int offset, const int songs_total, const int songs_received, const QString &album_artist) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (type_) {
        case QueryType_Songs:
          AddSongsRequest(offset_next);
          break;
        case QueryType_SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So fallthrough.
          if (artist_id.isEmpty() && album_id.isEmpty()) {
            AddSongsSearchRequest(offset_next);
            break;
          }
          // fallthrough
        case QueryType_Artists:
        case QueryType_SearchArtists:
        case QueryType_Albums:
        case QueryType_SearchAlbums:
          AddAlbumSongsRequest(artist_id, album_id, album_artist, offset_next);
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

QString TidalRequest::ParseSong(Song &song, const QJsonObject &json_obj, const QString &artist_id_requested, const QString &album_id_requested, const QString &album_artist) {

  Q_UNUSED(artist_id_requested);

  if (
      !json_obj.contains("album") ||
      !json_obj.contains("allowStreaming") ||
      !json_obj.contains("artist") ||
      !json_obj.contains("artists") ||
      !json_obj.contains("audioQuality") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("id") ||
      !json_obj.contains("streamReady") ||
      !json_obj.contains("title") ||
      !json_obj.contains("trackNumber") ||
      !json_obj.contains("url") ||
      !json_obj.contains("volumeNumber") ||
      !json_obj.contains("copyright")
    ) {
    Error("Invalid Json reply, track is missing one or more values.", json_obj);
    return QString();
  }

  QJsonValue value_artist = json_obj["artist"];
  QJsonValue value_album = json_obj["album"];
  QJsonValue json_duration = json_obj["duration"];
  QJsonArray array_artists = json_obj["artists"].toArray();

  QString song_id;
  if (json_obj["id"].isString()) {
    song_id = json_obj["id"].toString();
  }
  else {
    song_id = QString::number(json_obj["id"].toInt());
  }

  QString title = json_obj["title"].toString();
  QString urlstr = json_obj["url"].toString();
  int track = json_obj["trackNumber"].toInt();
  int disc = json_obj["volumeNumber"].toInt();
  bool allow_streaming = json_obj["allowStreaming"].toBool();
  bool stream_ready = json_obj["streamReady"].toBool();
  QString copyright = json_obj["copyright"].toString();

  if (!value_artist.isObject()) {
    Error("Invalid Json reply, track artist is not a object.", value_artist);
    return QString();
  }
  QJsonObject obj_artist = value_artist.toObject();
  if (!obj_artist.contains("id") || !obj_artist.contains("name")) {
    Error("Invalid Json reply, track artist is missing id or name.", obj_artist);
    return QString();
  }
  QString artist_id;
  if (obj_artist["id"].isString()) {
    artist_id = obj_artist["id"].toString();
  }
  else {
    artist_id = QString::number(obj_artist["id"].toInt());
  }
  QString artist = obj_artist["name"].toString();

  if (!value_album.isObject()) {
    Error("Invalid Json reply, track album is not a object.", value_album);
    return QString();
  }
  QJsonObject obj_album = value_album.toObject();
  if (!obj_album.contains("id") || !obj_album.contains("title") || !obj_album.contains("cover")) {
    Error("Invalid Json reply, track album is missing id, title or cover.", obj_album);
    return QString();
  }
  QString album_id;
  if (obj_album["id"].isString()) {
    album_id = obj_album["id"].toString();
  }
  else {
    album_id = QString::number(obj_album["id"].toInt());
  }
  if (!album_id_requested.isEmpty() && album_id_requested != album_id) {
    Error("Invalid Json reply, track album id is wrong.", obj_album);
    return QString();
  }
  QString album = obj_album["title"].toString();
  QString cover = obj_album["cover"].toString();

  if (!allow_streaming) {
    Warn(QString("Song %1 %2 %3 is not allowStreaming").arg(artist).arg(album).arg(title));
    return QString();
  }

  if (!stream_ready) {
    Warn(QString("Song %1 %2 %3 is not streamReady").arg(artist).arg(album).arg(title));
    return QString();
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  QVariant q_duration = json_duration.toVariant();
  quint64 duration = 0;
  if (q_duration.isValid()) {
    duration = q_duration.toLongLong() * kNsecPerSec;
  }
  else {
    Error("Invalid duration for song.", json_duration);
    return QString();
  }

  cover = cover.replace("-", "/");
  QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg(coversize_));

  title.remove(Song::kTitleRemoveMisc);

  //qLog(Debug) << "id" << song_id << "track" << track << "disc" << disc << "title" << title << "album" << album << "album artist" << album_artist << "artist" << artist << cover << allow_streaming << url;

  song.set_source(Song::Source_Tidal);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  if (album_artist != artist) song.set_albumartist(album_artist);
  song.set_album(album);
  song.set_artist(artist);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);
  song.set_url(url);
  song.set_length_nanosec(duration);
  song.set_art_automatic(cover_url);
  song.set_comment(copyright);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType_Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

  return song_id;

}

void TidalRequest::GetAlbumCovers() {

  for (Song &song : songs_) {
    AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ == 1) emit UpdateStatus(query_id_, tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else emit UpdateStatus(query_id_, tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  emit ProgressSetMaximum(query_id_, album_covers_requested_);
  emit UpdateProgress(query_id_, 0);

}

void TidalRequest::AddAlbumCoverRequest(Song &song) {

  if (album_covers_requests_sent_.contains(song.album_id())) {
    album_covers_requests_sent_.insert(song.album_id(), &song);
    return;
  }

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = QUrl(song.art_automatic());
  request.filename = app_->album_cover_loader()->CoverFilePath(song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), request.url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(song.album_id(), &song);
  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void TidalRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumCoverReceived(reply, request.album_id, request.url, request.filename); });

  }

}

void TidalRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

  if (album_cover_replies_.contains(reply)) {
    album_cover_replies_.removeAll(reply);
    disconnect(reply, nullptr, this, nullptr);
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

  if (!album_covers_requests_sent_.contains(album_id)) {
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
     Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (!QImageReader::supportedMimeTypes().contains(mimetype.toUtf8())) {
    Error(QString("Unsupported mimetype for image reader %1 for %2").arg(mimetype).arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 12, 0))
  QList<QByteArray> format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
#else
  QList<QByteArray> format_list = Utilities::ImageFormatsForMimeType(mimetype.toUtf8());
#endif

  QByteArray data = reply->readAll();
  if (format_list.isEmpty() || data.isEmpty()) {
    Error(QString("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }
  QByteArray format = format_list.first();

  QImage image;
  if (image.loadFromData(data, format)) {
    if (image.save(filename, format)) {
      while (album_covers_requests_sent_.contains(album_id)) {
        Song *song = album_covers_requests_sent_.take(album_id);
        song->set_art_automatic(QUrl::fromLocalFile(filename));
      }
    }
    else {
      Error(QString("Error saving image data to %1").arg(filename));
      if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    }
  }
  else {
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    Error(QString("Error decoding image data from %1").arg(url.toString()));
  }

  AlbumCoverFinishCheck();

}

void TidalRequest::AlbumCoverFinishCheck() {

  if (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests)
    FlushAlbumCoverRequests();

  FinishCheck();

}

void TidalRequest::FinishCheck() {

  if (
      !finished_ &&
      !need_login_ &&
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
        emit Results(query_id_, SongList(), tr("No match."));
      else
        emit Results(query_id_, SongList(), QString());
    }
    else {
      if (songs_.isEmpty() && errors_.isEmpty())
        emit Results(query_id_, songs_, tr("Unknown error"));
      else
        emit Results(query_id_, songs_, ErrorsToHTML(errors_));
    }
  }

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

