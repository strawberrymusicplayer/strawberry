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

#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/song.h"
#include "organize/organize.h"
#include "organize/organizeformat.h"
#include "organize/organizedialog.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

TEST(OrganizeTest, UniqueFilenames) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"Artist/Song.mp3"_s, unique_filenames.Make(u"Artist/Song.mp3"_s));
  EXPECT_EQ(u"Artist/Song(2).mp3"_s, unique_filenames.Make(u"Artist/Song.mp3"_s));
  EXPECT_EQ(u"Artist/Song(3).mp3"_s, unique_filenames.Make(u"Artist/Song.mp3"_s));

}

// Filenames which only differ in case are the same file on case-insensitive filesystems.
TEST(OrganizeTest, UniqueFilenamesIgnoresCase) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"Artist/Song.mp3"_s, unique_filenames.Make(u"Artist/Song.mp3"_s));
  EXPECT_EQ(u"artist/song(2).mp3"_s, unique_filenames.Make(u"artist/song.mp3"_s));
  EXPECT_EQ(u"ARTIST/SONG(3).MP3"_s, unique_filenames.Make(u"ARTIST/SONG.MP3"_s));

}

// Composed and decomposed characters are the same file on filesystems which normalize them.
TEST(OrganizeTest, UniqueFilenamesIgnoresUnicodeNormalization) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"Caf\u00e9.mp3"_s, unique_filenames.Make(u"Caf\u00e9.mp3"_s));
  EXPECT_EQ(u"Cafe\u0301(2).mp3"_s, unique_filenames.Make(u"Cafe\u0301.mp3"_s));

}

// A number added to a filename can be the filename of another song, which is not used again.
TEST(OrganizeTest, UniqueFilenamesSkipsUsedNumbers) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"Song(2).mp3"_s, unique_filenames.Make(u"Song(2).mp3"_s));
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s));
  EXPECT_EQ(u"Song(3).mp3"_s, unique_filenames.Make(u"Song.mp3"_s));

}

TEST(OrganizeTest, UniqueFilenamesWithoutExtension) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"Song"_s, unique_filenames.Make(u"Song"_s));
  EXPECT_EQ(u"Song(2)"_s, unique_filenames.Make(u"Song"_s));

}

// Many songs with the same filename continue with the next number instead of trying all numbers again.
TEST(OrganizeTest, UniqueFilenamesManyDuplicates) {

  Organize::UniqueFilenames unique_filenames;
  EXPECT_EQ(u"01.mp3"_s, unique_filenames.Make(u"01.mp3"_s));
  for (int i = 2; i <= 2000; ++i) {
    ASSERT_EQ(u"01(%1).mp3"_s.arg(i), unique_filenames.Make(u"01.mp3"_s));
  }

}

// When organizing in place, the current files of the songs which are not organized yet are not given to other songs, which would overwrite them.
TEST(OrganizeTest, UniqueFilenamesKeepsFilesOfOtherSongs) {

  const QStringList filenames = QStringList() << u"Song.mp3"_s << u"song.mp3"_s << u"song(2).mp3"_s;
  SongList songs;
  for (const QString &filename : filenames) {
    Song song;
    song.set_url(QUrl::fromLocalFile(u"/music/"_s + filename));
    songs << song;
  }

  Organize::UniqueFilenames unique_filenames(u"/music"_s, songs);
  // Each song can keep its own file.
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song.mp3"_s));
  // The second song gets a number, but not the one of the file of the third song.
  EXPECT_EQ(u"song(3).mp3"_s, unique_filenames.Make(u"song.mp3"_s, u"/music/song.mp3"_s));
  EXPECT_EQ(u"song(2).mp3"_s, unique_filenames.Make(u"song(2).mp3"_s, u"/music/song(2).mp3"_s));

}

// The current file of a song stays reserved until it's moved, also after the song got its filename, for example while it's being transcoded.
TEST(OrganizeTest, UniqueFilenamesKeepsFilesUntilReleased) {

  SongList songs;
  for (const QString &filename : QStringList{u"/music/song(2).ogg"_s, u"/other/a.ogg"_s, u"/other/b.ogg"_s}) {
    Song song;
    song.set_url(QUrl::fromLocalFile(filename));
    songs << song;
  }

  Organize::UniqueFilenames unique_filenames(u"/music"_s, songs);
  EXPECT_EQ(u"Other.ogg"_s, unique_filenames.Make(u"Other.ogg"_s, u"/music/song(2).ogg"_s));
  EXPECT_EQ(u"song.ogg"_s, unique_filenames.Make(u"song.ogg"_s, u"/other/a.ogg"_s));
  // The first song is not moved yet, so its file is not used.
  EXPECT_EQ(u"song(3).ogg"_s, unique_filenames.Make(u"song.ogg"_s, u"/other/b.ogg"_s));

  // After it's moved, its filename can be used.
  unique_filenames.Release(u"/music/song(2).ogg"_s);
  EXPECT_EQ(u"song(2).ogg"_s, unique_filenames.Make(u"song(2).ogg"_s));

}

