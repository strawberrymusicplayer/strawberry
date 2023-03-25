/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QString>

#include "scrobblercacheitem.h"

ScrobblerCacheItem::ScrobblerCacheItem(const QString &artist,
                                       const QString &album,
                                       const QString &song,
                                       const QString &albumartist,
                                       const int track,
                                       const qint64 duration,
                                       const quint64 timestamp,
                                       const QString &musicbrainz_album_id,
                                       const QString &musicbrainz_artist_id,
                                       const QString &musicbrainz_recording_id,
                                       const QString &musicbrainz_release_group_id,
                                       const QString &musicbrainz_track_id,
                                       const QString &musicbrainz_work_id,
                                       QObject *parent)
    : QObject(parent),
      artist_(artist),
      album_(album),
      song_(song),
      albumartist_(albumartist),
      track_(track),
      duration_(duration),
      timestamp_(timestamp),
      sent_(false),
      musicbrainz_album_id_(musicbrainz_album_id),
      musicbrainz_artist_id_(musicbrainz_artist_id),
      musicbrainz_recording_id_(musicbrainz_recording_id),
      musicbrainz_release_group_id_(musicbrainz_release_group_id),
      musicbrainz_track_id_(musicbrainz_track_id),
      musicbrainz_work_id_(musicbrainz_work_id) {}
