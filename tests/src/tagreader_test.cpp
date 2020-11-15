/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QFile>
#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

#include "core/song.h"

#include "tagreader.h"
#include "test_utils.h"

namespace {

class TagReaderTest : public ::testing::Test {
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

  static QString SHA256SUM(const QString &filename) {
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray buffer;
      QCryptographicHash hash(QCryptographicHash::Sha256);
      while (file.bytesAvailable() > 0) {
        quint64 bytes_read = qMin(file.bytesAvailable(), qint64(8192));
        buffer = file.read(bytes_read);
        if (buffer.isEmpty()) break;
        hash.addData(buffer, bytes_read);
      }
      file.close();
      return hash.result().toHex();
    }
    return QString();
  }

};

TEST_F(TagReaderTest, TestFLACAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.flac");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.flac");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  {  // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestWavPackAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.wv");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  {  // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.wv");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestOggFLACAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.oga");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.oga");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.ogg");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.ogg");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  {  // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestOggOpusAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.opus");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.opus");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.spx");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.spx");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  {  // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestAIFFAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.aif");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.aif");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    //EXPECT_EQ("strawberry album artist", song.albumartist());
    //EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    //EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    //EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    //EXPECT_EQ("new album artist", song.albumartist());
    //EXPECT_EQ("new composer", song.composer());
    //EXPECT_EQ("new performer", song.performer());
    //EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    //EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    //EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  {  // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    //EXPECT_EQ("strawberry album artist", song.albumartist());
    //EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    //EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    //EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  {  // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  {  // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestASFAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.asf");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.asf");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    //EXPECT_EQ("strawberry album artist", song.albumartist());
    //EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    //EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    //EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    //EXPECT_EQ("new album artist", song.albumartist());
    //EXPECT_EQ("new composer", song.composer());
    //EXPECT_EQ("new performer", song.performer());
    //EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    //EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    //EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    //EXPECT_EQ("strawberry album artist", song.albumartist());
    //EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    //EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    //EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestMP3AudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.mp3");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.mp3");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestM4AAudioFileTagging) {

  TemporaryResource r(":/audio/strawberry.m4a");

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(":/audio/strawberry.m4a");
      orig_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      temp_file.open(QIODevice::ReadOnly);
      EXPECT_TRUE(temp_file.isOpen());
      temp_file_data = temp_file.readAll();
      temp_file.close();
    }

    EXPECT_TRUE(!orig_file_data.isEmpty());
    EXPECT_TRUE(!temp_file_data.isEmpty());
    EXPECT_TRUE(orig_file_data == temp_file_data);

  }

  Song orig_song = ReadSongFromFile(r.fileName());

  { // Write tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());

  }

  { // Write new tags
    Song song;
    song.set_title("new title");
    song.set_artist("new artist");
    song.set_album("new album");
    song.set_albumartist("new album artist");
    song.set_composer("new composer");
    song.set_performer("new performer");
    song.set_grouping("new grouping");
    song.set_genre("new genre");
    song.set_comment("new comment");
    song.set_lyrics("new lyrics");
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("new title", song.title());
    EXPECT_EQ("new artist", song.artist());
    EXPECT_EQ("new album", song.album());
    EXPECT_EQ("new album artist", song.albumartist());
    EXPECT_EQ("new composer", song.composer());
    //EXPECT_EQ("new performer", song.performer());
    EXPECT_EQ("new grouping", song.grouping());
    EXPECT_EQ("new genre", song.genre());
    EXPECT_EQ("new comment", song.comment());
    EXPECT_EQ("new lyrics", song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
  }

  { // Write original tags
    Song song;
    song.set_title("strawberry title");
    song.set_artist("strawberry artist");
    song.set_album("strawberry album");
    song.set_albumartist("strawberry album artist");
    song.set_composer("strawberry composer");
    song.set_performer("strawberry performer");
    song.set_grouping("strawberry grouping");
    song.set_genre("strawberry genre");
    song.set_comment("strawberry comment");
    song.set_lyrics("strawberry lyrics");
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ("strawberry title", song.title());
    EXPECT_EQ("strawberry artist", song.artist());
    EXPECT_EQ("strawberry album", song.album());
    EXPECT_EQ("strawberry album artist", song.albumartist());
    EXPECT_EQ("strawberry composer", song.composer());
    //EXPECT_EQ("strawberry performer", song.performer());
    EXPECT_EQ("strawberry grouping", song.grouping());
    EXPECT_EQ("strawberry genre", song.genre());
    EXPECT_EQ("strawberry comment", song.comment());
    EXPECT_EQ("strawberry lyrics", song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

}  // namespace