// A song can only keep its own file with the exact same path, a path which only matches ignoring case is the file of another song on case-sensitive filesystems.
TEST(OrganizeTest, UniqueFilenamesOwnFileIsCaseSensitive) {

  SongList songs;
  for (const QString &filename : QStringList{u"/music/Song.mp3"_s, u"/music/song.mp3"_s}) {
    Song song;
    song.set_url(QUrl::fromLocalFile(filename));
    songs << song;
  }

  Organize::UniqueFilenames unique_filenames(u"/music"_s, songs);
  // The first song asks for the file of the second song.
  EXPECT_EQ(u"song(2).mp3"_s, unique_filenames.Make(u"song.mp3"_s, u"/music/Song.mp3"_s));
  // The second song keeps its own file.
  EXPECT_EQ(u"song.mp3"_s, unique_filenames.Make(u"song.mp3"_s, u"/music/song.mp3"_s));

}

// Only changing the case of the filename of a song is allowed when no other song has a file with the same filename ignoring case.
TEST(OrganizeTest, UniqueFilenamesCaseOnlyRename) {

  Song song;
  song.set_url(QUrl::fromLocalFile(u"/music/song.mp3"_s));

  Organize::UniqueFilenames unique_filenames(u"/music"_s, SongList() << song);
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/song.mp3"_s));

}

// Songs which were organized before with numbered filenames keep them, instead of getting another number every time they are organized.
TEST(OrganizeTest, UniqueFilenamesKeepsOwnNumberedFile) {

  SongList songs;
  for (const QString &filename : QStringList{u"/music/Song.mp3"_s, u"/music/Song(2).mp3"_s}) {
    Song song;
    song.set_url(QUrl::fromLocalFile(filename));
    songs << song;
  }

  Organize::UniqueFilenames unique_filenames(u"/music"_s, songs);
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song.mp3"_s));
  EXPECT_EQ(u"Song(2).mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song(2).mp3"_s));

}

// A song keeps its own numbered file also when a song with a higher number is organized first.
TEST(OrganizeTest, UniqueFilenamesKeepsOwnNumberedFileInAnyOrder) {

  SongList songs;
  for (const QString &filename : QStringList{u"/music/Song.mp3"_s, u"/music/Song(3).mp3"_s, u"/music/Song(2).mp3"_s}) {
    Song song;
    song.set_url(QUrl::fromLocalFile(filename));
    songs << song;
  }

  Organize::UniqueFilenames unique_filenames(u"/music"_s, songs);
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song.mp3"_s));
  EXPECT_EQ(u"Song(3).mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song(3).mp3"_s));
  EXPECT_EQ(u"Song(2).mp3"_s, unique_filenames.Make(u"Song.mp3"_s, u"/music/Song(2).mp3"_s));
  // A new song with the same filename gets the next free number.
  EXPECT_EQ(u"Song(4).mp3"_s, unique_filenames.Make(u"Song.mp3"_s));

}

// Files outside of the destination are not reserved.
TEST(OrganizeTest, UniqueFilenamesIgnoresFilesOutsideRoot) {

  Song song;
  song.set_url(QUrl::fromLocalFile(u"/other/Song.mp3"_s));

  Organize::UniqueFilenames unique_filenames(u"/music"_s, SongList() << song);
  EXPECT_EQ(u"Song.mp3"_s, unique_filenames.Make(u"Song.mp3"_s));

}

// Songs with titles that only differ in case get different filenames.
TEST(OrganizeTest, ComputeNewSongsFilenamesIgnoresCase) {

  OrganizeFormat format;
  format.set_format(u"%title.%extension"_s);

  const QStringList titles = QStringList() << u"Song"_s << u"song"_s;
  SongList songs;
  for (const QString &title : titles) {
    Song song;
    song.Init(title, u"Artist"_s, u"Album"_s, 123);
    song.set_url(QUrl(u"file:///tmp/"_s + title + u".mp3"_s));
    song.set_basefilename(title + u".mp3"_s);
    songs << song;
  }

  const Organize::NewSongInfoList new_songs_info = OrganizeDialog::ComputeNewSongsFilenames(songs, format, u"mp3"_s);
  ASSERT_EQ(2, new_songs_info.count());
  EXPECT_EQ(u"Song.mp3"_s, new_songs_info[0].new_filename_);
  EXPECT_EQ(u"song(2).mp3"_s, new_songs_info[1].new_filename_);

}

}  // namespace
