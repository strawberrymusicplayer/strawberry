/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QTimer>
#include <QScopeGuard>

#include "constants/timeconstants.h"
#include "utilities/imageutils.h"
#include "utilities/coverutils.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"
#include "spotifyrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMaxConcurrentArtistsRequests = 1;
constexpr int kMaxConcurrentAlbumsRequests = 1;
constexpr int kMaxConcurrentSongsRequests = 1;
constexpr int kMaxConcurrentArtistAlbumsRequests = 1;
constexpr int kMaxConcurrentAlbumSongsRequests = 1;
constexpr int kMaxConcurrentAlbumCoverRequests = 10;
constexpr int kFlushRequestsDelay = 200;
}  // namespace

SpotifyRequest::SpotifyRequest(SpotifyService *service, const SharedPtr<NetworkAccessManager> network, const Type type, QObject *parent)
    : SpotifyBaseRequest(service, network, parent),
      network_(network),
      timer_flush_requests_(new QTimer(this)),
      type_(type),
      fetchalbums_(service->fetchalbums()),
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
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &SpotifyRequest::FlushRequests);

}

SpotifyRequest::~SpotifyRequest() {

  if (timer_flush_requests_->isActive()) {
    timer_flush_requests_->stop();
  }

}

void SpotifyRequest::Process() {

  if (!service_->authenticated()) {
    Q_EMIT UpdateStatus(query_id_, tr("Authenticating..."));
    return;
  }

  switch (type_) {
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

void SpotifyRequest::StartRequests() {

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void SpotifyRequest::FlushRequests() {

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

void SpotifyRequest::Search(const int query_id, const QString &search_text) {

  query_id_ = query_id;
  search_text_ = search_text;

}

void SpotifyRequest::GetArtists() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving artists..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsRequest();

}

void SpotifyRequest::AddArtistsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  artists_requests_queue_.enqueue(request);

  ++artists_requests_total_;

  StartRequests();

}

void SpotifyRequest::FlushArtistsRequests() {

  while (!artists_requests_queue_.isEmpty() && artists_requests_active_ < kMaxConcurrentArtistsRequests) {

    const Request request = artists_requests_queue_.dequeue();

    ParamList parameters = ParamList() << Param(u"type"_s, u"artist"_s);
    if (type_ == Type::SearchArtists) {
      parameters << Param(u"q"_s, search_text_);
    }
    if (request.limit > 0) {
      parameters << Param(u"limit"_s, QString::number(request.limit));
    }
    if (request.offset > 0) {
      parameters << Param(u"offset"_s, QString::number(request.offset));
    }
    QNetworkReply *reply = nullptr;
    if (type_ == Type::FavouriteArtists) {
      reply = CreateRequest(u"me/following"_s, parameters);
    }
    if (type_ == Type::SearchArtists) {
      reply = CreateRequest(u"search"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistsReplyReceived(reply, request.limit, request.offset); });

    ++artists_requests_active_;

  }

}

void SpotifyRequest::GetAlbums() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving albums..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsRequest();

}

void SpotifyRequest::AddAlbumsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);

  ++albums_requests_total_;

  StartRequests();

}

void SpotifyRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    const Request request = albums_requests_queue_.dequeue();

    ParamList parameters;
    if (type_ == Type::SearchAlbums) {
      parameters << Param(u"type"_s, u"album"_s);
      parameters << Param(u"q"_s, search_text_);
    }
    else {
      parameters << Param(u"include_groups"_s, u"album,single"_s);
    }
    if (request.limit > 0) parameters << Param(u"limit"_s, QString::number(request.limit));
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = nullptr;
    if (type_ == Type::FavouriteAlbums) {
      reply = CreateRequest(u"me/albums"_s, parameters);
    }
    if (type_ == Type::SearchAlbums) {
      reply = CreateRequest(u"search"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.limit, request.offset); });

    ++albums_requests_active_;

  }

}

