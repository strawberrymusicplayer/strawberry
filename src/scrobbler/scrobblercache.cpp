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
#include <QStandardPaths>
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

using std::make_shared;
using namespace std::chrono_literals;

ScrobblerCache::ScrobblerCache(const QString &filename, QObject *parent)
    : QObject(parent),
      timer_flush_(new QTimer(this)),
      filename_(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QLatin1Char('/') + filename),
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
  if (!json_obj.contains(QLatin1String("tracks"))) {
    qLog(Error) << "Scrobbler cache is missing JSON tracks.";
    return;
  }
  QJsonValue json_tracks = json_obj[QLatin1String("tracks")];
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
        !json_obj_track.contains(QLatin1String("timestamp")) ||
        !json_obj_track.contains(QLatin1String("artist")) ||
        !json_obj_track.contains(QLatin1String("album")) ||
        !json_obj_track.contains(QLatin1String("title")) ||
        !json_obj_track.contains(QLatin1String("track")) ||
        !json_obj_track.contains(QLatin1String("albumartist")) ||
        !json_obj_track.contains(QLatin1String("length_nanosec"))
    ) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is missing data.";
      qLog(Debug) << value;
      continue;
    }

    ScrobbleMetadata metadata;
    quint64 timestamp = json_obj_track[QLatin1String("timestamp")].toVariant().toULongLong();
    metadata.artist = json_obj_track[QLatin1String("artist")].toString();
    metadata.album = json_obj_track[QLatin1String("album")].toString();
    metadata.title = json_obj_track[QLatin1String("title")].toString();
    metadata.track = json_obj_track[QLatin1String("track")].toInt();
    metadata.albumartist = json_obj_track[QLatin1String("albumartist")].toString();
    metadata.length_nanosec = json_obj_track[QLatin1String("length_nanosec")].toVariant().toLongLong();

    if (timestamp == 0 || metadata.artist.isEmpty() || metadata.title.isEmpty() || metadata.length_nanosec <= 0) {
      qLog(Error) << "Invalid cache data" << "for song" << metadata.title;
      continue;
    }

    if (json_obj_track.contains(QLatin1String("grouping"))) {
      metadata.grouping = json_obj_track[QLatin1String("grouping")].toString();
    }

    if (json_obj_track.contains(QLatin1String("musicbrainz_album_artist_id"))) {
      metadata.musicbrainz_album_artist_id = json_obj_track[QLatin1String("musicbrainz_album_artist_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_artist_id"))) {
      metadata.musicbrainz_artist_id = json_obj_track[QLatin1String("musicbrainz_artist_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_original_artist_id"))) {
      metadata.musicbrainz_original_artist_id = json_obj_track[QLatin1String("musicbrainz_original_artist_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_album_id"))) {
      metadata.musicbrainz_album_id = json_obj_track[QLatin1String("musicbrainz_album_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_original_album_id"))) {
      metadata.musicbrainz_original_album_id = json_obj_track[QLatin1String("musicbrainz_original_album_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_recording_id"))) {
      metadata.musicbrainz_recording_id = json_obj_track[QLatin1String("musicbrainz_recording_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_track_id"))) {
      metadata.musicbrainz_track_id = json_obj_track[QLatin1String("musicbrainz_track_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_disc_id"))) {
      metadata.musicbrainz_disc_id = json_obj_track[QLatin1String("musicbrainz_disc_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_release_group_id"))) {
      metadata.musicbrainz_release_group_id = json_obj_track[QLatin1String("musicbrainz_release_group_id")].toString();
    }
    if (json_obj_track.contains(QLatin1String("musicbrainz_work_id"))) {
      metadata.musicbrainz_work_id = json_obj_track[QLatin1String("musicbrainz_work_id")].toString();
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
    object.insert(QLatin1String("timestamp"), QJsonValue::fromVariant(cache_item->timestamp));
    object.insert(QLatin1String("artist"), QJsonValue::fromVariant(cache_item->metadata.artist));
    object.insert(QLatin1String("album"), QJsonValue::fromVariant(cache_item->metadata.album));
    object.insert(QLatin1String("title"), QJsonValue::fromVariant(cache_item->metadata.title));
    object.insert(QLatin1String("track"), QJsonValue::fromVariant(cache_item->metadata.track));
    object.insert(QLatin1String("albumartist"), QJsonValue::fromVariant(cache_item->metadata.albumartist));
    object.insert(QLatin1String("grouping"), QJsonValue::fromVariant(cache_item->metadata.grouping));
    object.insert(QLatin1String("musicbrainz_album_artist_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_artist_id));
    object.insert(QLatin1String("musicbrainz_artist_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_artist_id));
    object.insert(QLatin1String("musicbrainz_original_artist_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_artist_id));
    object.insert(QLatin1String("musicbrainz_album_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_album_id));
    object.insert(QLatin1String("musicbrainz_original_album_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_original_album_id));
    object.insert(QLatin1String("musicbrainz_recording_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_recording_id));
    object.insert(QLatin1String("musicbrainz_track_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_track_id));
    object.insert(QLatin1String("musicbrainz_disc_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_disc_id));
    object.insert(QLatin1String("musicbrainz_release_group_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_release_group_id));
    object.insert(QLatin1String("musicbrainz_work_id"), QJsonValue::fromVariant(cache_item->metadata.musicbrainz_work_id));
    object.insert(QLatin1String("length_nanosec"), QJsonValue::fromVariant(cache_item->metadata.length_nanosec));
    array.append(QJsonValue::fromVariant(object));
  }

  QJsonObject object;
  object.insert(QLatin1String("tracks"), array);
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
