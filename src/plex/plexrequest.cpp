/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "core/song.h"
#include "utilities/strutils.h"
#include "constants/timeconstants.h"
#include "plexservice.h"
#include "plexurlhandler.h"
#include "plexbaserequest.h"
#include "plexrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kContainerSize = 500;
}  // namespace

PlexRequest::PlexRequest(PlexService *service, PlexUrlHandler *url_handler, const SongMap &existing_songs, const qint64 updated_since, QObject *parent)
    : PlexBaseRequest(service, parent),
      service_(service),
      url_handler_(url_handler),
      finished_(false),
      incremental_(updated_since > 0 && !existing_songs.isEmpty()),
      updated_since_(updated_since),
      newest_updated_at_(0),
      server_total_songs_(0),
      songs_requests_active_(0),
      songs_total_(0),
      songs_received_(0) {

  if (incremental_) {
    songs_ = existing_songs;
    // Cover URLs carry the access token, refresh it in case the token changed since the last sync.
    for (Song &song : songs_) {
      QUrl cover_url = song.art_automatic();
      if (cover_url.isEmpty() || cover_url.isLocalFile()) continue;
      QUrlQuery cover_url_query(cover_url);
      if (!cover_url_query.hasQueryItem(u"X-Plex-Token"_s)) continue;
      cover_url_query.removeAllQueryItems(u"X-Plex-Token"_s);
      cover_url_query.addQueryItem(u"X-Plex-Token"_s, token());
      cover_url.setQuery(cover_url_query);
      song.set_art_automatic(cover_url);
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

  songs_total_ += array_metadata.count();

  for (const QJsonValue &value_metadata : array_metadata) {
    if (!value_metadata.isObject()) continue;
    ParseSong(value_metadata.toObject());
    ++songs_received_;
  }

  Q_EMIT ProgressSetMaximum(total_size > 0 ? total_size : songs_total_);
  Q_EMIT UpdateProgress(songs_received_);

  if (!array_metadata.isEmpty() && offset_requested + array_metadata.count() < total_size) {
    GetSectionSongs(section_key, offset_requested + array_metadata.count());
  }
  else {
    --songs_requests_active_;
  }

  SongsFinishCheck();

}

void PlexRequest::ParseSong(const QJsonObject &object_metadata) {

  if (!object_metadata.contains("ratingKey"_L1) || !object_metadata.contains("title"_L1) || !object_metadata.contains("Media"_L1)) {
    Error(u"Invalid Json reply, song is missing one or more values."_s, object_metadata);
    return;
  }

  const QString song_id = object_metadata["ratingKey"_L1].toString();
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
    Error(u"Invalid Json reply, song is missing Media."_s, object_metadata);
    return;
  }
  const QJsonObject object_media = array_media.first().toObject();
  const int bitrate = object_media["bitrate"_L1].toInt();
  const QString container = object_media["container"_L1].toString();

  const QJsonArray array_part = object_media["Part"_L1].toArray();
  if (array_part.isEmpty() || !array_part.first().isObject()) {
    Error(u"Invalid Json reply, song is missing Part."_s, object_metadata);
    return;
  }
  const QJsonObject object_part = array_part.first().toObject();
  const QString part_key = object_part["key"_L1].toString();
  if (part_key.isEmpty()) {
    Error(u"Invalid Json reply, song is missing Part key."_s, object_metadata);
    return;
  }
  const qint64 filesize = object_part["size"_L1].toVariant().toLongLong();

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(part_key);

  QString thumb = object_metadata["parentThumb"_L1].toString();
  if (thumb.isEmpty()) thumb = object_metadata["thumb"_L1].toString();
  QUrl cover_url;
  if (download_album_covers() && !thumb.isEmpty()) {
    cover_url = server_url();
    cover_url.setPath(cover_url.path() + thumb);
    QUrlQuery cover_url_query;
    cover_url_query.addQueryItem(u"X-Plex-Token"_s, token());
    cover_url.setQuery(cover_url_query);
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

}

void PlexRequest::SongsFinishCheck() {

  if (finished_) return;

  if (songs_requests_active_ > 0) return;

  // The incremental fetch cannot detect deleted songs, so compare the server's total track count with what we have and redo a full sync on mismatch.
  if (incremental_ && server_total_songs_ != songs_.count()) {
    qLog(Debug) << "Plex: Song count mismatch (server:" << server_total_songs_ << "local:" << songs_.count() << "), doing full sync";
    incremental_ = false;
    updated_since_ = 0;
    server_total_songs_ = 0;
    songs_total_ = 0;
    songs_received_ = 0;
    newest_updated_at_ = 0;
    songs_.clear();
    GetLibrarySections();
    return;
  }

  FinishCheck();

}

void PlexRequest::FinishCheck() {

  if (!finished_ && songs_requests_active_ <= 0) {
    finished_ = true;
    Q_EMIT Results(songs_, Utilities::StringListToHTML(errors_));
  }

}

void PlexRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Plex:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}
