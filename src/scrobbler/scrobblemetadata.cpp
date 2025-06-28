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

#include "core/song.h"

#include "scrobblemetadata.h"

ScrobbleMetadata::ScrobbleMetadata(const Song &song)
    : title(song.title()),
      album(song.album()),
      artist(song.artist()),
      albumartist(song.albumartist()),
      track(song.track()),
      grouping(song.grouping()),
      musicbrainz_album_artist_id(song.musicbrainz_album_artist_id()),
      musicbrainz_artist_id(song.musicbrainz_artist_id()),
      musicbrainz_original_artist_id(song.musicbrainz_original_artist_id()),
      musicbrainz_album_id(song.musicbrainz_album_id()),
      musicbrainz_original_album_id(song.musicbrainz_original_album_id()),
      musicbrainz_recording_id(song.musicbrainz_recording_id()),
      musicbrainz_track_id(song.musicbrainz_track_id()),
      musicbrainz_disc_id(song.musicbrainz_disc_id()),
      musicbrainz_release_group_id(song.musicbrainz_release_group_id()),
      musicbrainz_work_id(song.musicbrainz_work_id()),
      music_service(song.is_stream() ? song.DomainForSource() : QString()),
      music_service_name(song.is_stream() ? song.DescriptionForSource() : QString()),
      share_url(song.ShareURL()),
      spotify_id(song.source() == Song::Source::Spotify ? song.song_id() : QString()),
      length_nanosec(song.length_nanosec()) {}
