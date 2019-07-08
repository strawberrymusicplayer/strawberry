/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <assert.h>

#include <QObject>
#include <QByteArray>
#include <QDir>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QNetworkReply>
#include <QSslError>
#include <QSslConfiguration>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMimeDatabase>

#include "core/application.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "covermanager/albumcoverloader.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicrequest.h"

const int SubsonicRequest::kMaxConcurrentAlbumsRequests = 3;
const int SubsonicRequest::kMaxConcurrentAlbumSongsRequests = 3;
const int SubsonicRequest::kMaxConcurrentAlbumCoverRequests = 1;

SubsonicRequest::SubsonicRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, Application *app, NetworkAccessManager *network, QObject *parent)
    : SubsonicBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
      network_(network),
      finished_(false),
      albums_requests_active_(0),
      album_songs_requests_active_(0),
      album_songs_requested_(0),
      album_songs_received_(0),
      album_covers_requests_active_(0),
      album_covers_requested_(0),
      album_covers_received_(0),
      no_results_(false)
      {}

SubsonicRequest::~SubsonicRequest() {

  while (!album_cover_replies_.isEmpty()) {
    QNetworkReply *reply = album_cover_replies_.takeFirst();
    disconnect(reply, 0, nullptr, 0);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void SubsonicRequest::Reset() {

  finished_ = false;

  albums_requests_queue_.clear();
  album_songs_requests_queue_.clear();
  album_cover_requests_queue_.clear();
  album_songs_requests_pending_.clear();
  album_covers_requests_sent_.clear();

  albums_requests_active_ = 0;
  album_songs_requests_active_ = 0;
  album_songs_requested_ = 0;
  album_songs_received_ = 0;
  album_covers_requests_active_ = 0;
  album_covers_requested_ = 0;
  album_covers_received_ = 0;

  songs_.clear();
  errors_.clear();
  no_results_ = false;
  album_cover_replies_.clear();

}

void SubsonicRequest::GetAlbums() {

  emit UpdateStatus(tr("Retrieving albums..."));
  emit UpdateProgress(0);
  AddAlbumsRequest();

}

void SubsonicRequest::AddAlbumsRequest(const int offset, const int size) {

  Request request;
  request.size = size;
  request.offset = offset;
  albums_requests_queue_.enqueue(request);
  if (albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();

}

void SubsonicRequest::FlushAlbumsRequests() {

  while (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) {

    Request request = albums_requests_queue_.dequeue();
    ++albums_requests_active_;

    ParamList params = ParamList() << Param("type", "alphabeticalByName");
    if (request.size > 0) params << Param("size", QString::number(request.size));
    if (request.offset > 0) params << Param("offset", QString::number(request.offset));

    QNetworkReply *reply;
    reply = CreateGetRequest(QString("getAlbumList2"), params);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReplyReceived(QNetworkReply*, int)), reply, request.offset);

  }

}

void SubsonicRequest::AlbumsReplyReceived(QNetworkReply *reply, const int offset_requested) {

  --albums_requests_active_;

  QByteArray data = GetReplyData(reply);

  if (finished_) return;

  if (data.isEmpty()) {
    AlbumsFinishCheck(offset_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    AlbumsFinishCheck(offset_requested);
    return;
  }

  if (json_obj.contains("error")) {
    QJsonValue json_error = json_obj["error"];
    if (!json_error.isObject()) {
      Error("Json error is not an object.", json_obj);
      AlbumsFinishCheck(offset_requested);
      return;
    }
    json_obj = json_error.toObject();
    if (!json_obj.isEmpty() && json_obj.contains("code") && json_obj.contains("message")) {
      int code = json_obj["code"].toInt();
      QString message = json_obj["message"].toString();
      Error(QString("%1 (%2)").arg(message).arg(code));
      AlbumsFinishCheck(offset_requested);
    }
    else {
      Error("Json error object missing code or message.", json_obj);
      AlbumsFinishCheck(offset_requested);
      return;
    }
    return;
  }

  if (!json_obj.contains("albumList") && !json_obj.contains("albumList2")) {
    Error("Json reply is missing albumList.", json_obj);
    AlbumsFinishCheck(offset_requested);
    return;
  }
  QJsonValue json_albumlist;
  if (json_obj.contains("albumList")) json_albumlist = json_obj["albumList"];
  else if (json_obj.contains("albumList2")) json_albumlist = json_obj["albumList2"];

  if (!json_albumlist.isObject()) {
    Error("Json album list is not an object.", json_albumlist);
    AlbumsFinishCheck(offset_requested);
  }
  json_obj = json_albumlist.toObject();
  if (json_obj.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested);
    return;
  }

  if (!json_obj.contains("album")) {
    Error("Json album list does not contain album array.", json_obj);
    AlbumsFinishCheck(offset_requested);
  }
  QJsonValue json_album = json_obj["album"];
  if (json_album.isNull()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested);
    return;
  }
  if (!json_album.isArray()) {
    Error("Json album is not an array.", json_album);
    AlbumsFinishCheck(offset_requested);
  }
  QJsonArray json_albums = json_album.toArray();

  if (json_albums.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value : json_albums) {

    ++albums_received;

    if (!value.isObject()) {
      Error("Invalid Json reply, album is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (!json_obj.contains("id") || !json_obj.contains("artist")) {
      Error("Invalid Json reply, album object is missing ID or artist.", json_obj);
      continue;
    }

    if (!json_obj.contains("album") && !json_obj.contains("name")) {
      Error("Invalid Json reply, album object is missing album or name.", json_obj);
      continue;
    }

    qint64 album_id = json_obj["id"].toString().toLongLong();
    QString artist = json_obj["artist"].toString();
    QString album;
    if (json_obj.contains("album")) album = json_obj["album"].toString();
    else if (json_obj.contains("name")) album = json_obj["name"].toString();

    if (album_songs_requests_pending_.contains(album_id)) continue;

    Request request;
    request.album_id = album_id;
    request.album_artist = artist;
    album_songs_requests_pending_.insert(album_id, request);

  }

  AlbumsFinishCheck(offset_requested, albums_received);

}

void SubsonicRequest::AlbumsFinishCheck(const int offset, const int albums_received) {

  if (finished_) return;

  if (albums_received > 0) {
    int offset_next = offset + albums_received;
    if (offset_next > 0) {
      AddAlbumsRequest(offset_next);
    }
  }

  if (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();

  if (albums_requests_queue_.isEmpty() && albums_requests_active_ <= 0) { // Albums list is finished, get songs for all albums.

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

void SubsonicRequest::AddAlbumSongsRequest(const qint64 artist_id, const qint64 album_id, const QString &album_artist, const int offset) {

  Request request;
  request.artist_id = artist_id;
  request.album_id = album_id;
  request.album_artist = album_artist;
  request.offset = offset;
  album_songs_requests_queue_.enqueue(request);
  ++album_songs_requested_;
  if (album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();

}

void SubsonicRequest::FlushAlbumSongsRequests() {

  while (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) {

    Request request = album_songs_requests_queue_.dequeue();
    ++album_songs_requests_active_;
    ParamList params = ParamList() << Param("id", QString::number(request.album_id));
    QNetworkReply *reply = CreateGetRequest(QString("getAlbum"), params);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumSongsReplyReceived(QNetworkReply*, const qint64, const qint64, const QString&)), reply, request.artist_id, request.album_id, request.album_artist);

  }

}

void SubsonicRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const qint64 artist_id, const qint64 album_id, const QString &album_artist) {

  --album_songs_requests_active_;
  ++album_songs_received_;

  emit UpdateProgress(album_songs_received_);

  QByteArray data = GetReplyData(reply);

  if (finished_) return;

  if (data.isEmpty()) {
    SongsFinishCheck();
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    SongsFinishCheck();
    return;
  }

  if (json_obj.contains("error")) {
    QJsonValue json_error = json_obj["error"];
    if (!json_error.isObject()) {
      Error("Json error is not an object.", json_obj);
      SongsFinishCheck();
      return;
    }
    json_obj = json_error.toObject();
    if (!json_obj.isEmpty() && json_obj.contains("code") && json_obj.contains("message")) {
      int code = json_obj["code"].toInt();
      QString message = json_obj["message"].toString();
      Error(QString("%1 (%2)").arg(message).arg(code));
      SongsFinishCheck();
    }
    else {
      Error("Json error object missing code or message.", json_obj);
      SongsFinishCheck();
    }
    return;
  }

  if (!json_obj.contains("album")) {
    Error("Json reply is missing albumList.", json_obj);
    SongsFinishCheck();
    return;
  }
  QJsonValue json_album = json_obj["album"];

  if (!json_album.isObject()) {
    Error("Json album is not an object.", json_album);
    SongsFinishCheck();
    return;
  }
  QJsonObject json_album_obj = json_album.toObject();

  if (!json_album_obj.contains("song")) {
    Error("Json album object does not contain song array.", json_obj);
    SongsFinishCheck();
    return;
  }
  QJsonValue json_song = json_album_obj["song"];
  if (!json_song.isArray()) {
    Error("Json song is not an array.", json_album_obj);
    SongsFinishCheck();
    return;
  }
  QJsonArray json_array = json_song.toArray();

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValue &value : json_array) {

    if (!value.isObject()) {
      Error("Invalid Json reply, track is not a object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();

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

  SongsFinishCheck();

}

void SubsonicRequest::SongsFinishCheck() {

  if (finished_) return;

  if (!album_songs_requests_queue_.isEmpty() && album_songs_requests_active_ < kMaxConcurrentAlbumSongsRequests) FlushAlbumSongsRequests();

  if (
      download_album_covers() &&
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

int SubsonicRequest::ParseSong(Song &song, const QJsonObject &json_obj, const qint64 artist_id_requested, const qint64 album_id_requested, const QString &album_artist) {

  if (
      !json_obj.contains("id") ||
      !json_obj.contains("title") ||
      !json_obj.contains("album") ||
      !json_obj.contains("artist") ||
      !json_obj.contains("size") ||
      !json_obj.contains("contentType") ||
      !json_obj.contains("suffix") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("bitRate") ||
      !json_obj.contains("albumId") ||
      !json_obj.contains("artistId") ||
      !json_obj.contains("type")
    ) {
    Error("Invalid Json reply, song is missing one or more values.", json_obj);
    return -1;
  }

  qint64 song_id = json_obj["id"].toString().toLongLong();
  qint64 album_id = json_obj["albumId"].toString().toLongLong();
  qint64 artist_id = json_obj["artistId"].toString().toLongLong();

  QString title = json_obj["title"].toString();
  title.remove(Song::kTitleRemoveMisc);
  QString album = json_obj["album"].toString();
  QString artist = json_obj["artist"].toString();
  int size = json_obj["size"].toInt();
  QString mimetype = json_obj["contentType"].toString();
  quint64 duration = json_obj["duration"].toInt() * kNsecPerSec;
  int bitrate = json_obj["bitRate"].toInt();

  int year = 0;
  if (json_obj.contains("year")) year = json_obj["year"].toInt();

  int disc = 0;
  if (json_obj.contains("disc")) disc = json_obj["disc"].toString().toInt();

  int track = 0;
  if (json_obj.contains("track")) track = json_obj["track"].toInt();

  QString genre;
  if (json_obj.contains("genre")) genre = json_obj["genre"].toString();

  int cover_art_id = -1;
  if (json_obj.contains("coverArt")) cover_art_id = json_obj["coverArt"].toString().toInt();

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(QString::number(song_id));

  QUrl cover_url;
  if (cover_art_id != -1) {
    const ParamList params = ParamList() << Param("id", QString::number(cover_art_id));
    cover_url = CreateUrl("getCoverArt", params);
  }

  Song::FileType filetype(Song::FileType_Stream);
  QMimeDatabase mimedb;
  for (QString suffix : mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes()) {
    filetype = Song::FiletypeByExtension(suffix);
    if (filetype != Song::FileType_Unknown) break;
  }
  if (filetype == Song::FileType_Unknown) {
    qLog(Debug) << "Subsonic: Unknown mimetype" << mimetype;
    filetype = Song::FileType_Stream;
  }

  song.set_source(Song::Source_Subsonic);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  if (album_artist != artist) song.set_albumartist(album_artist);
  song.set_album(album);
  song.set_artist(artist);
  song.set_title(title);
  if (track > 0) song.set_track(track);
  if (disc > 0) song.set_disc(disc);
  if (year > 0) song.set_year(year);
  song.set_url(url);
  song.set_length_nanosec(duration);
  if (cover_url.isValid()) song.set_art_automatic(cover_url);
  song.set_genre(genre);
  song.set_directory_id(0);
  song.set_filetype(filetype);
  song.set_filesize(size);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_bitrate(bitrate);
  song.set_valid(true);

  return song_id;

}

void SubsonicRequest::GetAlbumCovers() {

  for (Song &song : songs_) {
    if (!song.art_automatic().isEmpty()) AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ == 1) emit UpdateStatus(tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else emit UpdateStatus(tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  emit ProgressSetMaximum(album_covers_requested_);
  emit UpdateProgress(0);

}

void SubsonicRequest::AddAlbumCoverRequest(Song &song) {

  QUrl cover_url(song.art_automatic());
  if (!cover_url.isValid()) return;

  if (album_covers_requests_sent_.contains(song.album_id())) {
    album_covers_requests_sent_.insertMulti(song.album_id(), &song);
    return;
  }

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = cover_url;
  request.filename = app_->album_cover_loader()->CoverFilePath(song.source(), song.effective_albumartist(), song.effective_album(), song.album_id(), QString(), cover_url);
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insertMulti(song.album_id(), &song);
  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void SubsonicRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);

    if (!verify_certificate()) {
      QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
      sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
      req.setSslConfiguration(sslconfig);
    }

    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumCoverReceived(QNetworkReply*, const QString&, const QUrl&, const QString&)), reply, request.album_id, request.url, request.filename);

  }

}

void SubsonicRequest::AlbumCoverReceived(QNetworkReply *reply, const QString &album_id, const QUrl &url, const QString &filename) {

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

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QString("Received empty image data for %1").arg(url.toString()));
    album_covers_requests_sent_.remove(album_id);
    AlbumCoverFinishCheck();
    return;
  }

  QImage image;
  if (image.loadFromData(data)) {
    if (image.save(filename, "JPG")) {
      while (album_covers_requests_sent_.contains(album_id)) {
        Song *song = album_covers_requests_sent_.take(album_id);
        song->set_art_automatic(QUrl::fromLocalFile(filename));
      }
    }
  }
  else {
    album_covers_requests_sent_.remove(album_id);
    Error(QString("Error decoding image data from %1").arg(url.toString()));
  }

  AlbumCoverFinishCheck();

}

void SubsonicRequest::AlbumCoverFinishCheck() {

  if (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests)
    FlushAlbumCoverRequests();

  FinishCheck();

}

void SubsonicRequest::FinishCheck() {

  if (
      !finished_ &&
      albums_requests_queue_.isEmpty() &&
      album_songs_requests_queue_.isEmpty() &&
      album_cover_requests_queue_.isEmpty() &&
      album_songs_requests_pending_.isEmpty() &&
      album_covers_requests_sent_.isEmpty() &&
      albums_requests_active_ <= 0 &&
      album_songs_requests_active_ <= 0 &&
      album_songs_received_ >= album_songs_requested_ &&
      album_covers_requests_active_ <= 0 &&
      album_covers_received_ >= album_covers_requested_
  ) {
    finished_ = true;
    if (no_results_ && songs_.isEmpty()) {
      emit Results(SongList(), QString());
    }
    else {
      if (songs_.isEmpty() && errors_.isEmpty())
        emit Results(songs_, tr("Unknown error"));
      else
        emit Results(songs_, ErrorsToHTML(errors_));
    }

  }

}

void SubsonicRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Subsonic:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  FinishCheck();

}

void SubsonicRequest::Warn(const QString &error, const QVariant &debug) {

  qLog(Error) << "Subsonic:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

