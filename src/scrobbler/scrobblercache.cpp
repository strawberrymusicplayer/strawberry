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

#include <memory>

#include <QObject>
#include <QStandardPaths>
#include <QHash>
#include <QVariant>
#include <QString>
#include <QFile>
#include <QIODevice>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QtDebug>

#include "core/song.h"
#include "core/logging.h"
#include "core/closure.h"

#include "scrobblercache.h"
#include "scrobblercacheitem.h"

ScrobblerCache::ScrobblerCache(const QString &filename, QObject *parent) :
  QObject(parent),
  filename_(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + filename),
  loaded_(false) {
  ReadCache();
  loaded_ = true;
}

ScrobblerCache::~ScrobblerCache() {
  scrobbler_cache_.clear();
}

void ScrobblerCache::ReadCache() {

  QFile file(filename_);
  bool result = file.open(QIODevice::ReadOnly | QIODevice::Text);
  if (!result) return;

  QTextStream stream(&file);
  stream.setCodec("UTF-8");
  QString data = stream.readAll();
  file.close();

  if (data.isEmpty()) return;

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data.toUtf8(), &error);
  if (error.error != QJsonParseError::NoError) {
    qLog(Error) << "Scrobbler cache is missing JSON data.";
    return;
  }
  if (json_doc.isNull() || json_doc.isEmpty()) {
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

  for (const QJsonValue &value : json_array) {
    if (!value.isObject()) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is not an object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj_track = value.toObject();
    if (
        !json_obj_track.contains("timestamp") ||
        !json_obj_track.contains("song") ||
        !json_obj_track.contains("album") ||
        !json_obj_track.contains("artist") ||
        !json_obj_track.contains("albumartist") ||
        !json_obj_track.contains("track") ||
        !json_obj_track.contains("duration")
    ) {
      qLog(Error) << "Scrobbler cache JSON tracks array value is missing data.";
      qLog(Debug) << value;
      continue;
    }

    quint64 timestamp = json_obj_track["timestamp"].toVariant().toULongLong();
    QString artist = json_obj_track["artist"].toString();
    QString album = json_obj_track["album"].toString();
    QString song = json_obj_track["song"].toString();
    QString albumartist = json_obj_track["albumartist"].toString();
    int track = json_obj_track["track"].toInt();
    qint64 duration = json_obj_track["duration"].toVariant().toLongLong();

    if (timestamp <= 0 || artist.isEmpty() || album.isEmpty() || song.isEmpty() || duration <= 0) {
      qLog(Error) << "Invalid cache data" << "for song" << song;
      continue;
    }
    if (scrobbler_cache_.contains(timestamp)) continue;
    scrobbler_cache_.insert(timestamp, std::make_shared<ScrobblerCacheItem>(artist, album, song, albumartist, track, duration, timestamp));

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

  QHash <quint64, std::shared_ptr<ScrobblerCacheItem>> ::iterator i;
  for (i = scrobbler_cache_.begin() ; i != scrobbler_cache_.end() ; ++i) {
    ScrobblerCacheItemPtr item = i.value();
    QJsonObject object;
    object.insert("timestamp", QJsonValue::fromVariant(item->timestamp_));
    object.insert("artist", QJsonValue::fromVariant(item->artist_));
    object.insert("album", QJsonValue::fromVariant(item->album_));
    object.insert("song", QJsonValue::fromVariant(item->song_));
    object.insert("albumartist", QJsonValue::fromVariant(item->albumartist_));
    object.insert("track", QJsonValue::fromVariant(item->track_));
    object.insert("duration", QJsonValue::fromVariant(item->duration_));
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
  stream.setCodec("UTF-8");
  stream << doc.toJson();
  file.close();

}

ScrobblerCacheItemPtr ScrobblerCache::Add(const Song &song, const quint64 &timestamp) {

  if (scrobbler_cache_.contains(timestamp)) return nullptr;

  QString album = song.album();
  QString title = song.title();

  album.remove(Song::kAlbumRemoveDisc);
  album.remove(Song::kAlbumRemoveMisc);
  title.remove(Song::kTitleRemoveMisc);

  ScrobblerCacheItemPtr item = std::make_shared<ScrobblerCacheItem>(song.artist(), album, title, song.albumartist(), song.track(), song.length_nanosec(), timestamp);
  scrobbler_cache_.insert(timestamp, item);

  if (loaded_) DoInAMinuteOrSo(this, SLOT(WriteCache()));

  return item;

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
  scrobbler_cache_.remove(item->timestamp_);
}

void ScrobblerCache::ClearSent(const QList<quint64> &list) {

  for (const quint64 timestamp : list) {
    if (!scrobbler_cache_.contains(timestamp)) continue;
    ScrobblerCacheItemPtr item = scrobbler_cache_.take(timestamp);
    item->sent_ = false;
  }

}

void ScrobblerCache::Flush(const QList<quint64> &list) {

  for (const quint64 timestamp : list) {
    if (!scrobbler_cache_.contains(timestamp)) continue;
    scrobbler_cache_.remove(timestamp);
  }
  DoInAMinuteOrSo(this, SLOT(WriteCache()));

}