void SpotifyRequest::GetSongs() {

  Q_EMIT UpdateStatus(query_id_, tr("Receiving songs..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsRequest();

}

void SpotifyRequest::AddSongsRequest(const int offset, const int limit) {

  Request request;
  request.limit = limit;
  request.offset = offset;
  songs_requests_queue_.enqueue(request);

  ++songs_requests_total_;

  StartRequests();

}

void SpotifyRequest::FlushSongsRequests() {

  while (!songs_requests_queue_.isEmpty() && songs_requests_active_ < kMaxConcurrentSongsRequests) {

    const Request request = songs_requests_queue_.dequeue();

    ParamList parameters;
    if (type_ == Type::SearchSongs) {
      parameters << Param(u"type"_s, u"track"_s);
      parameters << Param(u"q"_s, search_text_);
    }
    if (request.limit > 0) {
      parameters << Param(u"limit"_s, QString::number(request.limit));
    }
    if (request.offset > 0) {
      parameters << Param(u"offset"_s, QString::number(request.offset));
    }
    QNetworkReply *reply = nullptr;
    if (type_ == Type::FavouriteSongs) {
      reply = CreateRequest(u"me/tracks"_s, parameters);
    }
    if (type_ == Type::SearchSongs) {
      reply = CreateRequest(u"search"_s, parameters);
    }
    if (!reply) continue;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { SongsReplyReceived(reply, request.limit, request.offset); });

    ++songs_requests_active_;

  }

}

void SpotifyRequest::ArtistsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddArtistsSearchRequest();

}

void SpotifyRequest::AddArtistsSearchRequest(const int offset) {

  AddArtistsRequest(offset, service_->artistssearchlimit());

}

void SpotifyRequest::AlbumsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddAlbumsSearchRequest();

}

void SpotifyRequest::AddAlbumsSearchRequest(const int offset) {

  AddAlbumsRequest(offset, service_->albumssearchlimit());

}

void SpotifyRequest::SongsSearch() {

  Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  Q_EMIT UpdateProgress(query_id_, 0);
  AddSongsSearchRequest();

}

void SpotifyRequest::AddSongsSearchRequest(const int offset) {

  AddSongsRequest(offset, service_->songssearchlimit());

}

void SpotifyRequest::ArtistsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

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

  if (!json_object.contains("artists"_L1) || !json_object["artists"_L1].isObject()) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }
  const QJsonObject obj_artists = json_object["artists"_L1].toObject();

  if (!obj_artists.contains("limit"_L1) ||
      !obj_artists.contains("total"_L1) ||
      !obj_artists.contains("items"_L1)) {
    Error(u"Json object missing values."_s, obj_artists);
    return;
  }

  if (obj_artists.contains("offset"_L1)) {
    offset = obj_artists["offset"_L1].toInt();
  }
  const int artists_total = obj_artists["total"_L1].toInt();

  if (offset_requested == 0) {
    artists_total_ = artists_total;
  }
  else if (artists_total != artists_total_) {
    Error(QStringLiteral("Total returned does not match previous total! %1 != %2").arg(artists_total).arg(artists_total_));
    return;
  }

  if (offset != offset_requested) {
    Error(QStringLiteral("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    return;
  }

  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(artists_received_, artists_total_));
  }

  const JsonArrayResult json_array_result = GetJsonArray(obj_artists, u"items"_s);
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
      Error(u"Invalid Json reply, item in array is not a object."_s);
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1)) {
      QJsonValue json_item = object_item["item"_L1];
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

    const QString artist_id = object_item["id"_L1].toString();
    const QString artist = object_item["name"_L1].toString();

    if (artist_albums_requests_pending_.contains(artist_id)) continue;

    ArtistAlbumsRequest request;
    request.artist.artist_id = artist_id;
    request.artist.artist = artist;
    artist_albums_requests_pending_.insert(artist_id, request);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) Q_EMIT UpdateProgress(query_id_, GetProgress(artists_total_, artists_received_));

}

void SpotifyRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if (finished_) return;

  if (artists_received > 0 && (limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (type_ == Type::FavouriteArtists) AddArtistsRequest(offset_next);
      else if (type_ == Type::SearchArtists) AddArtistsSearchRequest(offset_next);
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

void SpotifyRequest::AlbumsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --albums_requests_active_;
  ++albums_requests_received_;
  AlbumsReceived(reply, Artist(), limit_requested, offset_requested);

}

void SpotifyRequest::AddArtistAlbumsRequest(const Artist &artist, const int offset) {

  ArtistAlbumsRequest request;
  request.artist = artist;
  request.offset = offset;
  artist_albums_requests_queue_.enqueue(request);

  ++artist_albums_requests_total_;

  StartRequests();

}

void SpotifyRequest::FlushArtistAlbumsRequests() {

  while (!artist_albums_requests_queue_.isEmpty() && artist_albums_requests_active_ < kMaxConcurrentArtistAlbumsRequests) {

    const ArtistAlbumsRequest request = artist_albums_requests_queue_.dequeue();

    ParamList parameters;
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("artists/%1/albums").arg(request.artist.artist_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ArtistAlbumsReplyReceived(reply, request.artist, request.offset); });

    ++artist_albums_requests_active_;

  }

}

void SpotifyRequest::ArtistAlbumsReplyReceived(QNetworkReply *reply, const Artist &artist, const int offset_requested) {

  --artist_albums_requests_active_;
  ++artist_albums_requests_received_;
  Q_EMIT UpdateProgress(query_id_, GetProgress(artist_albums_requests_received_, artist_albums_requests_total_));
  AlbumsReceived(reply, artist, 0, offset_requested);

}

