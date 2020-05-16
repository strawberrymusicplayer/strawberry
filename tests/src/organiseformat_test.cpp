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

#include "organise/organiseformat.h"
#include "core/timeconstants.h"
#include "core/song.h"
#include "core/logging.h"

#include <QUrl>

class OrganiseFormatTest : public ::testing::Test {
protected:
  OrganiseFormat format_;
  Song song_;
};


TEST_F(OrganiseFormatTest, BasicReplace) {

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

  EXPECT_EQ("album_albumartist_artist_123_comment_composer_performer_grouping_789_genre_987_654_32_title_321_2010", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, Extension) {

  song_.set_url(QUrl("file:///some/path/filename.flac"));

  format_.set_format("%extension");
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ("flac", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, ArtistInitial) {

  song_.set_artist("bob");

  format_.set_format("%artistinitial");
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ("B", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, AlbumArtistInitial) {

  song_.set_albumartist("bob");

  format_.set_format("%artistinitial");
  ASSERT_TRUE(format_.IsValid());
  EXPECT_EQ("B", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, InvalidTag) {

  format_.set_format("%invalid");
  EXPECT_FALSE(format_.IsValid());

}

TEST_F(OrganiseFormatTest, Blocks) {

  format_.set_format("Before{Inside%year}After");
  ASSERT_TRUE(format_.IsValid());

  song_.set_year(-1);
  EXPECT_EQ("BeforeAfter", format_.GetFilenameForSong(song_));

  song_.set_year(0);
  EXPECT_EQ("BeforeAfter", format_.GetFilenameForSong(song_));

  song_.set_year(123);
  EXPECT_EQ("BeforeInside123After", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, ReplaceSpaces) {

  song_.set_title("The Song Title");
  format_.set_format("The Format String %title");

  format_.set_replace_spaces(false);
  EXPECT_EQ("The Format String The Song Title", format_.GetFilenameForSong(song_));
  format_.set_replace_spaces(true);
  EXPECT_EQ("The_Format_String_The_Song_Title", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, ReplaceNonAscii) {

  song_.set_artist(QString::fromUtf8("Röyksopp"));
  format_.set_format("%artist");

  format_.set_remove_non_ascii(false);
  EXPECT_EQ(QString::fromUtf8("Röyksopp"), format_.GetFilenameForSong(song_));

  format_.set_remove_non_ascii(true);
  EXPECT_EQ("Royksopp", format_.GetFilenameForSong(song_));

  song_.set_artist(QString::fromUtf8("Владимир Высоцкий"));
  EXPECT_EQ("????????_????????", format_.GetFilenameForSong(song_));

}

TEST_F(OrganiseFormatTest, TrackNumberPadding) {

  format_.set_format("%track");

  song_.set_track(9);
  EXPECT_EQ("09", format_.GetFilenameForSong(song_));

  song_.set_track(99);
  EXPECT_EQ("99", format_.GetFilenameForSong(song_));

  song_.set_track(999);
  EXPECT_EQ("999", format_.GetFilenameForSong(song_));

  song_.set_track(0);
  EXPECT_EQ("", format_.GetFilenameForSong(song_));

}

#if 0
TEST_F(OrganiseFormatTest, ReplaceSlashes) {

  format_.set_format("%title");
  song_.set_title("foo/bar\\baz");
  qLog(Debug) << format_.GetFilenameForSong(song_);
  EXPECT_EQ("foo_bar_baz", format_.GetFilenameForSong(song_));

}
#endif
