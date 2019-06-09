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
#include <QDir>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalrequest.h"

const char *TidalRequest::kResourcesUrl = "http://resources.tidal.com";
const int TidalRequest::kMaxConcurrentArtistsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumsRequests = 3;
const int TidalRequest::kMaxConcurrentSongsRequests = 3;
const int TidalRequest::kMaxConcurrentArtistAlbumsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumSongsRequests = 3;
const int TidalRequest::kMaxConcurrentAlbumCoverRequests = 1;

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, NetworkAccessManager *network, QueryType type, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      network_(network),
      type_(type),
      search_id_(-1),
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

  while (!album_cover_replies_.isEmpty()) {
    QNetworkReply *reply = album_cover_replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void TidalRequest::LoginComplete(bool success, QString error) {

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

void TidalRequest::Search(const int search_id, const QString &search_text) {
  search_id_ = search_id;
  search_text_ = search_text;
}

void TidalRequest::GetArtists() {

  emit UpdateStatus(tr("Retrieving artists..."));
  emit UpdateProgress(0);
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
    QNetworkReply *reply;
    if (type_ == QueryType_Artists) {
      reply = CreateRequest(QString("users/%1/favorites/artists").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchArtists) {
      reply = CreateRequest("search/artists", parameters);
    }
    NewClosure(reply, SIGNAL(finished()), this, SLOT(ArtistsReplyReceived(QNetworkReply*, int, int)), reply, request.limit, request.offset);

  }

}

void TidalRequest::GetAlbums() {

  emit UpdateStatus(tr("Retrieving albums..."));
  emit UpdateProgress(0);
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
    QNetworkReply *reply;
    if (type_ == QueryType_Albums) {
      reply = CreateRequest(QString("users/%1/favorites/albums").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchAlbums) {
      reply = CreateRequest("search/albums", parameters);
    }
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReplyReceived(QNetworkReply*, int, int)), reply, request.limit, request.offset);

  }

}

void TidalRequest::GetSongs() {

  emit UpdateStatus(tr("Retrieving songs..."));
  emit UpdateProgress(0);
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
    QNetworkReply *reply;
    if (type_ == QueryType_Songs) {
      reply = CreateRequest(QString("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
    }
    if (type_ == QueryType_SearchSongs) {
      reply = CreateRequest("search/tracks", parameters);
    }
    NewClosure(reply, SIGNAL(finished()), this, SLOT(SongsReplyReceived(QNetworkReply*, int, int)), reply, request.limit, request.offset);

  }

}

void TidalRequest::ArtistsSearch() {

  emit UpdateStatus(tr("Searching..."));
  emit UpdateProgress(0);
  AddArtistsSearchRequest();

}

void TidalRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void TidalRequest::AlbumsSearch() {

  emit UpdateStatus(tr("Searching..."));
  emit UpdateProgress(0);
  AddAlbumsSearchRequest();

}

void TidalRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void TidalRequest::SongsSearch() {

  emit UpdateStatus(tr("Searching..."));
  emit UpdateProgress(0);
  AddSongsSearchRequest();

}

void TidalRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void TidalRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  QString error;
  QByteArray data = GetReplyData(reply, error, (offset_requested == 0));

  --artists_requests_active_;

  if (finished_) return;

  if (data.isEmpty()) {
    ArtistsFinishCheck();
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
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
    emit ProgressSetMaximum(artists_total_);
    emit UpdateProgress(artists_received_);
  }

  QJsonValue json_value = ExtractItems(data, error);
  if (!json_value.isArray()) {
    ArtistsFinishCheck();
    return;
  }

  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {  // Empty array means no results
    no_results_ = true;
    ArtistsFinishCheck();
    return;
  }

  int artists_received = 0;
  for (const QJsonValue &value : json_items) {

    ++artists_received;

    if (!value.isObject()) {
      Error("Invalid Json reply, item not a object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (json_obj.contains("item")) {
      QJsonValue json_item = json_obj["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item not a object.", json_item);
        continue;
      }
      json_obj = json_item.toObject();
    }

    if (!json_obj.contains("id") || !json_obj.contains("name")) {
      Error("Invalid Json reply, item missing id or album.", json_obj);
      continue;
    }

    int artist_id = json_obj["id"].toInt();
    if (artist_albums_requests_pending_.contains(artist_id)) continue;
    artist_albums_requests_pending_.append(artist_id);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) emit UpdateProgress(artists_received_);

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
    for (int artist_id : artist_albums_requests_pending_) {
      AddArtistAlbumsRequest(artist_id);
      ++artist_albums_requested_;
    }
    artist_albums_requests_pending_.clear();

    if (artist_albums_requested_ > 0) {
      if (artist_albums_requested_ == 1) emit UpdateStatus(tr("Retrieving albums for %1 artist...").arg(artist_albums_requested_));
      else emit UpdateStatus(tr("Retrieving albums for %1 artists...").arg(artist_albums_requested_));
      emit ProgressSetMaximum(artist_albums_requested_);
      emit UpdateProgress(0);
    }

  }

  FinishCheck();

}

void TidalRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {
  --albums_requests_active_;
  AlbumsReceived(reply, 0, limit_requested, offset_requested, (offset_requested == 0));
  if (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();
}

void TidalRequest::AddArtistAlbumsRequest(const int artist_id, const int offset) {

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
    NewClosure(reply, SIGNAL(finished()), this, SLOT(ArtistAlbumsReplyReceived(QNetworkReply*, int, int)), reply, request.artist_id, request.offset);

  }

}

void TidalRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const int artist_id, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_received_;
  emit UpdateProgress(artist_albums_received_);
  AlbumsReceived(reply, artist_id, 0, offset_requested, false);
  if (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) FlushArtistAlbumsRequests();

}

void TidalRequest::AlbumsReceived(QNetworkReply *reply, const int artist_id_requested, const int limit_requested, const int offset_requested, const bool auto_login) {

  QString error;
  QByteArray data = GetReplyData(reply, error, auto_login);

  if (finished_) return;

  if (data.isEmpty()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
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

  QJsonValue json_value = ExtractItems(json_obj, error);
  if (!json_value.isArray()) {
    AlbumsFinishCheck(artist_id_requested);
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    no_results_ = true;
    AlbumsFinishCheck(artist_id_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value : json_items) {

    ++albums_received;

    if (!value.isObject()) {
      Error("Invalid Json reply, item not a object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (json_obj.contains("item")) {
      QJsonValue json_item = json_obj["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item not a object.", json_item);
        continue;
      }
      json_obj = json_item.toObject();
    }

    int album_id = 0;
    QString album;
    if (json_obj.contains("type")) {  // This was a albums request or search
      if (!json_obj.contains("id") || !json_obj.contains("title")) {
        Error("Invalid Json reply, item is missing ID or title.", json_obj);
        continue;
      }
      album_id = json_obj["id"].toInt();
      album = json_obj["title"].toString();
    }
    else if (json_obj.contains("album")) {  // This was a tracks request or search
      QJsonValue json_value_album = json_obj["album"];
      if (!json_value_album.isObject()) {
        Error("Invalid Json reply, item album is not a object.", json_value_album);
        continue;
      }
      QJsonObject json_album = json_value_album.toObject();
      if (!json_album.contains("id") || !json_album.contains("title")) {
        Error("Invalid Json reply, item album is missing ID or title.", json_album);
        continue;
      }
      album_id = json_album["id"].toInt();
      album = json_album["title"].toString();

    }
    else {
      Error("Invalid Json reply, item missing type or album.", json_obj);
      continue;
    }

    if (album_songs_requests_pending_.contains(album_id)) continue;

    if (!json_obj.contains("artist") || !json_obj.contains("title") || !json_obj.contains("audioQuality")) {
      Error("Invalid Json reply, item missing artist, title or audioQuality.", json_obj);
      continue;
    }
    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      Error("Invalid Json reply, item artist is not a object.", json_value_artist);
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();
    if (!json_artist.contains("id") || !json_artist.contains("name")) {
      Error("Invalid Json reply, item artist missing id or name.", json_artist);
      continue;
    }

    int artist_id = json_artist["id"].toInt();
    QString artist = json_artist["name"].toString();

    QString quality = json_obj["audioQuality"].toString();
    QString copyright = json_obj["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    Request request;
    if (artist_id_requested == 0) {
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

void TidalRequest::AlbumsFinishCheck(const int artist_id, const int limit, const int offset, const int albums_total, const int albums_received) {

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

    QHash<int, Request> ::iterator i;
    for (i = album_songs_requests_pending_.begin() ; i != album_songs_requests_pending_.end() ; ++i) {
      Request request = i.value();
      AddAlbumSongsRequest(request.artist_id, request.album_id, request.album_artist);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requested_ > 0) {
      if (album_songs_requested_ == 1) emit UpdateStatus(tr("Retrieving songs for %1 album...").arg(album_songs_requested_));
      else emit UpdateStatus(tr("Retrieving songs for %1 albums...").arg(album_songs_requested_));
      emit ProgressSetMaximum(album_songs_requested_);
      emit UpdateProgress(0);
    }
  }

  FinishCheck();

}

void TidalRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  if (type_ == QueryType_SearchSongs && service_->fetchalbums()) {
    AlbumsReceived(reply, 0, limit_requested, offset_requested, (offset_requested == 0));
  }
  else {
    SongsReceived(reply, 0, 0, limit_requested, offset_requested, (offset_requested == 0));
  }

}

void TidalRequest::AddAlbumSongsRequest(const int artist_id, const int album_id, const QString &album_artist, const int offset) {

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

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    Request request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    ParamList parameters;
    if (request.offset > 0) parameters << Param("offset", QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(request.album_id), parameters);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumSongsReplyReceived(QNetworkReply*, int, int, int, QString)), reply, request.artist_id, request.album_id, request.offset, request.album_artist);

  }

}

void TidalRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const int artist_id, const int album_id, const int offset_requested, const QString album_artist) {

  --album_songs_requests_active_;
  ++album_songs_received_;
  if (offset_requested == 0) {
    emit UpdateProgress(album_songs_received_);
  }
  SongsReceived(reply, artist_id, album_id, 0, offset_requested, false, album_artist);

}

