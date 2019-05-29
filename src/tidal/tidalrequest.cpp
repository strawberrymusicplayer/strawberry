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

TidalRequest::TidalRequest(TidalService *service, TidalUrlHandler *url_handler, NetworkAccessManager *network, QueryType type, QObject *parent)
    : TidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      network_(network),
      type_(type),
      search_id_(-1),
      artists_total_(0),
      artists_chunk_requested_(0),
      artists_chunk_received_(0),
      artists_received_(0),
      artist_albums_chunk_requested_(0),
      artist_albums_chunk_received_(0),
      artist_albums_received_(0),
      album_songs_received_(0),
      album_covers_requested_(0),
      album_covers_received_(0),
      need_login_(false),
      no_results_(false) {}

TidalRequest::~TidalRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    reply->abort();
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

void TidalRequest::GetArtists(const int offset) {

  if (offset == 0) emit UpdateStatus(tr("Retrieving artists..."));

  ++artists_chunk_requested_;

  ParamList parameters;
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest(QString("users/%1/favorites/artists").arg(service_->user_id()), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(ArtistsReceived(QNetworkReply*, int, int)), reply, 0, offset);

}

void TidalRequest::GetAlbums(const int offset) {

  if (offset == 0) emit UpdateStatus(tr("Retrieving albums..."));

  if (!service_->authenticated()) {
    need_login_ = true;
    return;
  }

  ++artist_albums_chunk_requested_;

  ParamList parameters;
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest(QString("users/%1/favorites/albums").arg(service_->user_id()), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int, int)), reply, 0, 0, offset);

}

void TidalRequest::GetSongs(const int offset) {

  if (offset == 0) emit UpdateStatus(tr("Retrieving songs..."));

  if (!service_->authenticated()) {
    need_login_ = true;
    return;
  }

  ParamList parameters;
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest(QString("users/%1/favorites/tracks").arg(service_->user_id()), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SongsReceived(QNetworkReply*, int, int)), reply, 0, offset);

}

void TidalRequest::ArtistsSearch(const int offset) {

  if (!service_->authenticated()) {
    need_login_ = true;
    return;
  }

  if (offset == 0) emit UpdateStatus(tr("Searching..."));

  ++artists_chunk_requested_;

  int limit = service_->artistssearchlimit();

  ParamList parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(limit));
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest("search/artists", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(ArtistsReceived(QNetworkReply*, int, int)), reply, limit, offset);

}

void TidalRequest::AlbumsSearch(const int offset) {

  if (!service_->authenticated()) {
    need_login_ = true;
    return;
  }

  if (offset == 0) emit UpdateStatus(tr("Searching..."));

  ++artist_albums_chunk_requested_;

  int limit = service_->albumssearchlimit();

  ParamList parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(limit));
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest("search/albums", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int, int)), reply, 0, limit, offset);

}

void TidalRequest::SongsSearch(const int offset) {

  if (!service_->authenticated()) {
    need_login_ = true;
    return;
  }

  if (offset == 0) emit UpdateStatus(tr("Searching..."));

  int limit = service_->songssearchlimit();

  ParamList parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(limit));
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest("search/tracks", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int, int)), reply, 0, limit, offset);

}

void TidalRequest::ArtistsReceived(QNetworkReply *reply, const int limit_requested, const int offset_requested) {

  QString error;
  QByteArray data = GetReplyData(reply, error, (offset_requested == 0));

  ++artists_chunk_received_;

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
      qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (json_obj.contains("item")) {
      QJsonValue json_item = json_obj["item"];
      if (!json_item.isObject()) {
        qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
        qLog(Debug) << json_item;
        continue;
      }
      json_obj = json_item.toObject();
    }

    if (!json_obj.contains("id") || !json_obj.contains("name")) {
      qLog(Error) << "Tidal: Invalid Json reply, item missing id or album.";
      qLog(Debug) << json_obj;
      continue;
    }

    int artist_id = json_obj["id"].toInt();
    if (artist_albums_queue_.contains(artist_id)) continue;
    artist_albums_queue_.append(artist_id);

  }
  artists_received_ += artists_received;

  if (offset_requested != 0) emit UpdateProgress(artists_received_);

  ArtistsFinishCheck(limit_requested, offset, artists_received);

}

