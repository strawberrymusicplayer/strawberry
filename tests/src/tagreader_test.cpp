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

#if defined(HAVE_TAGLIB)
#  include "tagreadertaglib.h"
#elif defined(HAVE_TAGPARSER)
#  include "tagreadertagparser.h"
#endif

#include "test_utils.h"

// clazy:excludeall=non-pod-global-static

namespace {

class TagReaderTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    // Return something from uninteresting mock functions.
#if defined(HAVE_TAGLIB)
    testing::DefaultValue<TagLib::String>::Set("foobarbaz");
#endif
  }

  static Song ReadSongFromFile(const QString& filename) {
#if defined(HAVE_TAGLIB)
    TagReaderTagLib tag_reader;
#elif defined(HAVE_TAGPARSER)
    TagReaderTagParser tag_reader;
#endif
    Song song;
    ::spb::tagreader::SongMetadata pb_song;

    // We need to init protobuf object from a Song object, to have default values initialized correctly.
    song.ToProtobuf(&pb_song);
    tag_reader.ReadFile(filename, &pb_song);
    song.InitFromProtobuf(pb_song);
    return song;
  }

  static void WriteSongToFile(const Song &song, const QString &filename) {
#if defined(HAVE_TAGLIB)
    TagReaderTagLib tag_reader;
#elif defined(HAVE_TAGPARSER)
    TagReaderTagParser tag_reader;
#endif
    ::spb::tagreader::WriteFileRequest request;
    request.set_filename(filename.toStdString());
    request.set_save_tags(true);
    song.ToProtobuf(request.mutable_metadata());
    tag_reader.WriteFile(filename, request);
  }

  static QString SHA256SUM(const QString &filename) {
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly)) {
      QByteArray buffer;
      QCryptographicHash hash(QCryptographicHash::Sha256);
      while (file.bytesAvailable() > 0) {
        qint64 bytes_read = qMin(file.bytesAvailable(), 8192LL);
        buffer = file.read(bytes_read);
        if (buffer.isEmpty()) break;
        hash.addData(buffer);
      }
      file.close();
      return QString::fromLatin1(hash.result().toHex());
    }
    return QString();
  }

  static void WriteSongPlaycountToFile(const Song &song, const QString &filename) {
#if defined(HAVE_TAGLIB)
    TagReaderTagLib tag_reader;
#elif defined(HAVE_TAGPARSER)
    TagReaderTagParser tag_reader;
#endif
    spb::tagreader::SongMetadata pb_song;
    song.ToProtobuf(&pb_song);
    tag_reader.SaveSongPlaycountToFile(filename, pb_song.playcount());
  }

  static void WriteSongRatingToFile(const Song &song, const QString &filename) {
#if defined(HAVE_TAGLIB)
    TagReaderTagLib tag_reader;
#elif defined(HAVE_TAGPARSER)
    TagReaderTagParser tag_reader;
#endif
    spb::tagreader::SongMetadata pb_song;
    song.ToProtobuf(&pb_song);
    tag_reader.SaveSongRatingToFile(filename, pb_song.rating());
  }

};