void SpotifyRequest::AlbumsReceived(QNetworkReply *reply, const Artist &artist_artist, const int limit_requested, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  JsonObjectResult json_object_result = ParseJsonObject(reply);

  if (finished_) return;

  int offset = 0;
  int albums_total = 0;
  int albums_received = 0;
  const QScopeGuard finish_check = qScopeGuard([this, artist_artist, limit_requested, &offset, &albums_total, &albums_received]() { AlbumsFinishCheck(artist_artist, limit_requested, offset, albums_total, albums_received); });

  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (json_object.contains("albums"_L1) && json_object["albums"_L1].isObject()) {
    json_object = json_object["albums"_L1].toObject();
  }

  if (json_object.contains("tracks"_L1) && json_object["tracks"_L1].isObject()) {
    json_object = json_object["tracks"_L1].toObject();
  }

  if (!json_object.contains("limit"_L1) ||
      !json_object.contains("offset"_L1) ||
      !json_object.contains("total"_L1) ||
      !json_object.contains("items"_L1)) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }

  offset = json_object["offset"_L1].toInt();
  albums_total = json_object["total"_L1].toInt();

  if (type_ == Type::FavouriteAlbums || type_ == Type::SearchAlbums) {
    albums_total_ = albums_total;
  }

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
    if ((type_ == Type::FavouriteAlbums || type_ == Type::SearchAlbums || (type_ == Type::SearchSongs && fetchalbums_)) && offset_requested == 0) {
      no_results_ = true;
    }
    return;
  }

  albums_received = static_cast<int>(array_items.count());

  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item in array is not a object."_s);
      continue;
    }
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1)) {
      QJsonValue json_item = object_item["item"_L1];
      if (!json_item.isObject()) {
        Error(u"Invalid Json reply, item in array is not a object."_s, json_item);
        continue;
      }
      object_item = json_item.toObject();
    }

    if (object_item.contains("album"_L1)) {
      QJsonValue json_item = object_item["album"_L1];
      if (!json_item.isObject()) {
        Error(u"Invalid Json reply, album in array is not a object."_s, json_item);
        continue;
      }
      object_item = json_item.toObject();
    }

    Artist artist;
    Album album;

    if (!object_item.contains("id"_L1)) {
      Error(u"Invalid Json reply, item is missing ID."_s, object_item);
      continue;
    }
    if (!object_item.contains("name"_L1)) {
      Error(u"Invalid Json reply, item is missing name."_s, object_item);
      continue;
    }
    if (!object_item.contains("images"_L1)) {
      Error(u"Invalid Json reply, item is missing images."_s, object_item);
      continue;
    }
    album.album_id = object_item["id"_L1].toString();
    album.album = object_item["name"_L1].toString();

    if (object_item.contains("artists"_L1) && object_item["artists"_L1].isArray()) {
      const QJsonArray array_artists = object_item["artists"_L1].toArray();
      bool artist_matches = false;
      for (const QJsonValue &value : array_artists) {
        if (!value.isObject()) {
          continue;
        }
        QJsonObject obj_artist = value.toObject();
        if (obj_artist.isEmpty() || !obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) continue;
        if (artist.artist_id.isEmpty() || artist.artist_id == artist_artist.artist_id) {
          artist.artist_id = obj_artist["id"_L1].toString();
          artist.artist = obj_artist["name"_L1].toString();
          if (artist.artist_id == artist_artist.artist_id) {
            artist_matches = true;
            break;
          }
        }
      }
      if (!artist_matches && (type_ == Type::FavouriteArtists || type_ == Type::SearchArtists)) {
        return;
      }
    }

    if (artist.artist_id.isEmpty()) {
      artist = artist_artist;
    }

    if (object_item.contains("images"_L1) && object_item["images"_L1].isArray()) {
      const QJsonArray array_images = object_item["images"_L1].toArray();
      for (const QJsonValue &value : array_images) {
        if (!value.isObject()) {
          continue;
        }
        QJsonObject obj_image = value.toObject();
        if (obj_image.isEmpty() || !obj_image.contains("url"_L1) || !obj_image.contains("width"_L1) || !obj_image.contains("height"_L1)) continue;
        int width = obj_image["width"_L1].toInt();
        int height = obj_image["height"_L1].toInt();
        if (width <= 300 || height <= 300) {
          continue;
        }
        album.cover_url = QUrl(obj_image["url"_L1].toString());
      }
    }

    if (object_item.contains("tracks"_L1) && object_item["tracks"_L1].isObject()) {
      QJsonObject obj_tracks = object_item["tracks"_L1].toObject();
      if (obj_tracks.contains("items"_L1) && obj_tracks["items"_L1].isArray()) {
        const QJsonArray array_tracks = obj_tracks["items"_L1].toArray();
        bool compilation = false;
        bool multidisc = false;
        SongList songs;
        for (const QJsonValue &value : array_tracks) {
          if (!value.isObject()) {
            continue;
          }
          QJsonObject obj_track = value.toObject();
          if (obj_track.contains("track"_L1) && obj_track["track"_L1].isObject()) {
            obj_track = obj_track["track"_L1].toObject();
          }
          Song song(Song::Source::Spotify);
          ParseSong(song, obj_track, artist, album);
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
      }
    }
    else if (!album_songs_requests_pending_.contains(album.album_id)) {
      AlbumSongsRequest request;
      request.artist = artist;
      request.album = album;
      album_songs_requests_pending_.insert(album.album_id, request);
    }

  }

  if (type_ == Type::FavouriteAlbums || type_ == Type::SearchAlbums) {
    albums_received_ += albums_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(albums_received_, albums_total_));
  }

}

void SpotifyRequest::AlbumsFinishCheck(const Artist &artist, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (finished_) return;

  if (albums_received > 0 && (limit == 0 || limit > albums_received)) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      switch (type_) {
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

    for (QMap<QString, AlbumSongsRequest> ::const_iterator it = album_songs_requests_pending_.constBegin(); it != album_songs_requests_pending_.constEnd(); ++it) {
      AlbumSongsRequest request = it.value();
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

void SpotifyRequest::SongsReplyReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  --songs_requests_active_;
  ++songs_requests_received_;
  if (type_ == Type::SearchSongs && fetchalbums_) {
    AlbumsReceived(reply, Artist(), limit_requested, offset_requested);
  }
  else {
    SongsReceived(reply, Artist(), Album(), limit_requested, offset_requested);
  }

}

void SpotifyRequest::AddAlbumSongsRequest(const Artist &artist, const Album &album, const int offset) {

  AlbumSongsRequest request;
  request.artist = artist;
  request.album = album;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);

  ++album_songs_requests_total_;

  StartRequests();

}

void SpotifyRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {
    const AlbumSongsRequest request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    ParamList parameters;
    if (request.offset > 0) parameters << Param(u"offset"_s, QString::number(request.offset));
    QNetworkReply *reply = CreateRequest(QStringLiteral("albums/%1/tracks").arg(request.album.album_id), parameters);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist, request.album, request.offset); });
  }

}

void SpotifyRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int offset_requested) {

  --album_songs_requests_active_;
  ++album_songs_requests_received_;
  if (offset_requested == 0) {
    Q_EMIT UpdateProgress(query_id_, GetProgress(album_songs_requests_received_, album_songs_requests_total_));
  }
  SongsReceived(reply, artist, album, 0, offset_requested);

}

void SpotifyRequest::SongsReceived(QNetworkReply *reply, const Artist &artist, const Album &album, const int limit_requested, const int offset_requested) {

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

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (json_object.contains("tracks"_L1) && json_object["tracks"_L1].isObject()) {
    json_object = json_object["tracks"_L1].toObject();
  }

  if (!json_object.contains("limit"_L1) ||
      !json_object.contains("offset"_L1) ||
      !json_object.contains("total"_L1) ||
      !json_object.contains("items"_L1)) {
    Error(u"Json object missing values."_s, json_object);
    return;
  }

  const int offset = json_object["offset"_L1].toInt();
  songs_total = json_object["total"_L1].toInt();

  if (type_ == Type::FavouriteSongs || type_ == Type::SearchSongs) {
    songs_total_ = songs_total;
  }

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
    if ((type_ == Type::FavouriteSongs || type_ == Type::SearchSongs) && offset_requested == 0) {
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
    QJsonObject object_item = value_item.toObject();

    if (object_item.contains("item"_L1) && object_item["item"_L1].isObject()) {
      object_item = object_item["item"_L1].toObject();
    }

    if (object_item.contains("track"_L1) && object_item["track"_L1].isObject()) {
      object_item = object_item["track"_L1].toObject();
    }

    ++songs_received;
    Song song(Song::Source::Spotify);
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

  if (type_ == Type::FavouriteSongs || type_ == Type::SearchSongs) {
    songs_received_ += songs_received;
    Q_EMIT UpdateProgress(query_id_, GetProgress(songs_received_, songs_total_));
  }

}

void SpotifyRequest::SongsFinishCheck(const Artist &artist, const Album &album, const int limit, const int offset, const int songs_total, const int songs_received) {

  if (finished_) return;

  if (songs_received > 0 && (limit == 0 || limit > songs_received)) {
    int offset_next = offset + songs_received;
    if (offset_next > 0 && offset_next < songs_total) {
      switch (type_) {
        case Type::FavouriteSongs:
          AddSongsRequest(offset_next);
          break;
        case Type::SearchSongs:
          // If artist_id and album_id isn't zero it means that it's a songs search where we fetch all albums too. So fallthrough.
          if (artist.artist_id.isEmpty() && album.album_id.isEmpty()) {
            AddSongsSearchRequest(offset_next);
            break;
          }
          // fallthrough
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

void SpotifyRequest::ParseSong(Song &song, const QJsonObject &json_obj, const Artist &album_artist, const Album &album) {

  if (
      !json_obj.contains("type"_L1) ||
      !json_obj.contains("id"_L1) ||
      !json_obj.contains("name"_L1) ||
      !json_obj.contains("uri"_L1) ||
      !json_obj.contains("duration_ms"_L1) ||
      !json_obj.contains("track_number"_L1) ||
      !json_obj.contains("disc_number"_L1)
    ) {
    Error(u"Invalid Json reply, track is missing one or more values."_s, json_obj);
    return;
  }

  QString artist_id;
  QString artist_title;
  if (json_obj.contains("artists"_L1) && json_obj["artists"_L1].isArray()) {
    const QJsonArray array_artists = json_obj["artists"_L1].toArray();
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
       QJsonObject obj_artist = value_artist.toObject();
       if (!obj_artist.contains("type"_L1) || !obj_artist.contains("id"_L1) || !obj_artist.contains("name"_L1)) {
         continue;
       }
       artist_id = obj_artist["id"_L1].toString();
       artist_title = obj_artist["name"_L1].toString();
       break;
    }
  }

  QString album_id;
  QString album_title;
  QUrl cover_url;
  if (json_obj.contains("album"_L1) && json_obj["album"_L1].isObject()) {
    QJsonObject obj_album = json_obj["album"_L1].toObject();
    if (obj_album.contains("type"_L1) && obj_album.contains("id"_L1) && obj_album.contains("name"_L1)) {
      album_id = obj_album["id"_L1].toString();
      album_title = obj_album["name"_L1].toString();
      if (obj_album.contains("images"_L1) && obj_album["images"_L1].isArray()) {
        const QJsonArray array_images = obj_album["images"_L1].toArray();
        for (const QJsonValue &value : array_images) {
          if (!value.isObject()) {
            continue;
          }
          QJsonObject obj_image = value.toObject();
          if (obj_image.isEmpty() || !obj_image.contains("url"_L1) || !obj_image.contains("width"_L1) || !obj_image.contains("height"_L1)) continue;
          int width = obj_image["width"_L1].toInt();
          int height = obj_image["height"_L1].toInt();
          if (width <= 300 || height <= 300) {
            continue;
          }
          cover_url = QUrl(obj_image["url"_L1].toString());
        }
      }
    }
  }

  if (artist_id.isEmpty() || artist_title.isEmpty()) {
    artist_id = album_artist.artist_id;
    artist_title = album_artist.artist;
  }

  if (album_id.isEmpty() || album_title.isEmpty() || cover_url.isEmpty()) {
    album_id = album.album_id;
    album_title = album.album;
    cover_url = album.cover_url;
  }

  QString song_id = json_obj["id"_L1].toString();
  QString title = json_obj["name"_L1].toString();
  QString uri = json_obj["uri"_L1].toString();
  qint64 duration = json_obj["duration_ms"_L1].toVariant().toLongLong() * kNsecPerMsec;
  int track = json_obj["track_number"_L1].toInt();
  int disc = json_obj["disc_number"_L1].toInt();

  QUrl url(uri);

  title = Song::TitleRemoveMisc(title);

  song.set_source(Song::Source::Spotify);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  if (album_artist.artist != artist_title) {
    song.set_albumartist(album_artist.artist);
  }
  song.set_album(album_title);
  song.set_artist(artist_title);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);
  song.set_url(url);
  song.set_length_nanosec(duration);
  song.set_art_automatic(cover_url);
  song.set_directory_id(0);
  song.set_filetype(Song::FileType::Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

}

void SpotifyRequest::GetAlbumCoversCheck() {

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

void SpotifyRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    AddAlbumCoverRequest(song);
  }

  if (album_covers_requests_total_ == 1) Q_EMIT UpdateStatus(query_id_, tr("Receiving album cover for %1 album...").arg(album_covers_requests_total_));
  else Q_EMIT UpdateStatus(query_id_, tr("Receiving album covers for %1 albums...").arg(album_covers_requests_total_));
  Q_EMIT UpdateProgress(query_id_, 0);

  StartRequests();

}

void SpotifyRequest::AddAlbumCoverRequest(const Song &song) {

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

void SpotifyRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {
    const AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    QNetworkReply *reply = CreateGetRequest(request.url);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request.album_id, request.url, request.filename); });
    ++album_covers_requests_active_;
  }

}

void SpotifyRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

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

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(url.toString()));
    if (album_covers_requests_sent_.contains(album_id)) album_covers_requests_sent_.remove(album_id);
    return;
  }

  QList<QByteArray> format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
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

void SpotifyRequest::AlbumCoverFinishCheck() {

  FinishCheck();

}

void SpotifyRequest::FinishCheck() {

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
        Q_EMIT Results(query_id_, songs_, tr("Data missing error"));
      }
      else {
        Q_EMIT Results(query_id_, songs_, error_);
      }
    }
  }

}

int SpotifyRequest::GetProgress(const int count, const int total) {

  return static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0F);

}

void SpotifyRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "Spotify:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = error_message;

}

void SpotifyRequest::Warn(const QString &error_message, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error_message;
  if (debug.isValid()) qLog(Debug) << debug;

}