void TidalRequest::ArtistsFinishCheck(const int limit, const int offset, const int artists_received) {

  if ((limit == 0 || limit > artists_received) && artists_received_ < artists_total_) {
    int offset_next = offset + artists_received;
    if (offset_next > 0 && offset_next < artists_total_) {
      if (IsQuery()) GetArtists(offset_next);
      else ArtistsSearch(offset_next);
    }
  }

  if (artists_chunk_received_ >= artists_chunk_requested_) {  // Artist query is finished, get all albums for all artists.

    // Get artist albums
    for (int artist_id : artist_albums_queue_) {
      GetArtistAlbums(artist_id);
    }
    artist_albums_queue_.clear();

    if (artist_albums_requests_.count() > 0) {
      if (artist_albums_requests_.count() == 1) emit UpdateStatus(tr("Retrieving albums for %1 artist...").arg(artist_albums_requests_.count()));
      else emit UpdateStatus(tr("Retrieving albums for %1 artists...").arg(artist_albums_requests_.count()));
      emit ProgressSetMaximum(artist_albums_requests_.count());
      emit UpdateProgress(0);
    }

  }

  FinishCheck();

}

void TidalRequest::GetArtistAlbums(const int artist_id, const int offset) {

  if (offset == 0) artist_albums_requests_.append(artist_id);

  ++artist_albums_chunk_requested_;

  ParamList parameters;
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest(QString("artists/%1/albums").arg(artist_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int, int)), reply, artist_id, 0, offset);

}

