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

#include "gtest_include.h"

#include "test_utils.h"

#include "constants/timeconstants.h"
#include "core/logging.h"
#include "core/song.h"
#include "organize/organizeformat.h"

#include <QUrl>

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=returning-void-expression

class OrganizeFormatTest : public ::testing::Test {
protected:
  OrganizeFormat format_;
  Song song_;
};


TEST_F(OrganizeFormatTest, BasicReplace) {

  song_.set_title(u"title"_s);
  song_.set_album(u"album"_s);
  song_.set_artist(u"artist"_s);
  song_.set_albumartist(u"albumartist"_s);
  song_.set_track(321);
  song_.set_disc(789);
  song_.set_year(2010);
  song_.set_originalyear(1995);
  song_.set_genre(u"genre"_s);
  song_.set_composer(u"composer"_s);
  song_.set_performer(u"performer"_s);
  song_.set_grouping(u"grouping"_s);
  song_.set_comment(u"comment"_s);
  song_.set_length_nanosec(987 * kNsecPerSec);
  song_.set_samplerate(654);
  song_.set_bitdepth(32);
  song_.set_bitrate(123);

  format_.set_format(u"%album %albumartist %artist %bitrate %comment %composer %performer %grouping %disc %genre %length %samplerate %bitdepth %title %track %year"_s);

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ(u"album_albumartist_artist_123_comment_composer_performer_grouping_789_genre_987_654_32_title_321_2010"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, BasicReplacePaths) {

  song_.set_title(u"title"_s);
  song_.set_album(u"album"_s);
  song_.set_artist(u"artist"_s);
  song_.set_albumartist(u"albumartist"_s);
  song_.set_track(321);

  format_.set_format(u"%albumartist/%album/%track %albumartist %artist %album %title"_s);

  ASSERT_TRUE(format_.IsValid());

  EXPECT_EQ(u"albumartist/album/321_albumartist_artist_album_title"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ArtistInitial) {

  song_.set_artist(u"bob"_s);

  format_.set_format(u"%artistinitial"_s);
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ(u"B"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, AlbumArtistInitial) {

  song_.set_albumartist(u"bob"_s);

  format_.set_format(u"%artistinitial"_s);
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ(u"B"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, InvalidTag) {

  format_.set_format(u"%invalid"_s);
  EXPECT_FALSE(format_.IsValid());

}

TEST_F(OrganizeFormatTest, Blocks) {

  format_.set_format(u"Before{Inside%year}After"_s);
  ASSERT_TRUE(format_.IsValid());

  song_.set_year(-1);
  EXPECT_EQ(u"BeforeAfter"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_year(0);
  EXPECT_EQ(u"BeforeAfter"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_year(123);
  EXPECT_EQ(u"BeforeInside123After"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSpaces) {

  song_.set_title(u"The Song Title"_s);
  format_.set_format(u"The Format String %title"_s);

  format_.set_replace_spaces(false);
  EXPECT_EQ(u"The Format String The Song Title"_s, format_.GetFilenameForSong(song_).filename);
  format_.set_replace_spaces(true);
  EXPECT_EQ(u"The_Format_String_The_Song_Title"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceNonAscii) {

  song_.set_artist(u"Röyksopp"_s);
  format_.set_format(u"%artist"_s);

  format_.set_remove_non_ascii(false);
  EXPECT_EQ(u"Röyksopp"_s, format_.GetFilenameForSong(song_).filename);

  format_.set_remove_non_ascii(true);
  EXPECT_EQ(u"Royksopp"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_artist(""_L1);
  EXPECT_EQ(QLatin1String(""), format_.GetFilenameForSong(song_).filename);

  song_.set_artist(u"Владимир Высоцкий"_s);
  EXPECT_EQ(u"Vladimir_Vysockij"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_artist(u"エックス・ジャパン"_s);
  EXPECT_EQ(u"ekkusujapan"_s, format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, TrackNumberPadding) {

  format_.set_format(u"%track"_s);

  song_.set_track(9);
  EXPECT_EQ(u"09"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_track(99);
  EXPECT_EQ(u"99"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_track(999);
  EXPECT_EQ(u"999"_s, format_.GetFilenameForSong(song_).filename);

  song_.set_track(0);
  EXPECT_EQ(QLatin1String(""), format_.GetFilenameForSong(song_).filename);

}

TEST_F(OrganizeFormatTest, ReplaceSlashes) {

  format_.set_format(u"%title"_s);
  song_.set_title(u"foo/bar\\baz"_s);
  EXPECT_EQ(u"foobarbaz"_s, format_.GetFilenameForSong(song_).filename);

}
