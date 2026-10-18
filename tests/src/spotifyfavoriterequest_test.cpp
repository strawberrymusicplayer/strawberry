/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "gtest_include.h"

#include <QList>
#include <QString>

#include "core/song.h"
#include "spotify/spotifyfavoriterequest.h"

namespace {

Song SpotifySong(const int song_number, const int album_number) {

  Song song(Song::Source::Spotify);
  song.set_song_id(QStringLiteral("song%1").arg(song_number));
  song.set_album_id(QStringLiteral("album%1").arg(album_number));
  song.set_artist_id(QStringLiteral("artist%1").arg(album_number));
  return song;

}

// Regression test: every ID was sent in one request, the API only accepts 50 songs or artists and 20 albums in one request.
TEST(SpotifyFavoriteRequestTest, SongsAreSplitInBatchesOf50) {

  SongList songs;
  for (int i = 0; i < 120; ++i) {
    songs << SpotifySong(i, i);
  }

  const QList<SpotifyFavoriteRequest::FavoriteBatch> batches = SpotifyFavoriteRequest::SplitFavorites(SpotifyFavoriteRequest::FavoriteType_Songs, songs);
  ASSERT_EQ(3, batches.count());
  EXPECT_EQ(50, batches[0].ids.count());
  EXPECT_EQ(50, batches[1].ids.count());
  EXPECT_EQ(20, batches[2].ids.count());
  EXPECT_EQ(50, batches[0].songs.count());
  EXPECT_EQ(QStringLiteral("song50"), batches[1].ids.first());
  EXPECT_EQ(QStringLiteral("song50"), batches[1].songs.first().song_id());

}

TEST(SpotifyFavoriteRequestTest, AlbumsAreSplitInBatchesOf20WithTheirSongs) {

  // 41 albums, the first album has two songs, which are both in the first batch.
  SongList songs;
  songs << SpotifySong(0, 0);
  for (int i = 0; i < 41; ++i) {
    songs << SpotifySong(i + 1, i);
  }
  Song song_without_album(Song::Source::Spotify);
  song_without_album.set_song_id(QStringLiteral("song_without_album"));
  songs << song_without_album;

  const QList<SpotifyFavoriteRequest::FavoriteBatch> batches = SpotifyFavoriteRequest::SplitFavorites(SpotifyFavoriteRequest::FavoriteType_Albums, songs);
  ASSERT_EQ(3, batches.count());
  EXPECT_EQ(20, batches[0].ids.count());
  EXPECT_EQ(20, batches[1].ids.count());
  EXPECT_EQ(1, batches[2].ids.count());
  EXPECT_EQ(21, batches[0].songs.count());
  EXPECT_EQ(20, batches[1].songs.count());
  EXPECT_EQ(1, batches[2].songs.count());
  EXPECT_EQ(QStringLiteral("album40"), batches[2].ids.first());

}

TEST(SpotifyFavoriteRequestTest, NoIdsGiveNoBatches) {

  EXPECT_TRUE(SpotifyFavoriteRequest::SplitFavorites(SpotifyFavoriteRequest::FavoriteType_Artists, SongList() << Song(Song::Source::Spotify)).isEmpty());

}

}  // namespace