void TidalRequest::AlbumsReceived(QNetworkReply *reply, const int artist_id, const int limit_requested, const int offset_requested) {

  QString error;
  QByteArray data = GetReplyData(reply, error, (artist_id == 0));

  ++artist_albums_chunk_received_;

  if (artist_id != 0) {
    if (!artist_albums_requests_.contains(artist_id)) return;
    ++artist_albums_received_;
    emit UpdateProgress(artist_albums_received_);
  }

  if (data.isEmpty()) {
    AlbumsFinishCheck(artist_id);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    AlbumsFinishCheck(artist_id);
    return;
  }

  if (!json_obj.contains("limit") ||
      !json_obj.contains("offset") ||
      !json_obj.contains("totalNumberOfItems") ||
      !json_obj.contains("items")) {
    Error("Json object missing values.", json_obj);
    AlbumsFinishCheck(artist_id);
    return;
  }

  //int limit = json_obj["limit"].toInt();
  int offset = json_obj["offset"].toInt();
  int albums_total = json_obj["totalNumberOfItems"].toInt();

  if (offset != offset_requested) {
    Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
    AlbumsFinishCheck(artist_id);
    return;
  }

  QJsonValue json_value = ExtractItems(json_obj, error);
  if (!json_value.isArray()) {
    AlbumsFinishCheck(artist_id);
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    no_results_ = true;
    AlbumsFinishCheck(artist_id);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value : json_items) {

    ++albums_received;

    if (!value.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (json_obj.contains("item")) {
      QJsonValue json_item = json_obj["item"];
      if (!json_item.isObject()) {
        qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
        qLog(Debug) << json_item;
        continue;
      }
      json_obj = json_item.toObject();
    }

    int album_id = 0;
    QString album;
    if (json_obj.contains("type")) {  // This was a albums request or search
      if (!json_obj.contains("id") || !json_obj.contains("title")) {
        qLog(Error) << "Tidal: Invalid Json reply, item is missing ID or title.";
        qLog(Debug) << json_obj;
        continue;
      }
      album_id = json_obj["id"].toInt();
      album = json_obj["title"].toString();
    }
    else if (json_obj.contains("album")) {  // This was a tracks request or search
      if (!service_->fetchalbums()) {
        Song song;
        ParseSong(song, 0, value);
        songs_ << song;
        continue;
      }
      QJsonValue json_value_album = json_obj["album"];
      if (!json_value_album.isObject()) {
        qLog(Error) << "Tidal: Invalid Json reply, item album is not a object.";
        qLog(Debug) << json_value_album;
        continue;
      }
      QJsonObject json_album = json_value_album.toObject();
      if (!json_album.contains("id") || !json_album.contains("title")) {
        qLog(Error) << "Tidal: Invalid Json reply, item album is missing ID or title.";
        qLog(Debug) << json_album;
        continue;
      }
      album_id = json_album["id"].toInt();
      album = json_album["title"].toString();

    }
    else {
      qLog(Error) << "Tidal: Invalid Json reply, item missing type or album.";
      qLog(Debug) << json_obj;
      continue;
    }

    if (album_songs_requests_.contains(album_id)) continue;

    if (!json_obj.contains("artist") || !json_obj.contains("title") || !json_obj.contains("audioQuality")) {
      qLog(Error) << "Tidal: Invalid Json reply, item missing artist, title or audioQuality.";
      qLog(Debug) << json_obj;
      continue;
    }
    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item artist is not a object.";
      qLog(Debug) << json_value_artist;
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();
    if (!json_artist.contains("name")) {
      qLog(Error) << "Tidal: Invalid Json reply, item artist missing name.";
      qLog(Debug) << json_artist;
      continue;
    }
    QString artist = json_artist["name"].toString();

    QString quality = json_obj["audioQuality"].toString();
    QString copyright = json_obj["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    album_songs_requests_.insert(album_id, artist);

  }

  AlbumsFinishCheck(artist_id, limit_requested, offset, albums_total, albums_received);

}

void TidalRequest::AlbumsFinishCheck(const int artist_id, const int limit, const int offset, const int albums_total, const int albums_received) {

  if (limit == 0 || limit > albums_received) {
    int offset_next = offset + albums_received;
    if (offset_next > 0 && offset_next < albums_total) {
      if (type_ == QueryType_Albums) {
        GetAlbums(offset_next);
      }
      else if (type_ == QueryType_SearchAlbums) {
        AlbumsSearch(offset_next);
      }
      else if (type_ == QueryType_SearchSongs) {
        SongsSearch(offset_next);
      }
      else {
        GetArtistAlbums(artist_id, offset_next);
      }
    }
  }

  if (artist_albums_chunk_received_ >= artist_albums_chunk_requested_) {
    // Get songs for the albums.
    QHashIterator<int, QString> i(album_songs_requests_);
    while (i.hasNext()) {
      i.next();
      GetAlbumSongs(i.key());
    }

    if (album_songs_requests_.count() > 0) {
      if (album_songs_requests_.count() == 1) emit UpdateStatus(tr("Retrieving songs for %1 album...").arg(album_songs_requests_.count()));
      else emit UpdateStatus(tr("Retrieving songs for %1 albums...").arg(album_songs_requests_.count()));
      emit ProgressSetMaximum(album_songs_requests_.count());
      emit UpdateProgress(0);
    }
  }

  FinishCheck();

}

void TidalRequest::GetAlbumSongs(const int album_id) {

  ParamList parameters;
  QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(album_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SongsReceived(QNetworkReply*, int)), reply, album_id);

}

void TidalRequest::SongsReceived(QNetworkReply *reply, const int album_id) {

  QString error;
  QByteArray data = GetReplyData(reply, error, false);

  QString album_artist;
  if (album_id != 0) {
    if (!album_songs_requests_.contains(album_id)) return;
    album_artist = album_songs_requests_[album_id];
  }

  ++album_songs_received_;
  emit UpdateProgress(album_songs_received_);

  if (data.isEmpty()) {
    FinishCheck();
    return;
  }

  QJsonValue json_value = ExtractItems(data, error);
  if (!json_value.isArray()) {
    FinishCheck();
    return;
  }

  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    no_results_ = true;
    FinishCheck();
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  for (const QJsonValue &value : json_items) {
    Song song;
    ParseSong(song, album_id, value, album_artist);
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

  if (
      service_->cache_album_covers() &&
      artists_total_ <= artists_received_ &&
      artist_albums_requests_.count() <= artist_albums_received_ &&
      album_songs_requests_.count() <= album_songs_received_
  ) {
    GetAlbumCovers();
  }

  FinishCheck();

}

int TidalRequest::ParseSong(Song &song, const int album_id_requested, const QJsonValue &value, QString album_artist) {

  if (!value.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track is not a object.";
    qLog(Debug) << value;
    return -1;
  }
  QJsonObject json_obj = value.toObject();

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
    qLog(Error) << "Tidal: Invalid Json reply, track is missing one or more values.";
    qLog(Debug) << json_obj;
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
    qLog(Error) << "Tidal: Invalid Json reply, track artist is not a object.";
    qLog(Debug) << json_value_artist;
    return -1;
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("name")) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is missing name.";
    qLog(Debug) << json_artist;
    return -1;
  }
  QString artist = json_artist["name"].toString();

  if (!json_value_album.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is not a object.";
    qLog(Debug) << json_value_album;
    return -1;
  }
  QJsonObject json_album = json_value_album.toObject();
  if (!json_album.contains("id") || !json_album.contains("title") || !json_album.contains("cover")) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is missing id, title or cover.";
    qLog(Debug) << json_album;
    return -1;
  }
  int album_id = json_album["id"].toInt();
  if (album_id_requested != 0 && album_id_requested != album_id) {
    qLog(Error) << "Tidal: Invalid Json reply, track album id is wrong.";
    qLog(Debug) << json_album;
    return -1;
  }
  QString album = json_album["title"].toString();
  QString cover = json_album["cover"].toString();

  if (!allow_streaming) {
    qLog(Error) << "Tidal: Song" << artist << album << title << "is not allowStreaming";
  }

  if (!stream_ready) {
    qLog(Error) << "Tidal: Song" << artist << album << title << "is not streamReady.";
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
    qLog(Error) << "Tidal: Invalid duration for song.";
    qLog(Debug) << json_duration;
    return -1;
  }

  cover = cover.replace("-", "/");
  QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg(service_->coversize()));

  title.remove(Song::kTitleRemoveMisc);

  //qLog(Debug) << "id" << song_id << "track" << track << "disc" << disc << "title" << title << "album" << album << "album artist" << album_artist << "artist" << artist << cover << allow_streaming << url;

  song.set_source(Song::Source_Tidal);
  song.set_album_id(album_id);
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
    GetAlbumCover(song);
  }

  if (album_covers_requested_ == 1) emit UpdateStatus(tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else emit UpdateStatus(tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  emit ProgressSetMaximum(album_covers_requested_);
  emit UpdateProgress(0);

}

void TidalRequest::GetAlbumCover(Song &song) {

  if (album_covers_requests_.contains(song.album_id())) {
    album_covers_requests_.insertMulti(song.album_id(), &song);
    return;
  }

  album_covers_requests_.insertMulti(song.album_id(), &song);
  ++album_covers_requested_;

  QUrl url(song.art_automatic());
  QNetworkRequest req(url);
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumCoverReceived(QNetworkReply*, int, QUrl)), reply, song.album_id(), url);

}

