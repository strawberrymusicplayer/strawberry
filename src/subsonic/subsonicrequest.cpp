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
#include <QDir>
#include <QMimeDatabase>
#include <QByteArray>
#include <QByteArrayList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/networktimeouts.h"
#include "utilities/imageutils.h"
#include "utilities/timeconstants.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicbaserequest.h"
#include "subsonicrequest.h"

using namespace Qt::StringLiterals;

namespace {
constexpr int kMaxConcurrentAlbumsRequests = 3;
constexpr int kMaxConcurrentAlbumSongsRequests = 3;
constexpr int kMaxConcurrentAlbumCoverRequests = 1;
}  // namespace

SubsonicRequest::SubsonicRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, Application *app, QObject *parent)
    : SubsonicBaseRequest(service, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
      network_(new QNetworkAccessManager(this)),
      timeouts_(new NetworkTimeouts(30000, this)),
      finished_(false),
      albums_requests_active_(0),
      album_songs_requests_active_(0),
      album_songs_requested_(0),
      album_songs_received_(0),
      album_covers_requests_active_(0),
      album_covers_requested_(0),
      album_covers_received_(0),
      no_results_(false) {

  network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

}

SubsonicRequest::~SubsonicRequest() {

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
  cover_urls_.clear();
  errors_.clear();
  no_results_ = false;
  replies_.clear();
  album_cover_replies_.clear();

}

void SubsonicRequest::GetAlbums() {

  Q_EMIT UpdateStatus(tr("Retrieving albums..."));
  Q_EMIT UpdateProgress(0);
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

    ParamList params = ParamList() << Param(QStringLiteral("type"), QStringLiteral("alphabeticalByName"));
    if (request.size > 0) params << Param(QStringLiteral("size"), QString::number(request.size));
    if (request.offset > 0) params << Param(QStringLiteral("offset"), QString::number(request.offset));

    QNetworkReply *reply = CreateGetRequest(QStringLiteral("getAlbumList2"), params);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumsReplyReceived(reply, request.offset, request.size); });
    timeouts_->AddReply(reply);

  }

}

void SubsonicRequest::AlbumsReplyReceived(QNetworkReply *reply, const int offset_requested, const int size_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --albums_requests_active_;

  QByteArray data = GetReplyData(reply);

  if (finished_) return;

  if (data.isEmpty()) {
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }

  if (json_obj.contains("error"_L1)) {
    QJsonValue json_error = json_obj["error"_L1];
    if (!json_error.isObject()) {
      Error(QStringLiteral("Json error is not an object."), json_obj);
      AlbumsFinishCheck(offset_requested, size_requested);
      return;
    }
    json_obj = json_error.toObject();
    if (!json_obj.isEmpty() && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
      int code = json_obj["code"_L1].toInt();
      QString message = json_obj["message"_L1].toString();
      Error(QStringLiteral("%1 (%2)").arg(message).arg(code));
      AlbumsFinishCheck(offset_requested, size_requested);
    }
    else {
      Error(QStringLiteral("Json error object is missing code or message."), json_obj);
      AlbumsFinishCheck(offset_requested, size_requested);
    }
    return;
  }

  if (!json_obj.contains("albumList"_L1) && !json_obj.contains("albumList2"_L1)) {
    Error(QStringLiteral("Json reply is missing albumList."), json_obj);
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }
  QJsonValue value_albumlist;
  if (json_obj.contains("albumList"_L1)) value_albumlist = json_obj["albumList"_L1];
  else if (json_obj.contains("albumList2"_L1)) value_albumlist = json_obj["albumList2"_L1];

  if (!value_albumlist.isObject()) {
    Error(QStringLiteral("Json album list is not an object."), value_albumlist);
    AlbumsFinishCheck(offset_requested, size_requested);
  }
  json_obj = value_albumlist.toObject();
  if (json_obj.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }

  if (!json_obj.contains("album"_L1)) {
    Error(QStringLiteral("Json album list does not contain album array."), json_obj);
    AlbumsFinishCheck(offset_requested, size_requested);
  }
  QJsonValue json_album = json_obj["album"_L1];
  if (json_album.isNull()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }
  if (!json_album.isArray()) {
    Error(QStringLiteral("Json album is not an array."), json_album);
    AlbumsFinishCheck(offset_requested, size_requested);
  }
  const QJsonArray array_albums = json_album.toArray();

  if (array_albums.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested, size_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value_album : array_albums) {

    ++albums_received;

    if (!value_album.isObject()) {
      Error(QStringLiteral("Invalid Json reply, album is not an object."));
      continue;
    }
    QJsonObject obj_album = value_album.toObject();

    if (!obj_album.contains("id"_L1) || !obj_album.contains("artist"_L1)) {
      Error(QStringLiteral("Invalid Json reply, album object in array is missing ID or artist."), obj_album);
      continue;
    }

    if (!obj_album.contains("album"_L1) && !obj_album.contains("name"_L1)) {
      Error(QStringLiteral("Invalid Json reply, album object in array is missing album or name."), obj_album);
      continue;
    }

    QString album_id = obj_album["id"_L1].toString();
    if (album_id.isEmpty()) {
      album_id = QString::number(obj_album["id"_L1].toInt());
    }

    QString artist = obj_album["artist"_L1].toString();
    QString album;
    if (obj_album.contains("album"_L1)) album = obj_album["album"_L1].toString();
    else if (obj_album.contains("name"_L1)) album = obj_album["name"_L1].toString();

    if (album_songs_requests_pending_.contains(album_id)) continue;

    Request request;
    request.album_id = album_id;
    request.album_artist = artist;
    album_songs_requests_pending_.insert(album_id, request);

  }

  AlbumsFinishCheck(offset_requested, size_requested, albums_received);

}

