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

#include <memory>

#include <QObject>
#include <QMimeType>
#include <QMimeDatabase>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
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
#include <QtDebug>

#include "core/application.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "covermanager/albumcoverloader.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicbaserequest.h"
#include "subsonicrequest.h"

const int SubsonicRequest::kMaxConcurrentAlbumsRequests = 3;
const int SubsonicRequest::kMaxConcurrentAlbumSongsRequests = 3;
const int SubsonicRequest::kMaxConcurrentAlbumCoverRequests = 1;

SubsonicRequest::SubsonicRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, Application *app, QObject *parent)
    : SubsonicBaseRequest(service, parent),
      service_(service),
      url_handler_(url_handler),
      app_(app),
      network_(new QNetworkAccessManager),
      finished_(false),
      albums_requests_active_(0),
      album_songs_requests_active_(0),
      album_songs_requested_(0),
      album_songs_received_(0),
      album_covers_requests_active_(0),
      album_covers_requested_(0),
      album_covers_received_(0),
      no_results_(false)
      {

#if (QT_VERSION >= QT_VERSION_CHECK(5, 9, 0))
  network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
#endif

}

SubsonicRequest::~SubsonicRequest() {

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
  replies_.clear();
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
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumsReplyReceived(reply, request.offset); });

  }

}

void SubsonicRequest::AlbumsReplyReceived(QNetworkReply *reply, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

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
      Error("Json error object is missing code or message.", json_obj);
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
  QJsonValue value_albumlist;
  if (json_obj.contains("albumList")) value_albumlist = json_obj["albumList"];
  else if (json_obj.contains("albumList2")) value_albumlist = json_obj["albumList2"];

  if (!value_albumlist.isObject()) {
    Error("Json album list is not an object.", value_albumlist);
    AlbumsFinishCheck(offset_requested);
  }
  json_obj = value_albumlist.toObject();
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
  QJsonArray array_albums = json_album.toArray();

  if (array_albums.isEmpty()) {
    if (offset_requested == 0) no_results_ = true;
    AlbumsFinishCheck(offset_requested);
    return;
  }

  int albums_received = 0;
  for (const QJsonValue &value_album : array_albums) {

    ++albums_received;

    if (!value_album.isObject()) {
      Error("Invalid Json reply, album is not an object.", value_album);
      continue;
    }
    QJsonObject obj_album = value_album.toObject();

    if (!obj_album.contains("id") || !obj_album.contains("artist")) {
      Error("Invalid Json reply, album object in array is missing ID or artist.", obj_album);
      continue;
    }

    if (!obj_album.contains("album") && !obj_album.contains("name")) {
      Error("Invalid Json reply, album object in array is missing album or name.", obj_album);
      continue;
    }

    QString album_id = obj_album["id"].toString();
    if (album_id.isEmpty()) {
      album_id = QString::number(obj_album["id"].toInt());
    }

    QString artist = obj_album["artist"].toString();
    QString album;
    if (obj_album.contains("album")) album = obj_album["album"].toString();
    else if (obj_album.contains("name")) album = obj_album["name"].toString();

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

    QHash<QString, Request> ::iterator i;
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
    ParamList params = ParamList() << Param("id", request.album_id);
    QNetworkReply *reply = CreateGetRequest(QString("getAlbum"), params);
    replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumSongsReplyReceived(reply, request.artist_id, request.album_id, request.album_artist); });

  }

}

