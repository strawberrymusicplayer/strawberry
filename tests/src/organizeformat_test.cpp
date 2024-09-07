/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <gtest/gtest.h>
#include "test_utils.h"

#include "organize/organizeformat.h"
#include "utilities/timeconstants.h"
#include "core/song.h"
#include "core/logging.h"

#include <QUrl>

using namespace Qt::StringLiterals;

// clazy:excludeall=returning-void-expression

class OrganizeFormatTest : public ::testing::Test {
protected:
  OrganizeFormat format_;
  Song song_;
};


TEST_F(OrganizeFormatTest, BasicReplace) {

  song_.set_title(QStringLiteral("title"));
  song_.set_album(QStringLiteral("album"));
  song_.set_artist(QStringLiteral("artist"));
  song_.set_albumartist(QStringLiteral("albumartist"));
  song_.set_track(321);
  song_.set_disc(789);
  song_.set_year(2010);
  song_.set_originalyear(1995);
  song_.set_genre(QStringLiteral("genre"));
  song_.set_composer(QStringLiteral("composer"));
  song_.set_performer(QStringLiteral("performer"));
  song_.set_grouping(QStringLiteral("grouping"));
  song_.set_comment(QStringLiteral("comment"));
  song_.set_length_nanosec(987 * kNsecPerSec);
  song_.set_samplerate(654);
  song_.set_bitdepth(32);
  song_.set_bitrate(123);

  format_.set_format(QStringLiteral("%album %albumartist %artist %bitrate %comment %composer %performer %grouping %disc %genre %length %samplerate %bitdepth %title %track %year"));

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ(QStringLiteral("album_albumartist_artist_123_comment_composer_performer_grouping_789_genre_987_654_32_title_321_2010"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, BasicReplacePaths) {

  song_.set_title(QStringLiteral("title"));
  song_.set_album(QStringLiteral("album"));
  song_.set_artist(QStringLiteral("artist"));
  song_.set_albumartist(QStringLiteral("albumartist"));
  song_.set_track(321);

  format_.set_format(QStringLiteral("%albumartist/%album/%track %albumartist %artist %album %title"));

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ(QStringLiteral("albumartist/album/321_albumartist_artist_album_title"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ArtistInitial) {

  song_.set_artist(QStringLiteral("bob"));

  format_.set_format(QStringLiteral("%artistinitial"));
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ(QStringLiteral("B"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, AlbumArtistInitial) {

  song_.set_albumartist(QStringLiteral("bob"));

  format_.set_format(QStringLiteral("%artistinitial"));
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ(QStringLiteral("B"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, InvalidTag) {

  format_.set_format(QStringLiteral("%invalid"));
  EXPECT_FALSE(format_.IsValid());

}

TEST_F(OrganizeFormatTest, Blocks) {

  format_.set_format(QStringLiteral("Before{Inside%year}After"));
  ASSERT_TRUE(format_.IsValid());

  song_.set_year(-1);
  EXPECT_EQ(QStringLiteral("BeforeAfter"), format_.GetFilenameForSong(song_).filename);

  song_.set_year(0);
  EXPECT_EQ(QStringLiteral("BeforeAfter"), format_.GetFilenameForSong(song_).filename);

  song_.set_year(123);
  EXPECT_EQ(QStringLiteral("BeforeInside123After"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSpaces) {

  song_.set_title(QStringLiteral("The Song Title"));
  format_.set_format(QStringLiteral("The Format String %title"));

  format_.set_replace_spaces(false);
  EXPECT_EQ(QStringLiteral("The Format String The Song Title"), format_.GetFilenameForSong(song_).filename);
  format_.set_replace_spaces(true);
  EXPECT_EQ(QStringLiteral("The_Format_String_The_Song_Title"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceNonAscii) {

  song_.set_artist(QStringLiteral("Röyksopp"));
  format_.set_format(QStringLiteral("%artist"));

  format_.set_remove_non_ascii(false);
  EXPECT_EQ(QStringLiteral("Röyksopp"), format_.GetFilenameForSong(song_).filename);

  format_.set_remove_non_ascii(true);
  EXPECT_EQ(QStringLiteral("Royksopp"), format_.GetFilenameForSong(song_).filename);

  song_.set_artist(""_L1);
  EXPECT_EQ(QLatin1String(""), format_.GetFilenameForSong(song_).filename);

  song_.set_artist(QStringLiteral("Владимир Высоцкий"));
  EXPECT_EQ(QStringLiteral("Vladimir_Vysockij"), format_.GetFilenameForSong(song_).filename);

  song_.set_artist(QStringLiteral("エックス・ジャパン"));
  EXPECT_EQ(QStringLiteral("ekkusujapan"), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, TrackNumberPadding) {

  format_.set_format(QStringLiteral("%track"));

  song_.set_track(9);
  EXPECT_EQ(QStringLiteral("09"), format_.GetFilenameForSong(song_).filename);

  song_.set_track(99);
  EXPECT_EQ(QStringLiteral("99"), format_.GetFilenameForSong(song_).filename);

  song_.set_track(999);
  EXPECT_EQ(QStringLiteral("999"), format_.GetFilenameForSong(song_).filename);

  song_.set_track(0);
  EXPECT_EQ(QLatin1String(""), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSlashes) {

  format_.set_format(QStringLiteral("%title"));
  song_.set_title(QStringLiteral("foo/bar\\baz"));
  EXPECT_EQ(QStringLiteral("foobarbaz"), format_.GetFilenameForSong(song_).filename);

}
