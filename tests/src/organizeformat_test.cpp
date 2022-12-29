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

// clazy:excludeall=returning-void-expression

class OrganizeFormatTest : public ::testing::Test {
protected:
  OrganizeFormat format_;
  Song song_;
};


TEST_F(OrganizeFormatTest, BasicReplace) {

  song_.set_title("title");
  song_.set_album("album");
  song_.set_artist("artist");
  song_.set_albumartist("albumartist");
  song_.set_track(321);
  song_.set_disc(789);
  song_.set_year(2010);
  song_.set_originalyear(1995);
  song_.set_genre("genre");
  song_.set_composer("composer");
  song_.set_performer("performer");
  song_.set_grouping("grouping");
  song_.set_comment("comment");
  song_.set_length_nanosec(987 * kNsecPerSec);
  song_.set_samplerate(654);
  song_.set_bitdepth(32);
  song_.set_bitrate(123);

  format_.set_format("%album %albumartist %artist %bitrate %comment %composer %performer %grouping %disc %genre %length %samplerate %bitdepth %title %track %year");

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ("album_albumartist_artist_123_comment_composer_performer_grouping_789_genre_987_654_32_title_321_2010", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, BasicReplacePaths) {

  song_.set_title("title");
  song_.set_album("album");
  song_.set_artist("artist");
  song_.set_albumartist("albumartist");
  song_.set_track(321);

  format_.set_format("%albumartist/%album/%track %albumartist %artist %album %title");

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ("albumartist/album/321_albumartist_artist_album_title", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ArtistInitial) {

  song_.set_artist("bob");

  format_.set_format("%artistinitial");
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ("B", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, AlbumArtistInitial) {

  song_.set_albumartist("bob");

  format_.set_format("%artistinitial");
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ("B", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, InvalidTag) {

  format_.set_format("%invalid");
  EXPECT_FALSE(format_.IsValid());

}

TEST_F(OrganizeFormatTest, Blocks) {

  format_.set_format("Before{Inside%year}After");
  ASSERT_TRUE(format_.IsValid());

  song_.set_year(-1);
  EXPECT_EQ("BeforeAfter", format_.GetFilenameForSong(song_).filename);

  song_.set_year(0);
  EXPECT_EQ("BeforeAfter", format_.GetFilenameForSong(song_).filename);

  song_.set_year(123);
  EXPECT_EQ("BeforeInside123After", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSpaces) {

  song_.set_title("The Song Title");
  format_.set_format("The Format String %title");

  format_.set_replace_spaces(false);
  EXPECT_EQ("The Format String The Song Title", format_.GetFilenameForSong(song_).filename);
  format_.set_replace_spaces(true);
  EXPECT_EQ("The_Format_String_The_Song_Title", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceNonAscii) {

  song_.set_artist(QString::fromUtf8("Röyksopp"));
  format_.set_format("%artist");

  format_.set_remove_non_ascii(false);
  EXPECT_EQ(QString::fromUtf8("Röyksopp"), format_.GetFilenameForSong(song_).filename);

  format_.set_remove_non_ascii(true);
  EXPECT_EQ("Royksopp", format_.GetFilenameForSong(song_).filename);

  song_.set_artist("");
  EXPECT_EQ("", format_.GetFilenameForSong(song_).filename);

#ifdef HAVE_ICU

  song_.set_artist(QString::fromUtf8("Владимир Высоцкий"));
  EXPECT_EQ("Vladimir_Vysockij", format_.GetFilenameForSong(song_).filename);

  song_.set_artist(QString::fromUtf8("エックス・ジャパン"));
  EXPECT_EQ("ekkusujapan", format_.GetFilenameForSong(song_).filename);

#endif

}

TEST_F(OrganizeFormatTest, TrackNumberPadding) {

  format_.set_format("%track");

  song_.set_track(9);
  EXPECT_EQ("09", format_.GetFilenameForSong(song_).filename);

  song_.set_track(99);
  EXPECT_EQ("99", format_.GetFilenameForSong(song_).filename);

  song_.set_track(999);
  EXPECT_EQ("999", format_.GetFilenameForSong(song_).filename);

  song_.set_track(0);
  EXPECT_EQ("", format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSlashes) {

  format_.set_format("%title");
  song_.set_title("foo/bar\\baz");
  EXPECT_EQ("foobarbaz", format_.GetFilenameForSong(song_).filename);

}
