/*
* Strawberry Music Player
* Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCROBBLEMETADATA_H
#define SCROBBLEMETADATA_H

#include <QtGlobal>
#include <QString>

#include "core/song.h"

class ScrobbleMetadata {
 public:
  explicit ScrobbleMetadata(const Song &song = Song());

  QString title;
  QString album;
  QString artist;
  QString albumartist;
  int track;
  QString grouping;
  QString musicbrainz_album_artist_id;     // artist_mbids
  QString musicbrainz_artist_id;           // artist_mbids
  QString musicbrainz_original_artist_id;  // artist_mbids
  QString musicbrainz_album_id;            // release_mbid
  QString musicbrainz_original_album_id;   // release_mbid
  QString musicbrainz_recording_id;        // recording_mbid
  QString musicbrainz_track_id;            // track_mbid
  QString musicbrainz_disc_id;
  QString musicbrainz_release_group_id;    // release_group_mbid
  QString musicbrainz_work_id;             // work_mbids
  QString music_service;
  QString music_service_name;
  QString share_url;
  QString spotify_id;
  qint64 length_nanosec;

  QString effective_albumartist() const { return albumartist.isEmpty() ? artist : albumartist; }

};

#endif  // SCROBBLEMETADATA_H