void SubsonicRequest::AlbumsFinishCheck(const int offset, const int size, const int albums_received) {

  if (finished_) return;

  if (albums_received > 0 && albums_received >= size) {
    int offset_next = offset + albums_received;
    if (offset_next > 0) {
      AddAlbumsRequest(offset_next);
    }
  }

  if (!albums_requests_queue_.isEmpty() && albums_requests_active_ < kMaxConcurrentAlbumsRequests) FlushAlbumsRequests();

  if (albums_requests_queue_.isEmpty() && albums_requests_active_ <= 0) { // Albums list is finished, get songs for all albums.

    for (QHash<QString, Request>::const_iterator it = album_songs_requests_pending_.constBegin(); it != album_songs_requests_pending_.constEnd(); ++it) {
      const Request request = it.value();
      AddAlbumSongsRequest(request.artist_id, request.album_id, request.album_artist);
    }
    album_songs_requests_pending_.clear();

    if (album_songs_requested_ > 0) {
      if (album_songs_requested_ == 1) Q_EMIT UpdateStatus(tr("Retrieving songs for %1 album...").arg(album_songs_requested_));
      else Q_EMIT UpdateStatus(tr("Retrieving songs for %1 albums...").arg(album_songs_requested_));
      Q_EMIT ProgressSetMaximum(album_songs_requested_);
      Q_EMIT UpdateProgress(0);
    }
  }

  FinishCheck();

}

void SubsonicRequest::AddAlbumSongsRequest(const QString &artist_id, const QString &album_id, const QString &album_artist, const int offset) {

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
    QNetworkReply *reply = CreateGetRequest(QStringLiteral("getAlbum"), ParamList() << Param(QStringLiteral("id"), request.album_id));
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumSongsReplyReceived(reply, request.artist_id, request.album_id, request.album_artist); });
    timeouts_->AddReply(reply);

  }

}

void SubsonicRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const QString &artist_id, const QString &album_id, const QString &album_artist) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --album_songs_requests_active_;
  ++album_songs_received_;

  Q_EMIT UpdateProgress(album_songs_received_);

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

  if (json_obj.contains("error"_L1)) {
    QJsonValue json_error = json_obj["error"_L1];
    if (!json_error.isObject()) {
      Error(QStringLiteral("Json error is not an object."), json_obj);
      SongsFinishCheck();
      return;
    }
    json_obj = json_error.toObject();
    if (!json_obj.isEmpty() && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
      int code = json_obj["code"_L1].toInt();
      QString message = json_obj["message"_L1].toString();
      Error(QStringLiteral("%1 (%2)").arg(message).arg(code));
      SongsFinishCheck();
    }
    else {
      Error(QStringLiteral("Json error object missing code or message."), json_obj);
      SongsFinishCheck();
    }
    return;
  }

  if (!json_obj.contains("album"_L1)) {
    Error(QStringLiteral("Json reply is missing albumList."), json_obj);
    SongsFinishCheck();
    return;
  }
  QJsonValue value_album = json_obj["album"_L1];

  if (!value_album.isObject()) {
    Error(QStringLiteral("Json album is not an object."), value_album);
    SongsFinishCheck();
    return;
  }
  QJsonObject obj_album = value_album.toObject();

  if (!obj_album.contains("song"_L1)) {
    Error(QStringLiteral("Json album object does not contain song array."), json_obj);
    SongsFinishCheck();
    return;
  }
  QJsonValue json_song = obj_album["song"_L1];
  if (!json_song.isArray()) {
    Error(QStringLiteral("Json song is not an array."), obj_album);
    SongsFinishCheck();
    return;
  }
  const QJsonArray array_songs = json_song.toArray();

  qint64 created = 0;
  if (obj_album.contains("created"_L1)) {
    created = QDateTime::fromString(obj_album["created"_L1].toString(), Qt::ISODate).toSecsSinceEpoch();
  }

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  for (const QJsonValue &value_song : array_songs) {

    if (!value_song.isObject()) {
      Error(QStringLiteral("Invalid Json reply, track is not a object."));
      continue;
    }
    QJsonObject obj_song = value_song.toObject();

    Song song(Song::Source::Subsonic);
    ParseSong(song, obj_song, artist_id, album_id, album_artist, created);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
    songs << song;
  }

  for (Song song : std::as_const(songs)) {
    if (compilation) song.set_compilation_detected(true);
    if (!multidisc) {
      song.set_disc(0);
    }
    songs_.insert(song.song_id(), song);
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

QString SubsonicRequest::ParseSong(Song &song, const QJsonObject &json_obj, const QString &artist_id_requested, const QString &album_id_requested, const QString &album_artist, const qint64 album_created) {

  Q_UNUSED(artist_id_requested);
  Q_UNUSED(album_id_requested);

  if (
      !json_obj.contains("id"_L1) ||
      !json_obj.contains("title"_L1) ||
      !json_obj.contains("size"_L1) ||
      !json_obj.contains("suffix"_L1) ||
      !json_obj.contains("duration"_L1) ||
      !json_obj.contains("type"_L1)
    ) {
    Error(QStringLiteral("Invalid Json reply, song is missing one or more values."), json_obj);
    return QString();
  }

  QString song_id;
  if (json_obj["id"_L1].type() == QJsonValue::String) {
    song_id = json_obj["id"_L1].toString();
  }
  else {
    song_id = QString::number(json_obj["id"_L1].toInt());
  }

  QString album_id;
  if (json_obj.contains("albumId"_L1)) {
    if (json_obj["albumId"_L1].type() == QJsonValue::String) {
      album_id = json_obj["albumId"_L1].toString();
    }
    else {
      album_id = QString::number(json_obj["albumId"_L1].toInt());
    }
  }

  QString artist_id;
  if (json_obj.contains("artistId"_L1)) {
    if (json_obj["artistId"_L1].type() == QJsonValue::String) {
      artist_id = json_obj["artistId"_L1].toString();
    }
    else {
      artist_id = QString::number(json_obj["artistId"_L1].toInt());
    }
  }

  QString title = json_obj["title"_L1].toString();

  QString album;
  if (json_obj.contains("album"_L1)) {
    album = json_obj["album"_L1].toString();
  }
  QString artist;
  if (json_obj.contains("artist"_L1)) {
    artist = json_obj["artist"_L1].toString();
  }

  int size = 0;
  if (json_obj["size"_L1].type() == QJsonValue::String) {
    size = json_obj["size"_L1].toString().toInt();
  }
  else {
    size = json_obj["size"_L1].toInt();
  }

  qint64 duration = 0;
  if (json_obj["duration"_L1].type() == QJsonValue::String) {
    duration = json_obj["duration"_L1].toString().toInt() * kNsecPerSec;
  }
  else {
    duration = json_obj["duration"_L1].toInt() * kNsecPerSec;
  }

  int bitrate = 0;
  if (json_obj.contains("bitRate"_L1)) {
    if (json_obj["bitRate"_L1].type() == QJsonValue::String) {
      bitrate = json_obj["bitRate"_L1].toString().toInt();
    }
    else {
      bitrate = json_obj["bitRate"_L1].toInt();
    }
  }

  QString mimetype;
  if (json_obj.contains("contentType"_L1)) {
    mimetype = json_obj["contentType"_L1].toString();
  }

  int year = 0;
  if (json_obj.contains("year"_L1)) {
    if (json_obj["year"_L1].type() == QJsonValue::String) {
      year = json_obj["year"_L1].toString().toInt();
    }
    else {
      year = json_obj["year"_L1].toInt();
    }
  }

  int disc = 0;
  if (json_obj.contains("discNumber"_L1)) {
    if (json_obj["discNumber"_L1].type() == QJsonValue::String) {
      disc = json_obj["discNumber"_L1].toString().toInt();
    }
    else {
      disc = json_obj["discNumber"_L1].toInt();
    }
  }

  int track = 0;
  if (json_obj.contains("track"_L1)) {
    if (json_obj["track"_L1].type() == QJsonValue::String) {
      track = json_obj["track"_L1].toString().toInt();
    }
    else {
      track = json_obj["track"_L1].toInt();
    }
  }

  QString genre;
  if (json_obj.contains("genre"_L1)) genre = json_obj["genre"_L1].toString();

  QString cover_id;
  if (json_obj.contains("coverArt"_L1)) {
    if (json_obj["coverArt"_L1].type() == QJsonValue::String) {
      cover_id = json_obj["coverArt"_L1].toString();
    }
    else {
      cover_id = QString::number(json_obj["coverArt"_L1].toInt());
    }
  }

  qint64 created = 0;
  if (json_obj.contains("created"_L1)) {
    created = QDateTime::fromString(json_obj["created"_L1].toString(), Qt::ISODate).toSecsSinceEpoch();
  }
  else {
    created = album_created;
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  QUrl cover_url;
  if (!cover_id.isEmpty()) {
    if (cover_urls_.contains(cover_id)) {
      cover_url = cover_urls_[cover_id];
    }
    else {
      cover_url = CreateUrl(server_url(), auth_method(), username(), password(), QStringLiteral("getCoverArt"), ParamList() << Param(QStringLiteral("id"), cover_id));
      cover_urls_.insert(cover_id, cover_url);
    }
  }

  Song::FileType filetype(Song::FileType::Stream);
  if (!mimetype.isEmpty()) {
    QMimeDatabase mimedb;
    const QStringList suffixes = mimedb.mimeTypeForName(mimetype).suffixes();
    for (const QString &suffix : suffixes) {
      filetype = Song::FiletypeByExtension(suffix);
      if (filetype != Song::FileType::Unknown) break;
    }
    if (filetype == Song::FileType::Unknown) {
      qLog(Debug) << "Subsonic: Unknown mimetype" << mimetype;
      filetype = Song::FileType::Stream;
    }
  }

  song.set_source(Song::Source::Subsonic);
  song.set_song_id(song_id);
  if (!album_id.isEmpty()) song.set_album_id(album_id);
  if (!artist_id.isEmpty()) song.set_artist_id(artist_id);
  if (!album_artist.isEmpty()) song.set_albumartist(album_artist);
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
  song.set_mtime(created);
  song.set_ctime(created);
  song.set_bitrate(bitrate);
  song.set_valid(true);

  return song_id;

}

void SubsonicRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    if (!song.art_automatic().isEmpty()) AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ == 1) Q_EMIT UpdateStatus(tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  else Q_EMIT UpdateStatus(tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  Q_EMIT ProgressSetMaximum(album_covers_requested_);
  Q_EMIT UpdateProgress(0);

}

void SubsonicRequest::AddAlbumCoverRequest(const Song &song) {

  const QUrl cover_url = song.art_automatic();

  if (!cover_url.isValid()) {
    return;
  }

  QUrlQuery cover_url_query(cover_url);

  if (!cover_url_query.hasQueryItem(QStringLiteral("id"))) {
    return;
  }

  QString cover_id = cover_url_query.queryItemValue(QStringLiteral("id"));

  if (album_covers_requests_sent_.contains(cover_id)) {
    album_covers_requests_sent_.insert(cover_id, song.song_id());
    return;
  }

  QString cover_path = Song::ImageCacheDir(Song::Source::Subsonic);
  QDir dir(cover_path);
  if (!dir.exists()) dir.mkpath(cover_path);

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.cover_id = cover_id;
  request.url = cover_url;
  request.filename = cover_path + QLatin1Char('/') + cover_id + ".jpg"_L1;
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(cover_id, song.song_id());
  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void SubsonicRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2());

    if (!verify_certificate()) {
      QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
      sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
      req.setSslConfiguration(sslconfig);
    }

    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request); });
    timeouts_->AddReply(reply);

  }

}