TEST_F(TagReaderTest, TestFLACAudioFileTagging) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.flac"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.flac"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.wv"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  {  // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.wv"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.oga"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.oga"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.ogg"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.ogg"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.opus"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.opus"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.spx"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.spx"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.aif"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.aif"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    //EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    //EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    //EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    //EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    //EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    //EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    //EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  {  // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    //EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    //EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    //EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.asf"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.asf"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    EXPECT_EQ(2019, song.originalyear());

  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    //EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    EXPECT_EQ(2019, song.originalyear());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestMP3AudioFileTagging) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.mp3"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.mp3"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
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

  TemporaryResource r(QStringLiteral(":/audio/strawberry.m4a"));

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(QStringLiteral(":/audio/strawberry.m4a"));
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
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  // Read new checksum
  QString sha256sum_tags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_tags.isEmpty());

  { // Read tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(QStringLiteral("new title"));
    song.set_artist(QStringLiteral("new artist"));
    song.set_album(QStringLiteral("new album"));
    song.set_albumartist(QStringLiteral("new album artist"));
    song.set_composer(QStringLiteral("new composer"));
    song.set_performer(QStringLiteral("new performer"));
    song.set_grouping(QStringLiteral("new grouping"));
    song.set_genre(QStringLiteral("new genre"));
    song.set_comment(QStringLiteral("new comment"));
    song.set_lyrics(QStringLiteral("new lyrics"));
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("new title"), song.title());
    EXPECT_EQ(QStringLiteral("new artist"), song.artist());
    EXPECT_EQ(QStringLiteral("new album"), song.album());
    EXPECT_EQ(QStringLiteral("new album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("new composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("new performer"), song.performer());
    EXPECT_EQ(QStringLiteral("new grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("new genre"), song.genre());
    EXPECT_EQ(QStringLiteral("new comment"), song.comment());
    EXPECT_EQ(QStringLiteral("new lyrics"), song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(QStringLiteral("strawberry title"));
    song.set_artist(QStringLiteral("strawberry artist"));
    song.set_album(QStringLiteral("strawberry album"));
    song.set_albumartist(QStringLiteral("strawberry album artist"));
    song.set_composer(QStringLiteral("strawberry composer"));
    song.set_performer(QStringLiteral("strawberry performer"));
    song.set_grouping(QStringLiteral("strawberry grouping"));
    song.set_genre(QStringLiteral("strawberry genre"));
    song.set_comment(QStringLiteral("strawberry comment"));
    song.set_lyrics(QStringLiteral("strawberry lyrics"));
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(QStringLiteral("strawberry title"), song.title());
    EXPECT_EQ(QStringLiteral("strawberry artist"), song.artist());
    EXPECT_EQ(QStringLiteral("strawberry album"), song.album());
    EXPECT_EQ(QStringLiteral("strawberry album artist"), song.albumartist());
    EXPECT_EQ(QStringLiteral("strawberry composer"), song.composer());
    //EXPECT_EQ(QStringLiteral("strawberry performer"), song.performer());
    EXPECT_EQ(QStringLiteral("strawberry grouping"), song.grouping());
    EXPECT_EQ(QStringLiteral("strawberry genre"), song.genre());
    EXPECT_EQ(QStringLiteral("strawberry comment"), song.comment());
    EXPECT_EQ(QStringLiteral("strawberry lyrics"), song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    //EXPECT_EQ(sha256sum, sha256sum_notags);
  }

}

TEST_F(TagReaderTest, TestFLACAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.flac"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestWavPackAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.wv"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestOggFLACAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.oga"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.ogg"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestOggOpusAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.opus"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.spx"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestMP3AudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.mp3"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

TEST_F(TagReaderTest, TestMP4AudioFileCompilation) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.m4a"));

  {
    Song song;
    song.set_compilation(true);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(true, song.compilation());
  }

  {
    Song song;
    song.set_compilation(false);
    WriteSongToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(false, song.compilation());
  }

}

#ifndef HAVE_TAGPARSER

TEST_F(TagReaderTest, TestFLACAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.flac"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestWavPackAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.wv"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestOggFLACAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.oga"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestOggVorbisAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.ogg"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestOggOpusAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.opus"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestOggSpeexAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.spx"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestOggASFAudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.asf"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestMP3AudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.mp3"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

TEST_F(TagReaderTest, TestMP4AudioFilePlaycount) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.m4a"));

  {
    Song song;
    song.set_playcount(4);
    WriteSongPlaycountToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(4, song.playcount());
  }

}

#endif  // HAVE_TAGPARSER

TEST_F(TagReaderTest, TestFLACAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.flac"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestWavPackAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.wv"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestOggFLACAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.oga"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.ogg"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestOggOpusAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.opus"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.spx"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestASFAudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.asf"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestMP3AudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.mp3"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestMP4AudioFileRating) {

  TemporaryResource r(QStringLiteral(":/audio/strawberry.m4a"));

  {
    Song song;
    song.set_rating(0.4);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

}  // namespace
