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

constexpr char TidalRequest::kResourcesUrl[] = "https://resources.tidal.com";
constexpr int TidalRequest::kMaxConcurrentArtistsRequests = 3;
constexpr int TidalRequest::kMaxConcurrentAlbumsRequests = 3;
constexpr int TidalRequest::kMaxConcurrentSongsRequests = 3;
constexpr int TidalRequest::kMaxConcurrentArtistAlbumsRequests = 3;
constexpr int TidalRequest::kMaxConcurrentAlbumSongsRequests = 3;
constexpr int TidalRequest::kMaxConcurrentAlbumCoverRequests = 1;
constexpr int TidalRequest::kFlushRequestsDelay = 200;

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, Application *app, NetworkAccessManager *network, QueryType query_type, QObject *parent)
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
    emit UpdateStatus(query_id_, tr("Authenticating..."));
    need_login_ = true;
    service_->TryLogin();
    return;
  }

  switch (query_type_) {
    case QueryType::Artists:
      GetArtists();
      break;
    case QueryType::Albums:
      GetAlbums();
      break;
    case QueryType::Songs:
      GetSongs();
      break;
    case QueryType::SearchArtists:
      ArtistsSearch();
      break;
    case QueryType::SearchAlbums:
      AlbumsSearch();
      break;
    case QueryType::SearchSongs:
      SongsSearch();
      break;
    default:
      Error("Invalid query type.");
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

  emit UpdateStatus(query_id_, tr("Receiving artists..."));
  emit UpdateProgress(query_id_, 0);
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
    if (query_type_ == QueryType::SearchArtists) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == QueryType::Artists) {
      reply = CreateRequest(QString("users/%1/favorites/artists").arg(service_->user_id()), parameters);
    }
    if (query_type_ == QueryType::SearchArtists) {
      reply = CreateRequest("search/artists", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

    ++artists_requests_active_;

  }

}

void TidalRequest::GetAlbums() {

  emit UpdateStatus(query_id_, tr("Receiving albums..."));
  emit UpdateProgress(query_id_, 0);
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
    if (query_type_ == QueryType::SearchAlbums) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == QueryType::Albums) {
      reply = CreateRequest(QString("users/%1/favorites/albums").arg(service_->user_id()), parameters);
    }
    if (query_type_ == QueryType::SearchAlbums) {
      reply = CreateRequest("search/albums", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

    ++albums_requests_active_;

  }

}

void TidalRequest::GetSongs() {

  emit UpdateStatus(query_id_, tr("Receiving songs..."));
  emit UpdateProgress(query_id_, 0);
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
    if (query_type_ == QueryType::SearchSongs) parameters << Param("query", search_text_);
    if (request.limit > 0) parameters << Param("limit", QString::number(request.limit));
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (query_type_ == QueryType::Songs) {
      reply = CreateRequest(QString("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
    }
    if (query_type_ == QueryType::SearchSongs) {
      reply = CreateRequest("search/tracks", parameters);
    }
    if (!reply) continue;
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

    ++songs_requests_active_;

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

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    ArtistsFinishCheck();
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
    emit UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));
  }

  QJsonValue value_items = ExtractItems(json_obj);
  if (!value_items.isArray()) {
    ArtistsFinishCheck();
    return;
  }

  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {  // Empty array means no results
    ArtistsFinishCheck();
    return;
  }

  int artists_received = 0;
  for (const QJsonValueRef value_item : array_items) {

    ++artists_received;

    if (!value_item.isObject()) {
      Error("Invalid Json reply, item in array is not a object.");
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

    Artist artist;
    if (obj_item["id"].isString()) {
      artist.artist_id = obj_item["id"].toString();
    }
    else {
      artist.artist_id = QString::number(obj_item["id"].toInt());
    }
    artist.artist = obj_item["name"].toString();

    if (artist_albums_requests_pending_.contains(artist.artist_id)) continue;

    ArtistAlbumsRequest request;
    request.artist = artist;
    artist_albums_requests_pending_.insert(artist.artist_id, request);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) emit UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));

  ArtistsFinishCheck(limit_requested, offset, artists_received);

}

void TidalRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if (finished_) return;

  if ((limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (query_type_ == QueryType::Artists) AddArtistsRequest(offset_next);
      else if (query_type_ == QueryType::SearchArtists) AddArtistsSearchRequest(offset_next);
    }
  }

  if (artists_requests_queue_.isEmpty() && artists_requests_active_ <= 0) {  // Artist query is finished, get all albums for all artists.

    // Get artist albums
    QList<ArtistAlbumsRequest> requests = artist_albums_requests_pending_.values();
    for (const ArtistAlbumsRequest &request : requests) {
      AddArtistAlbumsRequest(request.artist);
    }
    artist_albums_requests_pending_.clear();

    if (artist_albums_requests_total_ > 0) {
      if (artist_albums_requests_total_ == 1) emit UpdateStatus(query_id_, tr("Receiving albums for %1 artist...").arg(artist_albums_requests_total_));
      else emit UpdateStatus(query_id_, tr("Receiving albums for %1 artists...").arg(artist_albums_requests_total_));
      emit UpdateProgress(query_id_, 0);
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
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("artists/%1/albums").arg(request.artist.artist_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });
    replies_ << reply;

    ++artist_albums_requests_active_;

  }

}

void TidalRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  emit UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
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

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    AlbumsFinishCheck(artist_requested);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int albums_total = json_obj["totalNumberOfItems"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    AlbumsFinishCheck(artist_requested);
    return;
  }

  QJsonValue value_items = ExtractItems(json_obj);
  if (!value_items.isArray()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }
  QJsonArray array_items = value_items.toArray();
  if (array_items.isEmpty()) {
    AlbumsFinishCheck(artist_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValueRef value_item : array_items) {

    ++albums_received;

    if (!value_item.isObject()) {
      Error("Invalid Json reply, item in array is not a object.");
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

    Album album;
    if (obj_item.contains("type")) {  // This was an albums request or search
      if (!obj_item.contains("id") || !obj_item.contains("title")) {
        Error("Invalid Json reply, item is missing ID or title.", obj_item);
        continue;
      }
      if (obj_item["id"].isString()) {
        album.album_id = obj_item["id"].toString();
      }
      else {
        album.album_id = QString::number(obj_item["id"].toInt());
      }
      album.album = obj_item["title"].toString();
      if (service_->album_explicit() && obj_item.contains("explicit")) {
        album.album_explicit = obj_item["explicit"].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)");
        }
      }
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
        album.album_id = obj_album["id"].toString();
      }
      else {
        album.album_id = QString::number(obj_album["id"].toInt());
      }
      album.album = obj_album["title"].toString();
      if (service_->album_explicit() && obj_album.contains("explicit")) {
        album.album_explicit = obj_album["explicit"].toVariant().toBool();
        if (album.album_explicit && !album.album.isEmpty()) {
          album.album.append(" (Explicit)");
        }
      }
    }
    else {
      Error("Invalid Json reply, item missing type or album.", obj_item);
      continue;
    }

    if (album_songs_requests_pending_.contains(album.album_id)) continue;

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

    Artist album_artist;
    if (obj_artist["id"].isString()) {
      album_artist.artist_id = obj_artist["id"].toString();
    }
    else {
      album_artist.artist_id = QString::number(obj_artist["id"].toInt());
    }
    album_artist.artist = obj_artist["name"].toString();

    //QString quality = obj_item["audioQuality"].toString();
    //QString copyright = obj_item["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    AlbumSongsRequest request;
    request.artist = album_artist;
    request.album = album;
    album_songs_requests_pending_.insert(album.album_id, request);

  }

  if (query_type_ == QueryType::Albums || query_type_ == QueryType::SearchAlbums) {
    albums_received_ += albums_received;
    emit UpdateProgress(query_id_, GetProgress(albums_received_, albums_total_));
  }

  AlbumsFinishCheck(artist_requested, limit_requested, offset, albums_total, albums_received);

}

void TidalRequest::AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (finished_) return;

  if (limit == 0 || limit > albums_received) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      switch (query_type_) {
        case QueryType::Albums:
          AddAlbumsRequest(offset_next);
          break;
        case QueryType::SearchAlbums:
          AddAlbumsSearchRequest(offset_next);
          break;
        case QueryType::Artists:
        case QueryType::SearchArtists:
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

    for (QHash<QString, AlbumSongsRequest>::iterator it = album_songs_requests_pending_.begin(); it != album_songs_requests_pending_.end(); ++it) {
      const AlbumSongsRequest &request = it.value();
      AddAlbumSongsRequest(request.artist, request.album);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requests_total_ > 0) {
      if (album_songs_requests_total_ == 1) emit UpdateStatus(query_id_, tr("Receiving songs for %1 album...").arg(album_songs_requests_total_));
      else emit UpdateStatus(query_id_, tr("Receiving songs for %1 albums...").arg(album_songs_requests_total_));
      emit UpdateProgress(query_id_, 0);
    }
  }

  GetAlbumCoversCheck();
  FinishCheck();

}

void TidalRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  ++songs_requests_received_;
  if (query_type_ == QueryType::SearchSongs && fetchalbums_) {
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
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(request.album.album_id), parameters);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist, request.album, request.offset); });

    ++album_songs_requests_active_;

  }

}

void TidalRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int offset_requested) {

  --album_songs_requests_active_;
  ++album_songs_requests_received_;
  if (offset_requested == 0) {
    emit UpdateProgress(query_id_, GetProgress(album_songs_requests_received_, album_songs_requests_total_));
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

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    SongsFinishCheck(artist, album, limit_requested, offset_requested);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int songs_total = json_obj["totalNumberOfItems"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
    return;
  }

  QJsonValue json_value = ExtractItems(json_obj);
  if (!json_value.isArray()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
    return;
  }

  QJsonArray array_items = json_value.toArray();
  if (array_items.isEmpty()) {
    SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, 0);
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

    if (obj_item.contains("item")) {
      QJsonValue item = obj_item["item"];
      if (!item.isObject()) {
        Error("Invalid Json reply, item is not a object.", item);
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

  for (Song song : songs) {
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) song.set_disc(0);
    songs_.insert(song.song_id(), song);
  }

  if (query_type_ == QueryType::Songs || query_type_ == QueryType::SearchSongs) {
    songs_received_ += songs_received;
    emit UpdateProgress(query_id_, GetProgress(songs_received_, songs_total_));
  }

  SongsFinishCheck(artist, album, limit_requested, offset_requested, songs_total, songs_received);

}

void TidalRequest::SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (query_type_) {
        case QueryType::Songs:
          AddSongsRequest(offset_next);
          break;
        case QueryType::SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So fallthrough.
          if (artist.artist_id.isEmpty() && album.album_id.isEmpty()) {
            AddSongsSearchRequest(offset_next);
            break;
          }
          [[fallthrough]];
        case QueryType::Artists:
        case QueryType::SearchArtists:
        case QueryType::Albums:
        case QueryType::SearchAlbums:
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
    return;
  }

  QJsonValue value_artist = json_obj["artist"];
  QJsonValue value_album = json_obj["album"];
  QJsonValue json_duration = json_obj["duration"];
  //QJsonArray array_artists = json_obj["artists"].toArray();

  QString song_id;
  if (json_obj["id"].isString()) {
    song_id = json_obj["id"].toString();
  }
  else {
    song_id = QString::number(json_obj["id"].toInt());
  }

  QString title = json_obj["title"].toString();
  //QString urlstr = json_obj["url"].toString();
  int track = json_obj["trackNumber"].toInt();
  int disc = json_obj["volumeNumber"].toInt();
  bool allow_streaming = json_obj["allowStreaming"].toBool();
  bool stream_ready = json_obj["streamReady"].toBool();
  QString copyright = json_obj["copyright"].toString();

  if (!value_artist.isObject()) {
    Error("Invalid Json reply, track artist is not a object.", value_artist);
    return;
  }
  QJsonObject obj_artist = value_artist.toObject();
  if (!obj_artist.contains("id") || !obj_artist.contains("name")) {
    Error("Invalid Json reply, track artist is missing id or name.", obj_artist);
    return;
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
    return;
  }
  QJsonObject obj_album = value_album.toObject();
  if (!obj_album.contains("id") || !obj_album.contains("title")) {
    Error("Invalid Json reply, track album is missing ID or title.", obj_album);
    return;
  }
  QString album_id;
  if (obj_album["id"].isString()) {
    album_id = obj_album["id"].toString();
  }
  else {
    album_id = QString::number(obj_album["id"].toInt());
  }
  if (!album.album_id.isEmpty() && album.album_id != album_id) {
    Error("Invalid Json reply, track album id is wrong.", obj_album);
    return;
  }
  QString album_title = obj_album["title"].toString();
  if (album.album_explicit) album_title.append(" (Explicit)");

  if (!allow_streaming) {
    Warn(QString("Song %1 %2 %3 is not allowStreaming").arg(artist, album_title, title));
    return;
  }

  if (!stream_ready) {
    Warn(QString("Song %1 %2 %3 is not streamReady").arg(artist, album_title, title));
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
    Error("Invalid duration for song.", json_duration);
    return;
  }

  QUrl cover_url;
  if (obj_album.contains("cover")) {
    const QString cover = obj_album["cover"].toString().replace("-", "/");
    if (!cover.isEmpty()) {
      cover_url.setUrl(QString("%1/images/%2/%3.jpg").arg(kResourcesUrl, cover, coversize_));
    }
  }

  title.remove(Song::kTitleRemoveMisc);

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

  if (album_covers_requests_total_ == 1) emit UpdateStatus(query_id_, tr("Receiving album cover for %1 album...").arg(album_covers_requests_total_));
  else emit UpdateStatus(query_id_, tr("Receiving album covers for %1 albums...").arg(album_covers_requests_total_));
  emit UpdateProgress(query_id_, 0);

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

  emit UpdateProgress(query_id_, GetProgress(album_covers_requests_received_, album_covers_requests_total_));

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
  if (mimetype.contains(';')) {
    mimetype = mimetype.left(mimetype.indexOf(';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QString("Unsupported mimetype for image reader %1 for %2").arg(mimetype, url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QString("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArrayList format_list = ImageUtils::ImageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list.first().data();
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
          emit Results(query_id_, SongMap(), tr("No match."));
        }
        else {
          emit Results(query_id_);
        }
      }
      else {
        emit Results(query_id_, SongMap(), ErrorsToHTML(errors_));
      }
    }
    else {
      emit Results(query_id_, songs_);
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