void SubsonicRequest::AlbumCoverReceived(QNetworkReply *reply, const AlbumCoverRequest &request) {

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
  ++album_covers_received_;

  if (finished_) return;

  Q_EMIT UpdateProgress(album_covers_received_);

  if (!album_covers_requests_sent_.contains(request.cover_id)) {
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2) for %3").arg(reply->errorString()).arg(reply->error()).arg(request.url.toString()));
    if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(request.url.toString()));
    if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
    AlbumCoverFinishCheck();
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, request.url.toString()));
    if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
    AlbumCoverFinishCheck();
    return;
  }

  QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(request.url.toString()));
    if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
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
    if (image.save(request.filename, format)) {
      while (album_covers_requests_sent_.contains(request.cover_id)) {
        const QString song_id = album_covers_requests_sent_.take(request.cover_id);
        if (songs_.contains(song_id)) {
          songs_[song_id].set_art_automatic(QUrl::fromLocalFile(request.filename));
        }
      }
    }
    else {
      Error(QStringLiteral("Error saving image data to %1.").arg(request.filename));
      if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
    }
  }
  else {
    Error(QStringLiteral("Error decoding image data from %1.").arg(request.url.toString()));
    if (album_covers_requests_sent_.contains(request.cover_id)) album_covers_requests_sent_.remove(request.cover_id);
  }

  AlbumCoverFinishCheck();

}

void SubsonicRequest::AlbumCoverFinishCheck() {

  if (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {
    FlushAlbumCoverRequests();
  }

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
      Q_EMIT Results(SongMap(), QString());
    }
    else {
      if (songs_.isEmpty() && errors_.isEmpty()) {
        Q_EMIT Results(songs_, tr("Unknown error"));
      }
      else {
        Q_EMIT Results(songs_, ErrorsToHTML(errors_));
      }
    }

  }

}

void SubsonicRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Subsonic:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}

void SubsonicRequest::Warn(const QString &error, const QVariant &debug) {

  qLog(Error) << "Subsonic:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