void TidalRequest::SongsReceived(QNetworkReply *reply, const int artist_id, const int album_id, const int limit_requested, const int offset_requested, const bool auto_login, const QString album_artist) {

  QString error;
  QByteArray data = GetReplyData(reply, error, auto_login);

  if (finished_) return;

  if (data.isEmpty()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, 0, 0, album_artist);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
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

  QJsonValue json_value = ExtractItems(data, error);
  if (!json_value.isArray()) {
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist);
    return;
  }

  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    no_results_ = true;
    SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, 0, album_artist);
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValue &value : json_items) {

    if (!value.isObject()) {
      Error("Invalid Json reply, track is not a object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (json_obj.contains("item")) {
      QJsonValue json_item = json_obj["item"];
      if (!json_item.isObject()) {
        Error("Invalid Json reply, item not a object.", json_item);
        continue;
      }
      json_obj = json_item.toObject();
    }

    ++songs_received;
    Song song;
    ParseSong(song, json_obj, artist_id, album_id, album_artist);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
    songs << song;
  }

  for (Song &song : songs) {
    if (compilation) song.set_compilation_detected(true);
    if (multidisc) {
      QString album_full(QString("%1 - (Disc %2)").arg(song.album()).arg(song.disc()));
      song.set_album(album_full);
    }
    songs_ << song;
  }

  SongsFinishCheck(artist_id, album_id, limit_requested, offset_requested, songs_total, songs_received, album_artist);

}