void SubsonicRequest::AlbumSongsReplyReceived(QNetworkReply *reply, const QString artist_id, const QString album_id, const QString album_artist) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

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
  QJsonValue value_album = json_obj["album"];

  if (!value_album.isObject()) {
    Error("Json album is not an object.", value_album);
    SongsFinishCheck();
    return;
  }
  QJsonObject obj_album = value_album.toObject();

  if (!obj_album.contains("song")) {
    Error("Json album object does not contain song array.", json_obj);
    SongsFinishCheck();
    return;
  }
  QJsonValue json_song = obj_album["song"];
  if (!json_song.isArray()) {
    Error("Json song is not an array.", obj_album);
    SongsFinishCheck();
    return;
  }
  QJsonArray array_songs = json_song.toArray();

  bool compilation = false;
  bool multidisc = false;
  SongList songs;
  int songs_received = 0;
  for (const QJsonValue &value_song : array_songs) {

    if (!value_song.isObject()) {
      Error("Invalid Json reply, track is not a object.", value_song);
      continue;
    }
    QJsonObject obj_song = value_song.toObject();

    ++songs_received;
    Song song(Song::Source_Subsonic);
    ParseSong(song, obj_song, artist_id, album_id, album_artist);
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

QString SubsonicRequest::ParseSong(Song &song, const QJsonObject &json_obj, const QString &artist_id_requested, const QString &album_id_requested, const QString &album_artist) {

  Q_UNUSED(artist_id_requested);
  Q_UNUSED(album_id_requested);

  if (
      !json_obj.contains("id") ||
      !json_obj.contains("title") ||
      !json_obj.contains("size") ||
      !json_obj.contains("suffix") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("type")
    ) {
    Error("Invalid Json reply, song is missing one or more values.", json_obj);
    return QString();
  }

  QString song_id;
  if (json_obj["id"].type() == QJsonValue::String) {
    song_id = json_obj["id"].toString();
  }
  else {
    song_id = QString::number(json_obj["id"].toInt());
  }

  QString album_id;
  if (json_obj.contains("albumId")) {
    if (json_obj["albumId"].type() == QJsonValue::String) {
      album_id = json_obj["albumId"].toString();
    }
    else {
      album_id = QString::number(json_obj["albumId"].toInt());
    }
  }

  QString artist_id;
  if (json_obj.contains("artistId")) {
    if (json_obj["artistId"].type() == QJsonValue::String) {
      artist_id = json_obj["artistId"].toString();
    }
    else {
      artist_id = QString::number(json_obj["artistId"].toInt());
    }
  }

  QString title = json_obj["title"].toString();

  QString album;
  if (json_obj.contains("album")) {
    album = json_obj["album"].toString();
  }
  QString artist;
  if (json_obj.contains("artist")) {
    artist = json_obj["artist"].toString();
  }

  int size = 0;
  if (json_obj["size"].type() == QJsonValue::String) {
    size = json_obj["size"].toString().toInt();
  }
  else {
    size = json_obj["size"].toInt();
  }

  quint64 duration = 0;
  if (json_obj["duration"].type() == QJsonValue::String) {
    duration = json_obj["duration"].toString().toInt() * kNsecPerSec;
  }
  else {
    duration = json_obj["duration"].toInt() * kNsecPerSec;
  }

  int bitrate = 0;
  if (json_obj.contains("bitRate")) {
    if (json_obj["bitRate"].type() == QJsonValue::String) {
      bitrate = json_obj["bitRate"].toString().toInt();
    }
    else {
      bitrate = json_obj["bitRate"].toInt();
    }
  }

  QString mimetype;
  if (json_obj.contains("contentType")) {
    mimetype = json_obj["contentType"].toString();
  }

  int year = 0;
  if (json_obj.contains("year")) {
    if (json_obj["year"].type() == QJsonValue::String) {
      year = json_obj["year"].toString().toInt();
    }
    else {
      year = json_obj["year"].toInt();
    }
  }

  int disc = 0;
  if (json_obj.contains("discNumber")) {
    if (json_obj["discNumber"].type() == QJsonValue::String) {
      disc = json_obj["discNumber"].toString().toInt();
    }
    else {
      disc = json_obj["discNumber"].toInt();
    }
  }

  int track = 0;
  if (json_obj.contains("track")) {
    if (json_obj["track"].type() == QJsonValue::String) {
      track = json_obj["track"].toString().toInt();
    }
    else {
      track = json_obj["track"].toInt();
    }
  }

  QString genre;
  if (json_obj.contains("genre")) genre = json_obj["genre"].toString();

  QString cover_art_id;
  if (json_obj.contains("coverArt")) {
    if (json_obj["coverArt"].type() == QJsonValue::String) {
      cover_art_id = json_obj["coverArt"].toString();
    }
    else {
      cover_art_id = QString::number(json_obj["coverArt"].toInt());
    }
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  QUrl cover_url;
  if (!cover_art_id.isEmpty()) {
    cover_url = CreateUrl("getCoverArt", ParamList() << Param("id", cover_art_id));
  }

  Song::FileType filetype(Song::FileType_Stream);
  if (!mimetype.isEmpty()) {
    QMimeDatabase mimedb;
    for (QString suffix : mimedb.mimeTypeForName(mimetype.toUtf8()).suffixes()) {
      filetype = Song::FiletypeByExtension(suffix);
      if (filetype != Song::FileType_Unknown) break;
    }
    if (filetype == Song::FileType_Unknown) {
      qLog(Debug) << "Subsonic: Unknown mimetype" << mimetype;
      filetype = Song::FileType_Stream;
    }
  }

  song.set_source(Song::Source_Subsonic);
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
  QUrlQuery cover_url_query(cover_url);

  if (!cover_url.isValid()) return;

  if (album_covers_requests_sent_.contains(cover_url)) {
    album_covers_requests_sent_.insert(cover_url, &song);
    return;
  }

  AlbumCoverRequest request;
  request.album_id = song.album_id();
  request.url = cover_url;
  request.filename = Song::ImageCacheDir(Song::Source_Subsonic) + "/" + cover_url_query.queryItemValue("id");
  if (request.filename.isEmpty()) return;

  album_covers_requests_sent_.insert(cover_url, &song);
  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void SubsonicRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkRequest req(request.url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif

    if (!verify_certificate()) {
      QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
      sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
      req.setSslConfiguration(sslconfig);
    }

    QNetworkReply *reply = network_->get(req);
    album_cover_replies_ << reply;
    connect(reply, &QNetworkReply::finished, [=] { AlbumCoverReceived(reply, request.url, request.filename); });

  }

}

void SubsonicRequest::AlbumCoverReceived(QNetworkReply *reply, const QUrl url, const QString filename) {

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

  emit UpdateProgress(album_covers_received_);

  if (!album_covers_requests_sent_.contains(url)) {
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2) for %3").arg(reply->errorString()).arg(reply->error()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
    AlbumCoverFinishCheck();
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QString("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(url.toString()));
    if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
    AlbumCoverFinishCheck();
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (!QImageReader::supportedMimeTypes().contains(mimetype.toUtf8())) {
    Error(QString("Unsupported mimetype for image reader %1 for %2").arg(mimetype).arg(url.toString()));
    if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
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
    if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
    AlbumCoverFinishCheck();
    return;
  }
  QByteArray format = format_list.first();
  QString fullfilename = filename + "." + format.toLower();

  QImage image;
  if (image.loadFromData(data, format)) {
    if (image.save(fullfilename, format)) {
      while (album_covers_requests_sent_.contains(url)) {
        Song *song = album_covers_requests_sent_.take(url);
        song->set_art_automatic(QUrl::fromLocalFile(fullfilename));
      }
    }
    else {
      Error(QString("Error saving image data to %1.").arg(fullfilename));
      if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
    }
  }
  else {
    Error(QString("Error decoding image data from %1.").arg(url.toString()));
    if (album_covers_requests_sent_.contains(url)) album_covers_requests_sent_.remove(url);
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
