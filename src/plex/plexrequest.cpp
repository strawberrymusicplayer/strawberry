/*
 * Strawberry Music Player
 * Copyright 2025-2026, Strawberry Music Player contributors
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
#include <QString>
#include <QSet>
#include <QUrl>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QIODevice>
#include <QCryptographicHash>
#include <QImage>
#include <QImageReader>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/song.h"
#include "utilities/strutils.h"
#include "utilities/coverutils.h"
#include "utilities/imageutils.h"
#include "constants/timeconstants.h"
#include "plexservice.h"
#include "plexurlhandler.h"
#include "plexbaserequest.h"
#include "plexrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kContainerSize = 500;
constexpr int kMaxConcurrentAlbumCoverRequests = 3;
}  // namespace

PlexRequest::PlexRequest(PlexService *service, const SharedPtr<NetworkAccessManager> network, PlexUrlHandler *url_handler, const SongMap &existing_songs, const QStringList &rejected_song_ids, const qint64 updated_since, QObject *parent)
    : PlexBaseRequest(service, network, parent),
      url_handler_(url_handler),
      finished_(false),
      incremental_(updated_since > 0 && !existing_songs.isEmpty()),
      updated_since_(updated_since),
      newest_updated_at_(0),
      server_total_songs_(0),
      songs_requests_active_(0),
      songs_total_(0),
      songs_received_(0),
      album_covers_started_(false),
      album_covers_requests_active_(0),
      album_covers_requested_(0),
      album_covers_received_(0) {

  if (incremental_) {
    songs_ = existing_songs;
    rejected_song_ids_ = QSet<QString>(rejected_song_ids.cbegin(), rejected_song_ids.cend());
    // Cover URLs not downloaded yet carry the server address, update it in case it changed since the last sync, they are downloaded again with the other covers.
    // Older versions saved the access token in the cover URLs, it's removed, and downloaded covers which are not in the cache anymore are downloaded again.
    const QUrl current_server_url = server_url();
    for (Song &song : songs_) {
      if (song.art_automatic().isEmpty()) continue;
      song.set_art_automatic(CoverUrlForServer(song.art_automatic(), current_server_url));
    }
  }

}

PlexRequest::~PlexRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void PlexRequest::GetLibrarySections() {

  Q_EMIT UpdateStatus(tr("Retrieving library sections..."));
  Q_EMIT ProgressSetMaximum(0);
  Q_EMIT UpdateProgress(0);

  QNetworkReply *reply = CreateGetRequest(u"/library/sections"_s, ParamList());
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleLibrarySectionsReply(reply); });

}

void PlexRequest::HandleLibrarySectionsReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    FinishCheck();
    return;
  }

  const QJsonArray array_directory = json_object_result.json_object["Directory"_L1].toArray();

  QStringList section_keys;
  for (const QJsonValue &value_directory : array_directory) {
    if (!value_directory.isObject()) continue;
    const QJsonObject object_directory = value_directory.toObject();
    if (object_directory["type"_L1].toString() != "artist"_L1) continue;
    const QString key = object_directory["key"_L1].toString();
    if (!key.isEmpty()) section_keys << key;
  }

  if (section_keys.isEmpty()) {
    Error(tr("No music libraries found on the Plex server."));
    FinishCheck();
    return;
  }

  Q_EMIT UpdateStatus(tr("Retrieving songs..."));

  for (const QString &section_key : std::as_const(section_keys)) {
    ++songs_requests_active_;
    GetSectionSongs(section_key);
    if (incremental_) {
      ++songs_requests_active_;
      GetSectionCount(section_key);
    }
  }

}

void PlexRequest::GetSectionCount(const QString &section_key) {

  const ParamList params = ParamList() << Param(u"type"_s, u"10"_s)
                                       << Param(u"X-Plex-Container-Start"_s, u"0"_s)
                                       << Param(u"X-Plex-Container-Size"_s, u"0"_s);

  QNetworkReply *reply = CreateGetRequest(u"/library/sections/"_s + section_key + "/all"_L1, params);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleSectionCountReply(reply); });

}

void PlexRequest::HandleSectionCountReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (json_object_result.success()) {
    server_total_songs_ += json_object_result.json_object["totalSize"_L1].toInt();
  }
  else {
    Error(json_object_result.error_message);
  }

  --songs_requests_active_;

  SongsFinishCheck();

}

void PlexRequest::GetSectionSongs(const QString &section_key, const int offset) {

  ParamList params = ParamList() << Param(u"type"_s, u"10"_s)
                                 << Param(u"X-Plex-Container-Start"_s, QString::number(offset))
                                 << Param(u"X-Plex-Container-Size"_s, QString::number(kContainerSize));

  // Plex filter syntax: "updatedAt>=X" is sent as key "updatedAt>" so the joining "=" forms the ">=" operator.
  if (incremental_) params << Param(u"updatedAt>"_s, QString::number(updated_since_));

  QNetworkReply *reply = CreateGetRequest(u"/library/sections/"_s + section_key + "/all"_L1, params);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, section_key, offset]() { HandleSectionSongsReply(reply, section_key, offset); });

}

void PlexRequest::HandleSectionSongsReply(QNetworkReply *reply, const QString &section_key, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    --songs_requests_active_;
    Error(json_object_result.error_message);
    SongsFinishCheck();
    return;
  }

  const QJsonObject json_object = json_object_result.json_object;
  const int total_size = json_object["totalSize"_L1].toInt();
  const QJsonArray array_metadata = json_object["Metadata"_L1].toArray();

  songs_total_ += static_cast<int>(array_metadata.count());

  for (const QJsonValue &value_metadata : array_metadata) {
    if (!value_metadata.isObject()) continue;
    ParseSong(value_metadata.toObject());
    ++songs_received_;
  }

  Q_EMIT ProgressSetMaximum(total_size > 0 ? total_size : songs_total_);
  Q_EMIT UpdateProgress(songs_received_);

  if (!array_metadata.isEmpty() && offset_requested + array_metadata.count() < total_size) {
    GetSectionSongs(section_key, offset_requested + static_cast<int>(array_metadata.count()));
  }
  else {
    --songs_requests_active_;
  }

  SongsFinishCheck();

}

void PlexRequest::ParseSong(const QJsonObject &object_metadata) {

  const QString song_id = object_metadata["ratingKey"_L1].toString();

  if (song_id.isEmpty() || !object_metadata.contains("title"_L1) || !object_metadata.contains("Media"_L1)) {
    RejectSong(song_id, u"missing one or more values"_s, object_metadata);
    return;
  }

  const QString album_id = object_metadata["parentRatingKey"_L1].toString();
  const QString artist_id = object_metadata["grandparentRatingKey"_L1].toString();

  const QString title = object_metadata["title"_L1].toString();
  const QString album = object_metadata["parentTitle"_L1].toString();
  const QString album_artist = object_metadata["grandparentTitle"_L1].toString();
  QString artist = object_metadata["originalTitle"_L1].toString();
  if (artist.isEmpty()) artist = album_artist;

  const int track = object_metadata["index"_L1].toInt();
  const int disc = object_metadata["parentIndex"_L1].toInt();
  int year = object_metadata["parentYear"_L1].toInt();
  if (year == 0) year = object_metadata["year"_L1].toInt();
  const qint64 duration = object_metadata["duration"_L1].toVariant().toLongLong() * kNsecPerMsec;
  const qint64 created = object_metadata["addedAt"_L1].toVariant().toLongLong();
  const qint64 updated = object_metadata["updatedAt"_L1].toVariant().toLongLong();

  const QJsonArray array_media = object_metadata["Media"_L1].toArray();
  if (array_media.isEmpty() || !array_media.first().isObject()) {
    RejectSong(song_id, u"missing Media"_s, object_metadata);
    return;
  }
  const QJsonObject object_media = array_media.first().toObject();
  const int bitrate = object_media["bitrate"_L1].toInt();
  const QString container = object_media["container"_L1].toString();

  const QJsonArray array_part = object_media["Part"_L1].toArray();
  if (array_part.isEmpty() || !array_part.first().isObject()) {
    RejectSong(song_id, u"missing Part"_s, object_metadata);
    return;
  }
  const QJsonObject object_part = array_part.first().toObject();
  const QString part_key = object_part["key"_L1].toString();
  if (part_key.isEmpty()) {
    RejectSong(song_id, u"missing Part key"_s, object_metadata);
    return;
  }
  const qint64 filesize = object_part["size"_L1].toVariant().toLongLong();

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(part_key);

  QString thumb = object_metadata["parentThumb"_L1].toString();
  if (thumb.isEmpty()) thumb = object_metadata["thumb"_L1].toString();
  // The cover URL is saved in the collection, and exported in playlists, so it doesn't include the access token, the cover is downloaded with the token in a header when the songs are received.
  QUrl cover_url;
  if (download_album_covers() && !thumb.isEmpty()) {
    cover_url = CoverUrl(server_url(), thumb);
  }

  Song::FileType filetype(Song::FileType::Stream);
  if (!container.isEmpty()) {
    filetype = Song::FiletypeByExtension(container);
    if (filetype == Song::FileType::Unknown) {
      qLog(Debug) << "Plex: Unknown container" << container;
      filetype = Song::FileType::Stream;
    }
  }

  Song song(Song::Source::Plex);
  song.set_song_id(song_id);
  if (!album_id.isEmpty()) song.set_album_id(album_id);
  if (!artist_id.isEmpty()) song.set_artist_id(artist_id);
  song.set_title(title);
  song.set_album(album);
  song.set_artist(artist);
  if (!album_artist.isEmpty() && album_artist != artist) song.set_albumartist(album_artist);
  if (track > 0) song.set_track(track);
  if (disc > 0) song.set_disc(disc);
  if (year > 0) song.set_year(year);
  song.set_url(url);
  song.set_length_nanosec(duration);
  if (cover_url.isValid()) song.set_art_automatic(cover_url);
  song.set_directory_id(0);
  song.set_filetype(filetype);
  song.set_filesize(filesize);
  song.set_mtime(created);
  song.set_ctime(created);
  song.set_bitrate(bitrate);
  song.set_valid(true);

  // Only raise the incremental sync watermark for songs that were actually added, otherwise a song failing validation would be skipped by all future incremental syncs.
  newest_updated_at_ = qMax(newest_updated_at_, qMax(created, updated));

  songs_.insert(song.song_id(), song);
  rejected_song_ids_.remove(song.song_id());

}

void PlexRequest::RejectSong(const QString &song_id, const QString &reason, const QJsonObject &object_metadata) {

  // A single invalid song should not make the whole request fail, so this is not an error.
  qLog(Warning) << "Plex: Skipping song" << song_id << "which is" << reason;
  qLog(Debug) << object_metadata;

  // Remember rejected songs, since the server still counts them in the total used to detect deleted songs.
  if (!song_id.isEmpty()) rejected_song_ids_.insert(song_id);

}

void PlexRequest::SongsFinishCheck() {

  if (finished_) return;

  if (songs_requests_active_ > 0) return;

  // The incremental fetch cannot detect deleted songs, so compare the server's total track count with what we have and redo a full sync on mismatch.
  // Songs that could not be parsed are counted by the server, so include them.
  // This can't detect a song deleted and another added between syncs, the service does a full sync regularly for that.
  if (incremental_ && server_total_songs_ != songs_.count() + rejected_song_ids_.count()) {
    qLog(Debug) << "Plex: Song count mismatch (server:" << server_total_songs_ << "local:" << songs_.count() << "rejected:" << rejected_song_ids_.count() << "), doing full sync";
    incremental_ = false;
    updated_since_ = 0;
    server_total_songs_ = 0;
    songs_total_ = 0;
    songs_received_ = 0;
    newest_updated_at_ = 0;
    songs_.clear();
    rejected_song_ids_.clear();
    errors_.clear();
    GetLibrarySections();
    return;
  }

  FinishCheck();

}

void PlexRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    if (!song.art_automatic().isEmpty()) AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ > 0) {
    Q_EMIT UpdateStatus(tr("Retrieving album covers for %n album(s)...", "", album_covers_requested_));
    Q_EMIT ProgressSetMaximum(album_covers_requested_);
    Q_EMIT UpdateProgress(0);
  }

}

namespace {

// Returns the index of the thumb path in the path of a cover URL, the server path can also include /library/, for example behind a reverse proxy, so it's the last one.
qsizetype ThumbIndex(const QString &path) {
  return path.lastIndexOf("/library/"_L1);
}

}  // namespace

QString PlexRequest::CoverImageId(const QUrl &cover_url) {

  if (!cover_url.isValid() || cover_url.isLocalFile()) return QString();

  // The thumb path is /library/metadata/<id>/thumb/<timestamp>, the timestamp changes when the image changes.
  // Songs of the same album share the album thumb, so the image is identified by the thumb path, and downloaded once.
  // The path is the server path followed by the thumb path.
  const qsizetype thumb_index = ThumbIndex(cover_url.path());
  if (thumb_index == -1) return QString();
  const QStringList thumb_parts = cover_url.path().mid(thumb_index + 9).split(u'/', Qt::SkipEmptyParts);
  // The thumb path from the server is used in the cache filename, so only allow letters and digits, which Plex uses.
  static const QRegularExpression regex_id(u"^[A-Za-z0-9]+$"_s);
  if (thumb_parts.isEmpty()) return QString();
  for (const QString &thumb_part : thumb_parts) {
    if (!regex_id.match(thumb_part).hasMatch()) return QString();
  }

  return thumb_parts.join(u'-');

}

namespace {

// A server URL entered in the settings can have a query, which can include the access token, and a path ending with a slash.
QUrl ServerUrlWithoutQuery(const QUrl &server_url) {

  QUrl url = server_url.adjusted(QUrl::RemoveQuery | QUrl::RemoveFragment | QUrl::RemoveUserInfo);
  QString path = url.path();
  while (path.endsWith(u'/')) path.chop(1);
  url.setPath(path);
  return url;

}

}  // namespace

QUrl PlexRequest::CoverUrl(const QUrl &server_url, const QString &thumb) {

  QUrl cover_url = ServerUrlWithoutQuery(server_url);
  cover_url.setPath(cover_url.path() + thumb);
  return cover_url;

}

QUrl PlexRequest::CoverUrlForServer(const QUrl &cover_url, const QUrl &server_url) {

  if (cover_url.isLocalFile()) {
    const QString filename = cover_url.toLocalFile();
    if (QFile::exists(filename)) return cover_url;
    // The cache filename is the image ID, which is the thumb path with the slashes replaced, so the cover can be downloaded again.
    const QString image_id = QFileInfo(filename).completeBaseName();
    const QUrl server_cover_url = CoverUrl(server_url, "/library/"_L1 + QString(image_id).replace(u'-', u'/'));
    if (!server_url.isValid() || CoverImageId(server_cover_url) != image_id) return cover_url;
    return server_cover_url;
  }

  // Covers from somewhere else than the Plex server are kept as they are.
  if (CoverImageId(cover_url).isEmpty()) return cover_url;

  const QString thumb = cover_url.path().mid(ThumbIndex(cover_url.path()));
  if (server_url.isValid()) return CoverUrl(server_url, thumb);

  QUrl updated_cover_url = cover_url;
  updated_cover_url.setQuery(QString());
  return updated_cover_url;

}

QString PlexRequest::CoverCacheDir(const QString &server_machine_identifier, const QUrl &server_url) {

  // A server selected in the settings is identified by its machine identifier, a manually configured server only by its address.
  static const QRegularExpression regex_machine_identifier(u"^[A-Za-z0-9]+$"_s);
  QString server_dir;
  if (regex_machine_identifier.match(server_machine_identifier).hasMatch()) {
    server_dir = server_machine_identifier;
  }
  else {
    server_dir = QString::fromLatin1(QCryptographicHash::hash(ServerUrlWithoutQuery(server_url).toString().toUtf8(), QCryptographicHash::Sha1).toHex());
  }

  return Song::ImageCacheDir(Song::Source::Plex) + QLatin1Char('/') + server_dir;

}

void PlexRequest::AddAlbumCoverRequest(const Song &song) {

  const QUrl cover_url = song.art_automatic();
  const QString image_id = CoverImageId(cover_url);
  if (image_id.isEmpty()) return;

  const QString cover_path = CoverCacheDir(server_machine_identifier(), server_url());
  QDir dir(cover_path);
  if (!dir.exists()) dir.mkpath(cover_path);

  AlbumCoverRequest request;
  request.image_id = image_id;
  request.url = cover_url;
  request.filename = cover_path + QLatin1Char('/') + CoverUtils::CoverFilenameFromSource(Song::Source::Plex, cover_url, song.effective_albumartist(), song.album(), image_id, u"jpg"_s);

  // The cover is already in the cache, so reuse it instead of downloading it on every sync.
  if (QFile::exists(request.filename)) {
    if (songs_.contains(song.song_id())) {
      songs_[song.song_id()].set_art_automatic(QUrl::fromLocalFile(request.filename));
    }
    return;
  }

  // Record the song ID under the shared image ID so that every song of the album gets the downloaded cover, while only one request is sent for each image.
  const bool already_requested = album_covers_requests_sent_.contains(image_id);
  album_covers_requests_sent_[image_id] << song.song_id();
  if (already_requested) return;

  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void PlexRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    const AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkReply *reply = CreateImageRequest(request.url);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request); });

  }

}

void PlexRequest::AlbumCoverReceived(QNetworkReply *reply, const AlbumCoverRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --album_covers_requests_active_;
  ++album_covers_received_;

  const QScopeGuard finish_check = qScopeGuard([this]() {
    FlushAlbumCoverRequests();
    FinishCheck();
  });

  if (finished_) return;

  Q_EMIT UpdateProgress(album_covers_received_);

  // Covers are optional, so failed cover downloads are only logged, they don't make the results an error.
  // The songs keep the cover URL, so the cover is downloaded again on the next sync.

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Warning) << "Plex:" << reply->errorString() << reply->error() << "for" << Utilities::UrlForLog(request.url);
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_status_code != 200) {
    qLog(Warning) << "Plex: Received HTTP code" << http_status_code << "for" << Utilities::UrlForLog(request.url);
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    qLog(Warning) << "Plex: Unsupported mimetype for image reader" << mimetype << "for" << Utilities::UrlForLog(request.url);
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    qLog(Warning) << "Plex: Received empty image data for" << Utilities::UrlForLog(request.url);
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  QByteArrayList format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list[0].data();
  }

  QImage image;
  if (!image.loadFromData(data, format)) {
    qLog(Warning) << "Plex: Error decoding image data from" << Utilities::UrlForLog(request.url);
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  // The cache filename is always .jpg, and the file is saved atomically, so a failed save doesn't leave a partial file, which would be used as the cover.
  QSaveFile file(request.filename);
  if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "JPG") || !file.commit()) {
    qLog(Warning) << "Plex: Error saving image data to" << request.filename << file.errorString();
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  const QStringList song_ids = album_covers_requests_sent_.take(request.image_id);
  for (const QString &song_id : song_ids) {
    if (songs_.contains(song_id)) {
      songs_[song_id].set_art_automatic(QUrl::fromLocalFile(request.filename));
    }
  }

}

void PlexRequest::FinishCheck() {

  if (finished_ || songs_requests_active_ > 0) return;

  // Results with an error are still saved, only the songs missing from them are not removed, so the covers are also downloaded for them.
  if (!album_covers_started_ && download_album_covers()) {
    album_covers_started_ = true;
    GetAlbumCovers();
  }

  if (!album_cover_requests_queue_.isEmpty() || album_covers_requests_active_ > 0) return;

  finished_ = true;
  Q_EMIT Results(songs_, Utilities::StringListToHTML(errors_));

}

void PlexRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Plex:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}
