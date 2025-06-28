/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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
#include <functional>
#include <chrono>
#include <memory>

#include <QObject>
#include <QString>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "core/song.h"
#include "core/logging.h"
#include "core/standardpaths.h"

#include "scrobblercache.h"
#include "scrobblercacheitem.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;
using std::make_shared;

ScrobblerCache::ScrobblerCache(const QString &filename, QObject *parent)
    : QObject(parent),
      timer_flush_(new QTimer(this)),
      filename_(StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + QLatin1Char('/') + filename),
      loaded_(false) {

  ReadCache();
  loaded_ = true;

  timer_flush_->setSingleShot(true);
  timer_flush_->setInterval(10min);
  QObject::connect(timer_flush_, &QTimer::timeout, this, &ScrobblerCache::WriteCache);

}

ScrobblerCache::~ScrobblerCache() {
  scrobbler_cache_.clear();
}

void ScrobblerCache::ReadCache() {

  QFile file(filename_);
  bool result = file.open(QIODevice::ReadOnly | QIODevice::Text);
  if (!result) return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Encoding::Utf8);
  QString data = stream.readAll();
  file.close();

  if (data.isEmpty()) return;

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError) {
    qLog(Error) << "Scrobbler cache is missing JSON data.";
    return;
  }
  if (json_doc.isEmpty()) {
    qLog(Error) << "Scrobbler cache has empty JSON document.";
    return;
  }
  if (!json_doc.isObject()) {
    qLog(Error) << "Scrobbler cache JSON document is not an object.";
    return;
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    qLog(Error) << "Scrobbler cache has empty JSON object.";
    return;
  }
  if (!json_obj.contains("tracks"_L1)) {
    qLog(Error) << "Scrobbler cache is missing JSON tracks.";
    return;
  }
  QJsonValue json_tracks = json_obj["tracks"_L1];
  if (!json_tracks.isArray()) {
    qLog(Error) << "Scrobbler cache JSON tracks is not an array.";
    return;
  }
  const QJsonArray json_array = json_tracks.toArray();
  if (json_array.isEmpty()) {
    return;
  }

  for (const QJsonValue &value : json_array) {
    if (!value.isObject()) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is not an object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj_track = value.toObject();
    if (
        !json_obj_track.contains("timestamp"_L1) ||
        !json_obj_track.contains("artist"_L1) ||
        !json_obj_track.contains("album"_L1) ||
        !json_obj_track.contains("title"_L1) ||
        !json_obj_track.contains("track"_L1) ||
        !json_obj_track.contains("albumartist"_L1) ||
        !json_obj_track.contains("length_nanosec"_L1)
    ) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is missing data.";
      qLog(Debug) << value;
      continue;
    }

    ScrobbleMetadata metadata;
    quint64 timestamp = json_obj_track["timestamp"_L1].toVariant().toULongLong();
    metadata.artist = json_obj_track["artist"_L1].toString();
    metadata.album = json_obj_track["album"_L1].toString();
    metadata.title = json_obj_track["title"_L1].toString();
    metadata.track = json_obj_track["track"_L1].toInt();
    metadata.albumartist = json_obj_track["albumartist"_L1].toString();
    metadata.length_nanosec = json_obj_track["length_nanosec"_L1].toVariant().toLongLong();

    if (timestamp == 0 || metadata.artist.isEmpty() || metadata.title.isEmpty() || metadata.length_nanosec <= 0) {
      qLog(Error) << "Invalid cache data" << "for song" << metadata.title;
      continue;
    }

    if (json_obj_track.contains("grouping"_L1)) {
      metadata.grouping = json_obj_track["grouping"_L1].toString();
    }

    if (json_obj_track.contains("musicbrainz_album_artist_id"_L1)) {
      metadata.musicbrainz_album_artist_id = json_obj_track["musicbrainz_album_artist_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_artist_id"_L1)) {
      metadata.musicbrainz_artist_id = json_obj_track["musicbrainz_artist_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_original_artist_id"_L1)) {
      metadata.musicbrainz_original_artist_id = json_obj_track["musicbrainz_original_artist_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_album_id"_L1)) {
      metadata.musicbrainz_album_id = json_obj_track["musicbrainz_album_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_original_album_id"_L1)) {
      metadata.musicbrainz_original_album_id = json_obj_track["musicbrainz_original_album_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_recording_id"_L1)) {
      metadata.musicbrainz_recording_id = json_obj_track["musicbrainz_recording_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_track_id"_L1)) {
      metadata.musicbrainz_track_id = json_obj_track["musicbrainz_track_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_disc_id"_L1)) {
      metadata.musicbrainz_disc_id = json_obj_track["musicbrainz_disc_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_release_group_id"_L1)) {
      metadata.musicbrainz_release_group_id = json_obj_track["musicbrainz_release_group_id"_L1].toString();
    }
    if (json_obj_track.contains("musicbrainz_work_id"_L1)) {
      metadata.musicbrainz_work_id = json_obj_track["musicbrainz_work_id"_L1].toString();
    }
    if (json_obj_track.contains("music_service"_L1)) {
      metadata.music_service = json_obj_track["music_service"_L1].toString();
    }
    if (json_obj_track.contains("music_service_name"_L1)) {
      metadata.music_service_name = json_obj_track["music_service_name"_L1].toString();
    }
    if (json_obj_track.contains("share_url"_L1)) {
      metadata.share_url = json_obj_track["share_url"_L1].toString();
    }
    if (json_obj_track.contains("spotify_id"_L1)) {
      metadata.spotify_id = json_obj_track["spotify_id"_L1].toString();
    }

    ScrobblerCacheItemPtr cache_item = make_shared<ScrobblerCacheItem>(metadata, timestamp);
    scrobbler_cache_ << cache_item;

  }

}

