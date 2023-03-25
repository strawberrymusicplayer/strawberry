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

#include <memory>
#include <functional>
#include <chrono>

#include <QObject>
#include <QStandardPaths>
#include <QHash>
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

#include "scrobblercache.h"
#include "scrobblercacheitem.h"

using namespace std::chrono_literals;

ScrobblerCache::ScrobblerCache(const QString &filename, QObject *parent)
    : QObject(parent),
      timer_flush_(new QTimer(this)),
      filename_(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + filename),
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
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  stream.setEncoding(QStringConverter::Encoding::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
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
  if (!json_obj.contains("tracks")) {
    qLog(Error) << "Scrobbler cache is missing JSON tracks.";
    return;
  }
  QJsonValue json_tracks = json_obj["tracks"];
  if (!json_tracks.isArray()) {
    qLog(Error) << "Scrobbler cache JSON tracks is not an array.";
    return;
  }
  QJsonArray json_array = json_tracks.toArray();
  if (json_array.isEmpty()) {
    return;
  }

  for (const QJsonValueRef value : json_array) {
    if (!value.isObject()) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is not an object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj_track = value.toObject();
    if (
        !json_obj_track.contains("timestamp") ||
        !json_obj_track.contains("artist") ||
        !json_obj_track.contains("album") ||
        !json_obj_track.contains("title") ||
        !json_obj_track.contains("track") ||
        !json_obj_track.contains("albumartist") ||
        !json_obj_track.contains("length_nanosec")
    ) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is missing data.";
      qLog(Debug) << value;
      continue;
    }

    ScrobbleMetadata metadata;
    quint64 timestamp = json_obj_track["timestamp"].toVariant().toULongLong();
    metadata.artist = json_obj_track["artist"].toString();
    metadata.album = json_obj_track["album"].toString();
    metadata.title = json_obj_track["title"].toString();
    metadata.track = json_obj_track["track"].toInt();
    metadata.albumartist = json_obj_track["albumartist"].toString();
    metadata.length_nanosec = json_obj_track["length_nanosec"].toVariant().toLongLong();

    if (timestamp <= 0 || metadata.artist.isEmpty() || metadata.title.isEmpty() || metadata.length_nanosec <= 0) {
      qLog(Error) << "Invalid cache data" << "for song" << metadata.title;
      continue;
    }

    if (json_obj_track.contains("grouping")) {
      metadata.grouping = json_obj_track["grouping"].toString();
    }

    if (json_obj_track.contains("musicbrainz_album_artist_id")) {
      metadata.musicbrainz_album_artist_id = json_obj_track["musicbrainz_album_artist_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_artist_id")) {
      metadata.musicbrainz_artist_id = json_obj_track["musicbrainz_artist_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_original_artist_id")) {
      metadata.musicbrainz_original_artist_id = json_obj_track["musicbrainz_original_artist_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_album_id")) {
      metadata.musicbrainz_album_id = json_obj_track["musicbrainz_album_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_original_album_id")) {
      metadata.musicbrainz_original_album_id = json_obj_track["musicbrainz_original_album_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_recording_id")) {
      metadata.musicbrainz_recording_id = json_obj_track["musicbrainz_recording_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_track_id")) {
      metadata.musicbrainz_track_id = json_obj_track["musicbrainz_track_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_disc_id")) {
      metadata.musicbrainz_disc_id = json_obj_track["musicbrainz_disc_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_release_group_id")) {
      metadata.musicbrainz_release_group_id = json_obj_track["musicbrainz_release_group_id"].toString();
    }
    if (json_obj_track.contains("musicbrainz_work_id")) {
      metadata.musicbrainz_work_id = json_obj_track["musicbrainz_work_id"].toString();
    }

    if (scrobbler_cache_.contains(timestamp)) continue;
    std::shared_ptr<ScrobblerCacheItem> cache_item = std::make_shared<ScrobblerCacheItem>(metadata, timestamp);
    scrobbler_cache_.insert(timestamp, cache_item);

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
  for (QHash <quint64, std::shared_ptr<ScrobblerCacheItem>> ::iterator i = scrobbler_cache_.begin(); i != scrobbler_cache_.end(); ++i) {
    ScrobblerCacheItemPtr cache_item = i.value();
    QJsonObject object;
    object.insert("timestamp", QJsonValue::fromVariant(cache_item->timestamp));
    object.insert("artist", QJsonValue::fromVariant(cache_item->metadata.artist));
    object.insert("album", QJsonValue::fromVariant(cache_item->metadata.album));
    object.insert("title", QJsonValue::fromVariant(cache_item->metadata.title));
    object.insert("track", QJsonValue::fromVariant(cache_item->metadata.track));
    object.insert("albumartist", QJsonValue::fromVariant(cache_item->metadata.albumartist));
    object.insert("grouping", QJsonValue::fromVariant(cache_item->metadata.grouping));
    object.insert("musicbrainz_album_artist_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_artist_id));
    object.insert("musicbrainz_artist_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_artist_id));
    object.insert("musicbrainz_original_artist_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_artist_id));
    object.insert("musicbrainz_album_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_id));
    object.insert("musicbrainz_original_album_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_album_id));
    object.insert("musicbrainz_recording_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_recording_id));
    object.insert("musicbrainz_track_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_track_id));
    object.insert("musicbrainz_disc_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_disc_id));
    object.insert("musicbrainz_release_group_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_release_group_id));
    object.insert("musicbrainz_work_id", QJsonValue::fromVariant(cache_item->metadata.musicbrainz_work_id));
    object.insert("length_nanosec", QJsonValue::fromVariant(cache_item->metadata.length_nanosec));
    array.append(QJsonValue::fromVariant(object));
  }

  QJsonObject object;
  object.insert("tracks", array);
  QJsonDocument doc(object);

  QFile file(filename_);
  bool result = file.open(QIODevice::WriteOnly | QIODevice::Text);
  if (!result) {
    qLog(Error) << "Unable to open scrobbler cache file" << filename_;
    return;
  }
  QTextStream stream(&file);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  stream.setEncoding(QStringConverter::Encoding::Utf8);
#else
  stream.setCodec("UTF-8");
#endif
  stream << doc.toJson();
  file.close();

}

ScrobblerCacheItemPtr ScrobblerCache::Add(const Song &song, const quint64 timestamp) {

  if (scrobbler_cache_.contains(timestamp)) return nullptr;

  ScrobblerCacheItemPtr cache_item = std::make_shared<ScrobblerCacheItem>(ScrobbleMetadata(song), timestamp);

  scrobbler_cache_.insert(timestamp, cache_item);

  if (loaded_ && !timer_flush_->isActive()) {
    timer_flush_->start();
  }

  return cache_item;

}

ScrobblerCacheItemPtr ScrobblerCache::Get(const quint64 hash) {

  if (scrobbler_cache_.contains(hash)) { return scrobbler_cache_.value(hash); }
  else return nullptr;

}

void ScrobblerCache::Remove(const quint64 hash) {

  if (!scrobbler_cache_.contains(hash)) {
    qLog(Error) << "Tried to remove non-existing hash" << hash;
    return;
  }

  scrobbler_cache_.remove(hash);

}

void ScrobblerCache::Remove(ScrobblerCacheItemPtr item) {
  scrobbler_cache_.remove(item->timestamp);
}

void ScrobblerCache::ClearSent(const QList<quint64> &list) {

  for (const quint64 timestamp : list) {
    if (!scrobbler_cache_.contains(timestamp)) continue;
    ScrobblerCacheItemPtr item = scrobbler_cache_.value(timestamp);
    item->sent = false;
  }

}

void ScrobblerCache::Flush(const QList<quint64> &list) {

  for (const quint64 timestamp : list) {
    if (!scrobbler_cache_.contains(timestamp)) continue;
    scrobbler_cache_.remove(timestamp);
  }

  if (!timer_flush_->isActive()) {
    timer_flush_->start();
  }

}
