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

#include "gtest_include.h"
#include "gmock_include.h"

#include <QFile>
#include <QByteArray>
#include <QString>
#include <QCryptographicHash>
#include <QThread>
#include <QEventLoop>

#include "core/logging.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"

#include "test_utils.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

class TagReaderTest : public ::testing::Test {
 protected:

  ~TagReaderTest() {
    tagreader_client_thread_->exit();
    tagreader_client_thread_->wait(5000);
    tagreader_client_->deleteLater();
    tagreader_client_thread_->deleteLater();
  }

  void SetUp() override {
    tagreader_client_ = new TagReaderClient();
    tagreader_client_thread_ = new QThread();
    tagreader_client_->moveToThread(tagreader_client_thread_);
    tagreader_client_thread_->start();
  }

  static void SetUpTestCase() {
    // Return something from uninteresting mock functions.
    testing::DefaultValue<TagLib::String>::Set("foobarbaz");
  }

  Song ReadSongFromFile(const QString &filename) const {

    TagReaderReadFileReplyPtr reply = tagreader_client_->ReadFileAsync(filename);

    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReadFileReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->song();

  }

  void WriteSongToFile(const Song &song, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(filename, song, SaveTagsOption::Tags, SaveTagCoverData());

    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

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

  void WriteSongPlaycountToFile(const Song &song, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->SaveSongPlaycountAsync(filename, song.playcount());
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

  }

  void WriteSongRatingToFile(const Song &song, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->SaveSongRatingAsync(filename, song.rating());
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

  }

  QImage ReadCoverFromFile(const QString &filename) const {

    TagReaderLoadCoverImageReplyPtr reply = tagreader_client_->LoadCoverImageAsync(filename);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderLoadCoverImageReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->image();

  }

  TagReaderResult WriteCoverToFile(const QString &filename, const SaveTagCoverData &save_tag_cover_data) const {

    TagReaderReplyPtr reply = tagreader_client_->SaveCoverAsync(filename, save_tag_cover_data);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->result();

  }

  QThread *tagreader_client_thread_ = nullptr;
  TagReaderClient *tagreader_client_ = nullptr;

};

TEST_F(TagReaderTest, TestFLACAudioFileTagging) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.flac"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_titlesort(u"strawberry title sort"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_artistsort(u"strawberry artist sort"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumsort(u"strawberry album sort"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_albumartistsort(u"strawberry album artist sort"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_composersort(u"strawberry composer sort"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_performersort(u"strawberry performer sort"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry title sort"_s, song.titlesort());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry artist sort"_s, song.artistsort());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album sort"_s, song.albumsort());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry composer sort"_s, song.composersort());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry performer sort"_s, song.performersort());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_titlesort(u"new title sort"_s);
    song.set_artist(u"new artist"_s);
    song.set_artistsort(u"new artist sort"_s);
    song.set_album(u"new album"_s);
    song.set_albumsort(u"new album sort"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_albumartistsort(u"new album artist sort"_s);
    song.set_composer(u"new composer"_s);
    song.set_composersort(u"new composer sort"_s);
    song.set_performer(u"new performer"_s);
    song.set_performersort(u"new performer sort"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new title sort"_s, song.titlesort());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new artist sort"_s, song.artistsort());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album sort"_s, song.albumsort());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new composer sort"_s, song.composersort());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new performer sort"_s, song.performersort());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_titlesort(u"strawberry title sort"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_artistsort(u"strawberry artist sort"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumsort(u"strawberry album sort"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_albumartistsort(u"strawberry album artist sort"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_composersort(u"strawberry composer sort"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_performersort(u"strawberry performer sort"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry title sort"_s, song.titlesort());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry artist sort"_s, song.artistsort());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album sort"_s, song.albumsort());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry composer sort"_s, song.composersort());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry performer sort"_s, song.performersort());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.wv"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  {  // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.wv"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.oga"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.oga"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.ogg"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.ogg"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.opus"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.opus"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.spx"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.spx"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.aif"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.aif"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    //EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    //EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    //EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    //EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    //EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    //EXPECT_EQ(u"new album artist"_s, song.albumartist());
    //EXPECT_EQ(u"new composer"_s, song.composer());
    //EXPECT_EQ(u"new performer"_s, song.performer());
    //EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    //EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    //EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  {  // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  {  // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    //EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    //EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    //EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    //EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.asf"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    //EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    EXPECT_EQ(2019, song.originalyear());

  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    //EXPECT_EQ(u"new performer"_s, song.performer());
    //EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    //EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.mp3"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_titlesort(u"strawberry title sort"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_artistsort(u"strawberry artist sort"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumsort(u"strawberry album sort"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_albumartistsort(u"strawberry album artist sort"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_composersort(u"strawberry composer sort"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry title sort"_s, song.titlesort());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry artist sort"_s, song.artistsort());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album sort"_s, song.albumsort());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry composer sort"_s, song.composersort());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_titlesort(u"new title sort"_s);
    song.set_artist(u"new artist"_s);
    song.set_artistsort(u"new artist sort"_s);
    song.set_album(u"new album"_s);
    song.set_albumsort(u"new album sort"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_albumartistsort(u"new album artist sort"_s);
    song.set_composer(u"new composer"_s);
    song.set_composersort(u"new composer sort"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new title sort"_s, song.titlesort());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new artist sort"_s, song.artistsort());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album sort"_s, song.albumsort());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"new composer"_s, song.composer());
    EXPECT_EQ(u"new composer sort"_s, song.composersort());
    EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_titlesort(u"strawberry title sort"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_artistsort(u"strawberry artist sort"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumsort(u"strawberry album sort"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_albumartistsort(u"strawberry album artist sort"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_composersort(u"strawberry composer sort"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry title sort"_s, song.titlesort());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry artist sort"_s, song.artistsort());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album sort"_s, song.albumsort());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry album artist sort"_s, song.albumartistsort());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(u"strawberry composer sort"_s, song.composersort());
    EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

  QString sha256sum_notags = SHA256SUM(r.fileName());
  EXPECT_FALSE(sha256sum_notags.isEmpty());

  { // Compare files
    QByteArray orig_file_data;
    QByteArray temp_file_data;
    {
      QFile orig_file(u":/audio/strawberry.m4a"_s);
      EXPECT_TRUE(orig_file.open(QIODevice::ReadOnly));
      EXPECT_TRUE(orig_file.isOpen());
      orig_file_data = orig_file.readAll();
      orig_file.close();
    }

    {
      QFile temp_file(r.fileName());
      EXPECT_TRUE(temp_file.open(QIODevice::ReadOnly));
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
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
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
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    //EXPECT_EQ(2019, song.originalyear());
  }

  { // Write new tags
    Song song;
    song.set_title(u"new title"_s);
    song.set_artist(u"new artist"_s);
    song.set_album(u"new album"_s);
    song.set_albumartist(u"new album artist"_s);
    song.set_composer(u"new composer"_s);
    song.set_performer(u"new performer"_s);
    song.set_grouping(u"new grouping"_s);
    song.set_genre(u"new genre"_s);
    song.set_comment(u"new comment"_s);
    song.set_lyrics(u"new lyrics"_s);
    song.set_track(21);
    song.set_disc(4321);
    song.set_year(9102);
    song.set_originalyear(9102);
    WriteSongToFile(song, r.fileName());
  }

  { // Read new tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"new title"_s, song.title());
    EXPECT_EQ(u"new artist"_s, song.artist());
    EXPECT_EQ(u"new album"_s, song.album());
    EXPECT_EQ(u"new album artist"_s, song.albumartist());
    EXPECT_EQ(u"new composer"_s, song.composer());
    //EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    //EXPECT_EQ(9102, song.originalyear());
  }

  { // Write original tags
    Song song;
    song.set_title(u"strawberry title"_s);
    song.set_artist(u"strawberry artist"_s);
    song.set_album(u"strawberry album"_s);
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    song.set_performer(u"strawberry performer"_s);
    song.set_grouping(u"strawberry grouping"_s);
    song.set_genre(u"strawberry genre"_s);
    song.set_comment(u"strawberry comment"_s);
    song.set_lyrics(u"strawberry lyrics"_s);
    song.set_track(12);
    song.set_disc(1234);
    song.set_year(2019);
    song.set_originalyear(2019);
    WriteSongToFile(song, r.fileName());
  }

  { // Read original tags
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry title"_s, song.title());
    EXPECT_EQ(u"strawberry artist"_s, song.artist());
    EXPECT_EQ(u"strawberry album"_s, song.album());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    //EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
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

  TemporaryResource r(u":/audio/strawberry.flac"_s);

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

  TemporaryResource r(u":/audio/strawberry.wv"_s);

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

  TemporaryResource r(u":/audio/strawberry.oga"_s);

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

  TemporaryResource r(u":/audio/strawberry.ogg"_s);

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

  TemporaryResource r(u":/audio/strawberry.opus"_s);

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

  TemporaryResource r(u":/audio/strawberry.spx"_s);

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

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

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

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

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

TEST_F(TagReaderTest, TestFLACAudioFilePlaycount) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

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

  TemporaryResource r(u":/audio/strawberry.wv"_s);

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

  TemporaryResource r(u":/audio/strawberry.oga"_s);

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

  TemporaryResource r(u":/audio/strawberry.ogg"_s);

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

  TemporaryResource r(u":/audio/strawberry.opus"_s);

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

  TemporaryResource r(u":/audio/strawberry.spx"_s);

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

  TemporaryResource r(u":/audio/strawberry.asf"_s);

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

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

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

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

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

TEST_F(TagReaderTest, TestFLACAudioFileRating) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

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

  TemporaryResource r(u":/audio/strawberry.wv"_s);

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

  TemporaryResource r(u":/audio/strawberry.oga"_s);

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

  TemporaryResource r(u":/audio/strawberry.ogg"_s);

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

  TemporaryResource r(u":/audio/strawberry.opus"_s);

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

  TemporaryResource r(u":/audio/strawberry.spx"_s);

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

  TemporaryResource r(u":/audio/strawberry.asf"_s);

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

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

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

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

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

TEST_F(TagReaderTest, TestFLACAudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestOggOpusAudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.opus"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.spx"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestMP3AudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestMP4AudioFileCover) {

  TemporaryResource r(u":/audio/strawberry.mp4"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

}  // namespace