void ScrobblerCache::WriteCache() {

  if (!loaded_) return;

  qLog(Debug) << "Writing scrobbler cache file" << filename_;

  if (scrobbler_cache_.isEmpty()) {
    QFile file(filename_);
    if (file.exists()) file.remove();
    return;
  }

  QJsonArray array;
  for (ScrobblerCacheItemPtr cache_item : std::as_const(scrobbler_cache_)) {
    QJsonObject object;
    object.insert("timestamp"_L1, QJsonValue::fromVariant(cache_item->timestamp));
    object.insert("artist"_L1, QJsonValue::fromVariant(cache_item->metadata.artist));
    object.insert("album"_L1, QJsonValue::fromVariant(cache_item->metadata.album));
    object.insert("title"_L1, QJsonValue::fromVariant(cache_item->metadata.title));
    object.insert("track"_L1, QJsonValue::fromVariant(cache_item->metadata.track));
    object.insert("albumartist"_L1, QJsonValue::fromVariant(cache_item->metadata.albumartist));
    object.insert("grouping"_L1, QJsonValue::fromVariant(cache_item->metadata.grouping));
    object.insert("musicbrainz_album_artist_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_artist_id));
    object.insert("musicbrainz_artist_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_artist_id));
    object.insert("musicbrainz_original_artist_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_artist_id));
    object.insert("musicbrainz_album_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_id));
    object.insert("musicbrainz_original_album_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_album_id));
    object.insert("musicbrainz_recording_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_recording_id));
    object.insert("musicbrainz_track_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_track_id));
    object.insert("musicbrainz_disc_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_disc_id));
    object.insert("musicbrainz_release_group_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_release_group_id));
    object.insert("musicbrainz_work_id"_L1, QJsonValue::fromVariant(cache_item->metadata.musicbrainz_work_id));
    object.insert("music_service"_L1, QJsonValue::fromVariant(cache_item->metadata.music_service));
    object.insert("music_service_name"_L1, QJsonValue::fromVariant(cache_item->metadata.music_service_name));
    object.insert("share_url"_L1, QJsonValue::fromVariant(cache_item->metadata.share_url));
    object.insert("spotify_id"_L1, QJsonValue::fromVariant(cache_item->metadata.spotify_id));
    object.insert("length_nanosec"_L1, QJsonValue::fromVariant(cache_item->metadata.length_nanosec));
    array.append(QJsonValue::fromVariant(object));
  }

  QJsonObject object;
  object.insert("tracks"_L1, array);
  QJsonDocument doc(object);

  QFile file(filename_);
  bool result = file.open(QIODevice::WriteOnly | QIODevice::Text);
  if (!result) {
    qLog(Error) << "Unable to open scrobbler cache file" << filename_;
    return;
  }
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Encoding::Utf8);
  stream << doc.toJson();
  file.close();

}

ScrobblerCacheItemPtr ScrobblerCache::Add(const Song &song, const quint64 timestamp) {

  ScrobblerCacheItemPtr cache_item = make_shared<ScrobblerCacheItem>(ScrobbleMetadata(song), timestamp);

  scrobbler_cache_ << cache_item;

  if (loaded_ && !timer_flush_->isActive()) {
    timer_flush_->start();
  }

  return cache_item;

}

void ScrobblerCache::Remove(ScrobblerCacheItemPtr cache_item) {

  if (scrobbler_cache_.contains(cache_item)) {
    scrobbler_cache_.removeAll(cache_item);
  }
}

void ScrobblerCache::ClearSent(ScrobblerCacheItemPtrList cache_items) {

  for (ScrobblerCacheItemPtr cache_item : cache_items) {
    cache_item->sent = false;
  }

}

void ScrobblerCache::SetError(ScrobblerCacheItemPtrList cache_items) {

  for (ScrobblerCacheItemPtr item : cache_items) {
    item->error = true;
  }

}

void ScrobblerCache::Flush(ScrobblerCacheItemPtrList cache_items) {

  for (ScrobblerCacheItemPtr cache_item : cache_items) {
    if (scrobbler_cache_.contains(cache_item)) {
      scrobbler_cache_.removeAll(cache_item);
    }
  }

  if (!timer_flush_->isActive()) {
    timer_flush_->start();
  }

}