void TidalRequest::SongsFinishCheck(const int artist_id, const int album_id, const int limit, const int offset, const int songs_total, const int songs_received, const QString &album_artist) {

  if (finished_) return;

  if (limit == 0 || limit > songs_received) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (type_) {
        case QueryType_Songs:
          AddSongsRequest(offset_next);
          break;
        case QueryType_SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So pass through.
          if (artist_id == 0 && album_id == 0) {
            AddSongsSearchRequest(offset_next);
            break;
          }
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
      service_->cache_album_covers() &&
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

int TidalRequest::ParseSong(Song &song, const QJsonObject &json_obj, const int artist_id_requested, const int album_id_requested, const QString &album_artist) {

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
    return -1;
  }

  QJsonValue json_value_artist = json_obj["artist"];
  QJsonValue json_value_album = json_obj["album"];
  QJsonValue json_duration = json_obj["duration"];
  QJsonArray json_artists = json_obj["artists"].toArray();

  int song_id = json_obj["id"].toInt();

  QString title = json_obj["title"].toString();
  QString urlstr = json_obj["url"].toString();
  int track = json_obj["trackNumber"].toInt();
  int disc = json_obj["volumeNumber"].toInt();
  bool allow_streaming = json_obj["allowStreaming"].toBool();
  bool stream_ready = json_obj["streamReady"].toBool();
  QString copyright = json_obj["copyright"].toString();

  if (!json_value_artist.isObject()) {
    Error("Invalid Json reply, track artist is not a object.", json_value_artist);
    return -1;
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("id") || !json_artist.contains("name")) {
    Error("Invalid Json reply, track artist is missing id or name.", json_artist);
    return -1;
  }
  int artist_id = json_artist["id"].toInt();
  QString artist = json_artist["name"].toString();

  if (!json_value_album.isObject()) {
    Error("Invalid Json reply, track album is not a object.", json_value_album);
    return -1;
  }
  QJsonObject json_album = json_value_album.toObject();
  if (!json_album.contains("id") || !json_album.contains("title") || !json_album.contains("cover")) {
    Error("Invalid Json reply, track album is missing id, title or cover.", json_album);
    return -1;
  }
  int album_id = json_album["id"].toInt();
  if (album_id_requested != 0 && album_id_requested != album_id) {
    Error("Invalid Json reply, track album id is wrong.", json_album);
    return -1;
  }
  QString album = json_album["title"].toString();
  QString cover = json_album["cover"].toString();

  if (!allow_streaming) {
    Warn(QString("Song %1 %2 %3 is not allowStreaming").arg(artist).arg(album).arg(title));
  }

  if (!stream_ready) {
    Warn(QString("Song %1 %2 %3 is not streamReady").arg(artist).arg(album).arg(title));
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(QString::number(song_id));

  QVariant q_duration = json_duration.toVariant();
  quint64 duration = 0;
  if (q_duration.isValid() && (q_duration.type() == QVariant::Int || q_duration.type() == QVariant::Double)) {
    duration = q_duration.toInt() * kNsecPerSec;
  }
  else {
    Error("Invalid duration for song.", json_duration);
    return -1;
  }

  cover = cover.replace("-", "/");
  QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg(service_->coversize()));

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
  song.set_art_automatic(cover_url.toEncoded());
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

  if (album_covers_requested_ == 1) emit UpdateStatus(tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else emit UpdateStatus(tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  emit ProgressSetMaximum(album_covers_requested_);
  emit UpdateProgress(0);

}

void TidalRequest::AddAlbumCoverRequest(Song &song) {

  if (album_covers_requests_sent_.contains(song.album_id())) {
    album_covers_requests_sent_.insertMulti(song.album_id(), &song);
    return;
  }

  album_covers_requests_sent_.insertMulti(song.album_id(), &song);
  ++album_covers_requested_;

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = QUrl(song.art_automatic());

  album_cover_requests_queue_.enqueue(request);

}

void TidalRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);
    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumCoverReceived(QNetworkReply*, int, QUrl)), reply, request.album_id, request.url);

  }

}

void TidalRequest::AlbumCoverReceived(QNetworkReply *reply, const int album_id, const QUrl url) {

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

  emit UpdateProgress(album_covers_received_);

  if (!album_covers_requests_sent_.contains(album_id)) {
    AlbumCoverFinishCheck();
    return;
  }

  QString error;
  if (reply->error() != QNetworkReply::NoError) {
    error = Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    error = Error(QString("Received empty image data for %1").arg(url.toString()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QImage image;
  if (image.loadFromData(data)) {

    QDir dir;
    if (dir.mkpath(service_->CoverCacheDir())) {
      QString filename(service_->CoverCacheDir() + "/" + QString::number(album_id) + "-" + url.fileName());
      if (image.save(filename, "JPG")) {
        while (album_covers_requests_sent_.contains(album_id)) {
          Song *song = album_covers_requests_sent_.take(album_id);
          song->set_art_automatic(filename);
        }
      }
    }

  }
  else {
    album_covers_requests_sent_.remove(album_id);
    error = Error(QString("Error decoding image data from %1").arg(url.toString()));
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
    if (songs_.isEmpty()) {
      if (IsSearch()) {
        if (no_results_) emit ErrorSignal(search_id_, tr("No match"));
        else if (errors_.isEmpty()) emit ErrorSignal(search_id_, tr("Unknown error"));
        else emit ErrorSignal(search_id_, errors_);
      }
      else {
        if (no_results_) emit Results(songs_);
        else if (errors_.isEmpty()) emit ErrorSignal(tr("Unknown error"));
        else emit ErrorSignal(errors_);
      }
    }
    else {
      if (IsSearch()) {
        emit SearchResults(search_id_, songs_);
      }
      else {
        emit Results(songs_);
      }
    }

  }

}

QString TidalRequest::Error(QString error, QVariant debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  if (!error.isEmpty()) {
    errors_ += error;
    errors_ += "<br />";
  }
  FinishCheck();

  return error;

}

void TidalRequest::Warn(QString error, QVariant debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

