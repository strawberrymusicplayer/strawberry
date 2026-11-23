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

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QIODevice>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/memorydatabase.h"
#include "core/song.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmanager.h"
#include "constants/playlistsettings.h"
#include "playlistparsers/asxparser.h"
#include "playlistparsers/m3uparser.h"
#include "playlistparsers/parserbase.h"
#include "playlistparsers/plsparser.h"
#include "playlistparsers/wplparser.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

const QUrl kMediaUrl(u"file:///music/album.flac"_s);

Song MakeCueSong(const int track) {
  Song song(Song::Source::Collection);
  song.Init(u"Title %1"_s.arg(track), u"Artist"_s, u"Album"_s, (track - 1) * 100'000'000'000LL, track * 100'000'000'000LL);
  song.set_directory_id(1);
  song.set_url(kMediaUrl);
  song.set_cue_path(u"/music/album.cue"_s);
  song.set_track(track);
  song.set_mtime(1);
  song.set_ctime(1);
  song.set_filesize(1);
  return song;
}

class PlaylistParserCueTest : public ::testing::Test {
 protected:
  void SetUp() override {
    database_ = make_shared<MemoryDatabase>(nullptr);
    backend_ = make_shared<CollectionBackend>();
    backend_->Init(database_, nullptr, Song::Source::Collection, QLatin1String(CollectionManager::kSongsTable), QLatin1String(CollectionManager::kDirsTable), QLatin1String(CollectionManager::kSubdirsTable));
    backend_->AddDirectory(u"/music"_s);
    backend_->AddOrUpdateSongs(SongList() << MakeCueSong(1) << MakeCueSong(2) << MakeCueSong(3));
  }

  // Saves the songs, and loads them again, finding them in the collection.
  static SongList SaveAndLoad(const ParserBase &parser, const SongList &songs) {
    QByteArray data;
    {
      QBuffer buffer(&data);
      if (!buffer.open(QIODevice::WriteOnly)) return SongList();
      parser.Save(u"Playlist"_s, songs, &buffer, QDir(u"/music"_s), PlaylistSettings::PathType::Absolute);
    }
    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::ReadOnly)) return SongList();
    return parser.Load(&buffer, QString(), QDir(u"/music"_s), true).songs;
  }

  static void ExpectTracksKept(const ParserBase &parser) {
    const SongList songs = SaveAndLoad(parser, SongList() << MakeCueSong(3) << MakeCueSong(1) << MakeCueSong(2));
    ASSERT_EQ(3, songs.count());
    EXPECT_EQ(3, songs[0].track());
    EXPECT_EQ(1, songs[1].track());
    EXPECT_EQ(2, songs[2].track());
    EXPECT_EQ(200'000'000'000LL, songs[0].beginning_nanosec());
  }

  SharedPtr<Database> database_;
  SharedPtr<CollectionBackend> backend_;
};

// Regression tests: the songs of a CUE sheet share the URL, so they were all loaded as the first track.
TEST_F(PlaylistParserCueTest, M3UKeepsTracks) {
  ExpectTracksKept(M3UParser(nullptr, backend_));
}

TEST_F(PlaylistParserCueTest, PLSKeepsTracks) {
  ExpectTracksKept(PLSParser(nullptr, backend_));
}

TEST_F(PlaylistParserCueTest, WPLKeepsTracks) {
  ExpectTracksKept(WplParser(nullptr, backend_));
}

TEST_F(PlaylistParserCueTest, ASXKeepsTracks) {
  ExpectTracksKept(ASXParser(nullptr, backend_));
}

// Regression test: the keys of a PLS entry can be in any order, the track after the file was ignored.
TEST_F(PlaylistParserCueTest, PLSTrackAfterFile) {

  PLSParser parser(nullptr, backend_);
  QByteArray data = "[playlist]\nFile1=/music/album.flac\nTrack1=2\nTitle1=Other title\nLength1=7\n";
  QBuffer buffer(&data);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  const SongList songs = parser.Load(&buffer, QString(), QDir(u"/music"_s), true).songs;

  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(2, songs[0].track());
  EXPECT_EQ(MakeCueSong(2).beginning_nanosec(), songs[0].beginning_nanosec());
  EXPECT_EQ(u"Other title"_s, songs[0].title());
  EXPECT_EQ(7 * 1'000'000'000LL, songs[0].length_nanosec());

}

// The tag must not be read as the length of the song.
TEST_F(PlaylistParserCueTest, M3UTrackTagIsNotMetadata) {

  M3UParser parser(nullptr, backend_);
  QByteArray data = "#EXTM3U\n#EXTSTRAWBERRYTRACK:2\n/music/album.flac\n";
  QBuffer buffer(&data);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  const SongList songs = parser.Load(&buffer, QString(), QDir(u"/music"_s), true).songs;

  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(2, songs[0].track());
  EXPECT_EQ(MakeCueSong(2).length_nanosec(), songs[0].length_nanosec());

}

// The metadata in the playlist doesn't replace the metadata of collection songs, like with the other playlist formats.
TEST_F(PlaylistParserCueTest, M3UMetadataDoesNotReplaceCollectionMetadata) {

  M3UParser parser(nullptr, backend_);
  QByteArray data = "#EXTM3U\n#EXTINF:5,Other Artist - Other Title\n#EXTSTRAWBERRYTRACK:2\n/music/album.flac\n";
  QBuffer buffer(&data);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  const SongList songs = parser.Load(&buffer, QString(), QDir(u"/music"_s), true).songs;

  ASSERT_EQ(1, songs.count());
  EXPECT_EQ(Song::Source::Collection, songs[0].source());
  EXPECT_EQ(u"Artist"_s, songs[0].artist());
  EXPECT_EQ(u"Title 2"_s, songs[0].title());
  EXPECT_EQ(MakeCueSong(2).length_nanosec(), songs[0].length_nanosec());

}

// Other songs don't get anything added to the playlist.
TEST_F(PlaylistParserCueTest, OtherSongsHaveNoTrack) {

  Song song = MakeCueSong(2);
  song.set_cue_path(QString());

  M3UParser parser(nullptr, backend_);
  QByteArray data;
  QBuffer buffer(&data);
  ASSERT_TRUE(buffer.open(QIODevice::WriteOnly));
  parser.Save(u"Playlist"_s, SongList() << song, &buffer, QDir(u"/music"_s), PlaylistSettings::PathType::Absolute);

  EXPECT_FALSE(data.contains("#EXTSTRAWBERRYTRACK"));

}

}  // namespace