void TidalRequest::AlbumCoverReceived(QNetworkReply *reply, const int album_id, const QUrl url) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
    reply->deleteLater();
  }
  else {
    FinishCheck();
    return;
  }

  if (!album_covers_requests_.contains(album_id)) {
    FinishCheck();
    return;
  }

  ++album_covers_received_;
  emit UpdateProgress(album_covers_received_);

  QString error;
  if (reply->error() != QNetworkReply::NoError) {
    error = Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_.remove(album_id);
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    error = Error(QString("Received empty image data for %1").arg(url.toString()));
    album_covers_requests_.remove(album_id);
    return;
  }

  QImage image;
  if (image.loadFromData(data)) {

    QDir dir;
    if (dir.mkpath(service_->CoverCacheDir())) {
      QString filename(service_->CoverCacheDir() + "/" + QString::number(album_id) + "-" + url.fileName());
      if (image.save(filename, "JPG")) {
        while (album_covers_requests_.contains(album_id)) {
          Song *song = album_covers_requests_.take(album_id);
          song->set_art_automatic(filename);
        }
      }
    }

  }
  else {
    error = Error(QString("Error decoding image data from %1").arg(url.toString()));
  }

  FinishCheck();

}

void TidalRequest::FinishCheck() {

  if (
      !need_login_ &&
      artists_chunk_requested_ <= artists_chunk_received_ &&
      artist_albums_chunk_requested_ <= artist_albums_chunk_received_ &&
      artist_albums_requests_.count() <= artist_albums_received_ &&
      album_songs_requests_.count() <= album_songs_received_ &&
      album_covers_requested_ <= album_covers_received_
  ) {
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
