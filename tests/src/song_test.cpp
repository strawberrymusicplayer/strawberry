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

#include "config.h"

#include <id3v2tag.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QFile>
#include <QByteArray>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextCodec>
#include <QRegExp>

#include "core/song.h"

#include "tagreader.h"
#include "test_utils.h"

namespace {

class SongTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    // Return something from uninteresting mock functions.
    testing::DefaultValue<TagLib::String>::Set("foobarbaz");
  }

  static Song ReadSongFromFile(const QString& filename) {
    TagReader tag_reader;
    Song song;
    ::pb::tagreader::SongMetadata pb_song;

    // We need to init protobuf object from a Song object, to have default values initialized correctly.
    song.ToProtobuf(&pb_song);
    tag_reader.ReadFile(filename, &pb_song);
    song.InitFromProtobuf(pb_song);
    return song;
  }

  static void WriteSongToFile(const Song& song, const QString& filename) {
    TagReader tag_reader;
    ::pb::tagreader::SongMetadata pb_song;
    song.ToProtobuf(&pb_song);
    tag_reader.SaveFile(filename, pb_song);
  }

};

TEST_F(SongTest, TestAudioFileTagging) {

  const QStringList files_to_test = QStringList() << ":/audio/strawberry.wav"
                                                  << ":/audio/strawberry.flac"
                                                  << ":/audio/strawberry.wv"
                                                  << ":/audio/strawberry.oga"
                                                  << ":/audio/strawberry.opus"
                                                  << ":/audio/strawberry.spx"
                                                  << ":/audio/strawberry.aif"
                                                  << ":/audio/strawberry.m4a"
                                                  << ":/audio/strawberry.mp4"
                                                  << ":/audio/strawberry.mp3"
                                                  << ":/audio/strawberry.asf";

  for (const QString& test_filename : files_to_test) {

    TemporaryResource r(test_filename);
    Song song = ReadSongFromFile(r.fileName());

    // Compare files
    QFile orig_file(test_filename);
    orig_file.open(QIODevice::ReadOnly);
    EXPECT_TRUE(orig_file.isOpen());
    QByteArray orig_file_data = orig_file.readAll();

    QFile temp_file(r.fileName());
    temp_file.open(QIODevice::ReadOnly);
    EXPECT_TRUE(temp_file.isOpen());
    QByteArray temp_file_data = temp_file.readAll();

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

    if (test_filename.contains(QRegExp(".*\\.wav$"))) continue;

    // Write tags
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());

    // Read tags
    Song new_song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", new_song.title());
    EXPECT_EQ("strawberry artist", new_song.artist());
    EXPECT_EQ("strawberry album", new_song.album());
    if (!test_filename.contains(QRegExp(".*\\.aif$")) && !test_filename.contains(QRegExp(".*\\.asf$"))) {
      EXPECT_EQ("strawberry album artist", new_song.albumartist());
      EXPECT_EQ("strawberry composer", new_song.composer());
      if (!test_filename.contains(QRegExp(".*\\.mp4$")) && !test_filename.contains(QRegExp(".*\\.m4a$"))) {
        EXPECT_EQ("strawberry performer", new_song.performer());
      }
      EXPECT_EQ("strawberry grouping", new_song.grouping());
      EXPECT_EQ(1234, new_song.disc());
    }
    EXPECT_EQ("strawberry genre", new_song.genre());
    if (!test_filename.contains(QRegExp(".*\\.asf$"))) {
      EXPECT_EQ("strawberry comment", new_song.comment());
    }
    EXPECT_EQ(12, new_song.track());
    EXPECT_EQ(2019, new_song.year());

  }

}

}  // namespace
