/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <string>
#include <functional>

#include <QFile>
#include <QImage>
#include <QDir>
#include <QDataStream>
#include <QColor>
#include <QBuffer>
#include <QPair>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QThread>
#include <QEventLoop>
#include <QSemaphore>

#include <taglib/tstring.h>
#include <taglib/asffile.h>
#include <taglib/asftag.h>
#include <taglib/asfattribute.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4item.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2header.h>
#include <taglib/popularimeterframe.h>
#include <taglib/textidentificationframe.h>
#include <taglib/flacfile.h>
#include <taglib/xiphcomment.h>
#include <taglib/wavpackfile.h>
#include <taglib/apetag.h>
#include <taglib/apeitem.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacpicture.h>
#include <taglib/vorbisfile.h>
#ifdef HAVE_TAGLIB_DSFFILE
#  include <taglib/dsffile.h>
#endif
#ifdef HAVE_TAGLIB_DSDIFFFILE
#  include <taglib/dsdifffile.h>
#endif

#include "core/logging.h"
#include "core/song.h"
#include "tagreader/tagreaderclient.h"
#include "tagreader/tagreadertaglib.h"

#include "test_utils.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// For opening files with TagLib directly, to create or check tags the tag reader doesn't write itself.
#ifdef Q_OS_WIN32
std::wstring TagLibFileName(const QString &filename) { return filename.toStdWString(); }
#else
QByteArray TagLibFileName(const QString &filename) { return QFile::encodeName(filename); }
#endif

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

  Song ReadSongFromFile(const QString &filename, const Song &song = Song()) const {

    TagReaderReadFileReplyPtr reply = tagreader_client_->ReadFileAsync(filename, song);

    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReadFileReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->song();

  }

  TagReaderResult WriteSongToFile(const Song &song, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(filename, song, SaveTagsOption::Tags, SaveTagCoverData());

    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->result();

  }

  TagReaderResult WriteSyncedLyricsToFile(const QString &synced_lyrics, const QString &filename) const {

    Song song;
    song.set_synced_lyrics(synced_lyrics);
    song.set_synced_lyrics_storage_type(Song::StorageType::Tag);
    return WriteSongToFile(song, filename);

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

  TagReaderResult SaveLyricsToFile(const QString &lyrics, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->SaveSongLyricsAsync(filename, lyrics);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->result();

  }

  TagReaderResult SaveSyncedLyricsToFile(const QString &synced_lyrics, const QString &filename) const {

    TagReaderReplyPtr reply = tagreader_client_->SaveSongSyncedLyricsAsync(filename, synced_lyrics);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();

    return reply->result();

  }

  // Saves, overwrites and clears the lyrics, and checks that the other tags are not changed.
  void TestSaveLyrics(const QString &filename) const {

    {
      Song song;
      song.set_title(u"strawberry title"_s);
      song.set_artist(u"strawberry artist"_s);
      EXPECT_TRUE(WriteSongToFile(song, filename).success());
    }

    EXPECT_TRUE(SaveLyricsToFile(u"strawberry lyrics"_s, filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
      EXPECT_EQ(Song::StorageType::Tag, song.lyrics_storage_type());
      EXPECT_EQ(u"strawberry title"_s, song.title());
      EXPECT_EQ(u"strawberry artist"_s, song.artist());
    }

    EXPECT_TRUE(SaveLyricsToFile(u"new lyrics"_s, filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_EQ(u"new lyrics"_s, song.lyrics());
      EXPECT_EQ(u"strawberry title"_s, song.title());
      EXPECT_EQ(u"strawberry artist"_s, song.artist());
    }

    EXPECT_TRUE(SaveLyricsToFile(QString(), filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_TRUE(song.lyrics().isEmpty());
      EXPECT_EQ(u"strawberry title"_s, song.title());
    }

  }

  // Saves, overwrites and clears the synchronized lyrics, and checks that the plain lyrics are not changed.
  void TestSaveSyncedLyrics(const QString &filename) const {

    EXPECT_TRUE(SaveLyricsToFile(u"strawberry lyrics"_s, filename).success());

    EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:01.00]Line one\n[00:02.50]Line two"_s, filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_EQ(u"[00:01.00]Line one\n[00:02.50]Line two"_s, song.synced_lyrics());
      EXPECT_EQ(Song::StorageType::Tag, song.synced_lyrics_storage_type());
      EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    }

    EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:03.00]New line"_s, filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_EQ(u"[00:03.00]New line"_s, song.synced_lyrics());
      EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    }

    EXPECT_TRUE(SaveSyncedLyricsToFile(QString(), filename).success());
    {
      const Song song = ReadSongFromFile(filename);
      EXPECT_TRUE(song.synced_lyrics().isEmpty());
      EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    }

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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    // EXPECT_EQ(u"strawberry composer"_s, song.composer());
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    // EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    // EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    // EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"new album artist"_s, song.albumartist());
    // EXPECT_EQ(u"new composer"_s, song.composer());
    // EXPECT_EQ(u"new performer"_s, song.performer());
    // EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    // EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    // EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    // EXPECT_EQ(u"strawberry composer"_s, song.composer());
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    // EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    // EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    // EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    // EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"new performer"_s, song.performer());
    // EXPECT_EQ(u"new grouping"_s, song.grouping());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    // EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
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
    // EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(2019, song.originalyear());
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
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    // EXPECT_EQ(2019, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"new performer"_s, song.performer());
    EXPECT_EQ(u"new grouping"_s, song.grouping());
    EXPECT_EQ(u"new genre"_s, song.genre());
    EXPECT_EQ(u"new comment"_s, song.comment());
    EXPECT_EQ(u"new lyrics"_s, song.lyrics());
    EXPECT_EQ(21, song.track());
    EXPECT_EQ(4321, song.disc());
    EXPECT_EQ(9102, song.year());
    // EXPECT_EQ(9102, song.originalyear());
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
    song.set_lyrics_storage_type(Song::StorageType::Tag);
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
    // EXPECT_EQ(u"strawberry performer"_s, song.performer());
    EXPECT_EQ(u"strawberry grouping"_s, song.grouping());
    EXPECT_EQ(u"strawberry genre"_s, song.genre());
    EXPECT_EQ(u"strawberry comment"_s, song.comment());
    EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
    EXPECT_EQ(12, song.track());
    EXPECT_EQ(1234, song.disc());
    EXPECT_EQ(2019, song.year());
    // EXPECT_EQ(2019, song.originalyear());
  }

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    // EXPECT_EQ(sha256sum_tags, sha256sum);
  }

  WriteSongToFile(orig_song, r.fileName());

  { // Compare checksums
    QString sha256sum = SHA256SUM(r.fileName());
    EXPECT_FALSE(sha256sum.isEmpty());
    // EXPECT_EQ(sha256sum, sha256sum_notags);
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

TEST_F(TagReaderTest, TestMP4AudioFileReadBPM) {

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

  // The BPM is an integer item.
  {
    TagLib::MP4::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->setItem("tmpo", TagLib::MP4::Item(128));
    ASSERT_TRUE(file.save());
  }
  EXPECT_EQ(128.0F, ReadSongFromFile(r.fileName()).bpm());

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

TEST_F(TagReaderTest, TestMP3AudioFileSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  { // Multiple timestamps per line, fraction widths and sorting
    EXPECT_TRUE(WriteSyncedLyricsToFile(u"[ar:strawberry artist]\n[00:01.00]Line one\n[00:02.50][00:10.25]Chorus\n\n[00:03.123]Precise\n[00:04.5]Tenths"_s, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"[00:01.00]Line one\n[00:02.50]Chorus\n[00:03.123]Precise\n[00:04.50]Tenths\n[00:10.25]Chorus"_s, song.synced_lyrics());
  }

  { // Offset is applied to the timestamps, lines shifted before the start are clamped to zero
    EXPECT_TRUE(WriteSyncedLyricsToFile(u"[offset:+500]\n[00:01.00]Line one\n[00:00.20]Line zero"_s, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"[00:00.00]Line zero\n[00:00.50]Line one"_s, song.synced_lyrics());
  }

  { // Negative offset
    EXPECT_TRUE(WriteSyncedLyricsToFile(u"[offset:-1500]\n[01:00.00]Line one"_s, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"[01:01.50]Line one"_s, song.synced_lyrics());
  }

  { // Invalid LRC fails the write and leaves the existing lyrics alone
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"[00:01.00]Line one\nLine without timestamp"_s, r.fileName()).success());
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"Just plain lyrics"_s, r.fileName()).success());
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"[ar:strawberry artist]"_s, r.fileName()).success());
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"[00:75.00]Invalid seconds"_s, r.fileName()).success());
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"[99999999:00.00]Too large"_s, r.fileName()).success());
    EXPECT_FALSE(WriteSyncedLyricsToFile(u"[offset:abc]\n[00:01.00]Invalid offset"_s, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"[01:01.50]Line one"_s, song.synced_lyrics());
  }

  { // Clear
    EXPECT_TRUE(WriteSyncedLyricsToFile(QString(), r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_TRUE(song.synced_lyrics().isEmpty());
  }

}

TEST_F(TagReaderTest, TestFLACAudioFileSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  { // Stored as is
    const QString synced_lyrics = u"[ar:strawberry artist]\n[00:01.00]Line one\n[00:02.50][00:10.25]Chorus"_s;
    EXPECT_TRUE(WriteSyncedLyricsToFile(synced_lyrics, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(synced_lyrics, song.synced_lyrics());
  }

  { // Clear
    EXPECT_TRUE(WriteSyncedLyricsToFile(QString(), r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_TRUE(song.synced_lyrics().isEmpty());
  }

}

namespace {

void SetFLACVorbisCommentField(const QString &filename, const TagLib::String &key, const TagLib::String &value) {

  TagLib::FLAC::File file(TagLibFileName(filename).data());
  ASSERT_TRUE(file.isValid());
  file.xiphComment(true)->addField(key, value, true);
  ASSERT_TRUE(file.save());

}

TagLib::Ogg::FieldListMap FLACVorbisCommentFields(const QString &filename) {

  TagLib::FLAC::File file(TagLibFileName(filename).data());
  return file.isValid() && file.xiphComment() ? file.xiphComment()->fieldListMap() : TagLib::Ogg::FieldListMap();

}

}  // namespace

TEST_F(TagReaderTest, TestFLACAudioFileLyricsFieldWithSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[ar:strawberry artist]\n[00:01.00]Line one\n[00:02.50]Line two");

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[ar:strawberry artist]\n[00:01.00]Line one\n[00:02.50]Line two"_s, song.synced_lyrics());
  EXPECT_EQ(Song::StorageType::Tag, song.synced_lyrics_storage_type());
  EXPECT_TRUE(song.lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestFLACAudioFileLyricsFieldWithPlainLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  // Section headers in brackets are not LRC tags or timestamps.
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[Chorus]\nLine one\nLine two");

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[Chorus]\nLine one\nLine two"_s, song.lyrics());
  EXPECT_EQ(Song::StorageType::Tag, song.lyrics_storage_type());
  EXPECT_TRUE(song.synced_lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestFLACAudioFilePlainLyricsFieldWithSyncedLyricsField) {

  // Older versions saved plain lyrics in LYRICS and synchronized lyrics in SYNCEDLYRICS.
  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "strawberry lyrics");
  SetFLACVorbisCommentField(r.fileName(), "SYNCEDLYRICS", "[00:01.00]Line one");

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());

}

TEST_F(TagReaderTest, TestFLACAudioFileSyncedLyricsFieldTakesPrecedenceOverSyncedLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:02.00]New line");
  SetFLACVorbisCommentField(r.fileName(), "SYNCEDLYRICS", "[00:01.00]Old line");

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[00:02.00]New line"_s, song.synced_lyrics());

}

TEST_F(TagReaderTest, TestFLACAudioFileUnsyncedLyricsFieldTakesPrecedenceOverPlainLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "old lyrics");
  SetFLACVorbisCommentField(r.fileName(), "UNSYNCEDLYRICS", "new lyrics");

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"new lyrics"_s, song.lyrics());
  EXPECT_TRUE(song.synced_lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestFLACAudioFileSavingSyncedLyricsKeepsPlainLyricsFromLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "strawberry lyrics");

  EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());
  EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());

  const TagLib::Ogg::FieldListMap fields = FLACVorbisCommentFields(r.fileName());
  EXPECT_EQ(TagLib::StringList("[00:01.00]Line one"), fields["LYRICS"]);
  EXPECT_EQ(TagLib::StringList("strawberry lyrics"), fields["UNSYNCEDLYRICS"]);

}

TEST_F(TagReaderTest, TestFLACAudioFileSavingSyncedLyricsKeepsUnsyncedLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "old lyrics");
  SetFLACVorbisCommentField(r.fileName(), "UNSYNCEDLYRICS", "new lyrics");

  EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());
  EXPECT_EQ(u"new lyrics"_s, song.lyrics());

}

TEST_F(TagReaderTest, TestFLACAudioFileClearingSyncedLyricsRemovesSyncedLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "SYNCEDLYRICS", "[00:01.00]Line one");

  EXPECT_TRUE(SaveSyncedLyricsToFile(QString(), r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_TRUE(song.synced_lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestFLACAudioFileClearingLyricsRemovesPlainLyricsFromLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "strawberry lyrics");

  EXPECT_TRUE(SaveLyricsToFile(QString(), r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_TRUE(song.lyrics().isEmpty());
  EXPECT_TRUE(song.synced_lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestFLACAudioFileSavingLyricsKeepsSyncedLyricsInLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");

  EXPECT_TRUE(SaveLyricsToFile(u"strawberry lyrics"_s, r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"strawberry lyrics"_s, song.lyrics());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());

}

TEST_F(TagReaderTest, TestFLACAudioFileRereadingLyricsFieldChangedToSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "strawberry lyrics");
  const Song song = ReadSongFromFile(r.fileName());
  ASSERT_EQ(u"strawberry lyrics"_s, song.lyrics());

  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");
  const Song song_reread = ReadSongFromFile(r.fileName(), song);
  EXPECT_EQ(u"[00:01.00]Line one"_s, song_reread.synced_lyrics());
  EXPECT_EQ(Song::StorageType::Tag, song_reread.synced_lyrics_storage_type());
  EXPECT_TRUE(song_reread.lyrics().isEmpty());
  EXPECT_EQ(Song::StorageType::None, song_reread.lyrics_storage_type());

}

TEST_F(TagReaderTest, TestFLACAudioFileRereadingLyricsFieldChangedToPlainLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");
  const Song song = ReadSongFromFile(r.fileName());
  ASSERT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());

  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "strawberry lyrics");
  const Song song_reread = ReadSongFromFile(r.fileName(), song);
  EXPECT_EQ(u"strawberry lyrics"_s, song_reread.lyrics());
  EXPECT_TRUE(song_reread.synced_lyrics().isEmpty());
  EXPECT_EQ(Song::StorageType::None, song_reread.synced_lyrics_storage_type());

}

TEST_F(TagReaderTest, TestFLACAudioFileRereadingKeepsLyricsStoredInDatabase) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");

  Song song;
  song.set_lyrics(u"database lyrics"_s);
  song.set_lyrics_storage_type(Song::StorageType::Database);
  song.set_synced_lyrics(u"[00:02.00]Database line"_s);
  song.set_synced_lyrics_storage_type(Song::StorageType::Database);
  const Song song_reread = ReadSongFromFile(r.fileName(), song);
  EXPECT_EQ(u"database lyrics"_s, song_reread.lyrics());
  EXPECT_EQ(Song::StorageType::Database, song_reread.lyrics_storage_type());
  EXPECT_EQ(u"[00:02.00]Database line"_s, song_reread.synced_lyrics());
  EXPECT_EQ(Song::StorageType::Database, song_reread.synced_lyrics_storage_type());

}

TEST_F(TagReaderTest, TestFLACAudioFileInvalidSyncedLyricsAreNotSaved) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");
  const QString sha256sum = SHA256SUM(r.fileName());

  EXPECT_FALSE(WriteSyncedLyricsToFile(u"[00:01.00]Line one\nLine without timestamp"_s, r.fileName()).success());
  EXPECT_EQ(sha256sum, SHA256SUM(r.fileName()));

  EXPECT_FALSE(SaveSyncedLyricsToFile(u"Just plain lyrics"_s, r.fileName()).success());
  EXPECT_EQ(sha256sum, SHA256SUM(r.fileName()));

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileInvalidSyncedLyricsAreNotSaved) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());
  const QString sha256sum = SHA256SUM(r.fileName());

  EXPECT_FALSE(WriteSyncedLyricsToFile(u"Just plain lyrics"_s, r.fileName()).success());
  EXPECT_EQ(sha256sum, SHA256SUM(r.fileName()));

  EXPECT_FALSE(SaveSyncedLyricsToFile(u"Just plain lyrics"_s, r.fileName()).success());
  EXPECT_EQ(sha256sum, SHA256SUM(r.fileName()));

}

TEST_F(TagReaderTest, TestFLACAudioFileSyncedLyricsWithOnlyWhitespaceClearLyricsField) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  SetFLACVorbisCommentField(r.fileName(), "LYRICS", "[00:01.00]Line one");

  EXPECT_TRUE(SaveSyncedLyricsToFile(u" \n"_s, r.fileName()).success());

  EXPECT_FALSE(FLACVorbisCommentFields(r.fileName()).contains("LYRICS"));
  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_TRUE(song.synced_lyrics().isEmpty());

}

TEST_F(TagReaderTest, TestWavPackAudioFileSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.wv"_s);

  { // Stored as is
    const QString synced_lyrics = u"[00:01.00]Line one\n[00:02.50]Line two"_s;
    EXPECT_TRUE(WriteSyncedLyricsToFile(synced_lyrics, r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(synced_lyrics, song.synced_lyrics());
  }

  { // Clear
    EXPECT_TRUE(WriteSyncedLyricsToFile(QString(), r.fileName()).success());
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_TRUE(song.synced_lyrics().isEmpty());
  }

}

TEST_F(TagReaderTest, TestFLACAudioFileRating) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  {
    Song song;
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
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
    song.set_rating(0.4F);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestASFAudioFilePlaycountAndRatingReplaced) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  {
    Song song;
    song.set_playcount(4);
    song.set_rating(0.4F);
    WriteSongPlaycountToFile(song, r.fileName());
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song;
    song.set_playcount(5);
    song.set_rating(0.8F);
    WriteSongPlaycountToFile(song, r.fileName());
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(5, song.playcount());
    EXPECT_EQ(0.8F, song.rating());
  }

}

TEST_F(TagReaderTest, TestASFAudioFileWritePlaycountAndRating) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  {
    Song song;
    song.set_playcount(3);
    song.set_rating(0.6F);
    SaveTagsOptions save_tags_options;
    save_tags_options.setFlag(SaveTagsOption::Playcount);
    save_tags_options.setFlag(SaveTagsOption::Rating);
    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(r.fileName(), song, save_tags_options, SaveTagCoverData());
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_TRUE(reply->result().success());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(3, song.playcount());
    EXPECT_EQ(0.6F, song.rating());
  }

}

TEST_F(TagReaderTest, TestASFAudioFileWritePlaycountAndRatingKeepsTags) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  {
    Song song;
    song.set_albumartist(u"strawberry album artist"_s);
    song.set_composer(u"strawberry composer"_s);
    EXPECT_TRUE(WriteSongToFile(song, r.fileName()).success());
  }

  // Saving only the play count and rating should not touch the other tags.
  {
    Song song;
    song.set_playcount(3);
    song.set_rating(0.6F);
    SaveTagsOptions save_tags_options;
    save_tags_options.setFlag(SaveTagsOption::Playcount);
    save_tags_options.setFlag(SaveTagsOption::Rating);
    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(r.fileName(), song, save_tags_options, SaveTagCoverData());
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_TRUE(reply->result().success());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(u"strawberry album artist"_s, song.albumartist());
    EXPECT_EQ(u"strawberry composer"_s, song.composer());
    EXPECT_EQ(3, song.playcount());
    EXPECT_EQ(0.6F, song.rating());
  }

}

TEST_F(TagReaderTest, TestASFAudioFilePlaycountAndRatingRemoved) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  {
    Song song;
    song.set_playcount(4);
    song.set_rating(0.4F);
    WriteSongPlaycountToFile(song, r.fileName());
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song;
    song.set_playcount(0);
    song.set_rating(0.0F);
    WriteSongPlaycountToFile(song, r.fileName());
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0, song.playcount());
    EXPECT_LE(song.rating(), 0.0F);
  }

  {
    TagLib::ASF::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_FALSE(file.tag()->contains("FMPS/Playcount"));
    EXPECT_FALSE(file.tag()->contains("FMPS/Rating"));
  }

}

TEST_F(TagReaderTest, TestASFAudioFileReadDiscWithNumberOfDiscs) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  // The disc can include the number of discs.
  {
    TagLib::ASF::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->setAttribute("WM/PartOfSet", TagLib::ASF::Attribute(TagLib::String("2/3")));
    ASSERT_TRUE(file.save());
  }
  EXPECT_EQ(2, ReadSongFromFile(r.fileName()).disc());

  // Some taggers write it as a number.
  {
    TagLib::ASF::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->setAttribute("WM/PartOfSet", TagLib::ASF::Attribute(static_cast<unsigned int>(4)));
    ASSERT_TRUE(file.save());
  }
  EXPECT_EQ(4, ReadSongFromFile(r.fileName()).disc());

  // Written by Strawberry.
  Song song;
  song.set_disc(5);
  EXPECT_TRUE(WriteSongToFile(song, r.fileName()).success());
  EXPECT_EQ(5, ReadSongFromFile(r.fileName()).disc());

}

TEST_F(TagReaderTest, TestASFAudioFileReadLegacyPlaycountAndRating) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);

  // Older versions added a new value on every save, so the file has several values, and TagLib does not keep their order.
  {
    TagLib::ASF::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->addAttribute("FMPS/Playcount", TagLib::ASF::Attribute(TagLib::String("4")));
    file.tag()->addAttribute("FMPS/Playcount", TagLib::ASF::Attribute(TagLib::String("5")));
    file.tag()->addAttribute("FMPS/Rating", TagLib::ASF::Attribute(TagLib::String("0.4")));
    file.tag()->addAttribute("FMPS/Rating", TagLib::ASF::Attribute(TagLib::String("0.8")));
    ASSERT_TRUE(file.save());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    // The highest play count is the most recent, but for the rating it can't be known, so it can be either of them.
    EXPECT_EQ(5, song.playcount());
    EXPECT_TRUE(song.rating() == 0.4F || song.rating() == 0.8F);
  }

  // Saving again replaces all the old values.
  {
    Song song;
    song.set_playcount(6);
    song.set_rating(0.6F);
    WriteSongPlaycountToFile(song, r.fileName());
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    TagLib::ASF::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1U, file.tag()->attribute("FMPS/Playcount").size());
    EXPECT_EQ(1U, file.tag()->attribute("FMPS/Rating").size());
  }

  {
    const Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(6, song.playcount());
    EXPECT_EQ(0.6F, song.rating());
  }

}

TEST_F(TagReaderTest, TestMP3AudioFileRating) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  {
    Song song;
    song.set_rating(0.4F);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

// Ratings computed with floats can be slightly lower, like 0.59999996 for 3 stars, which should still be written as 3 stars.
TEST_F(TagReaderTest, TestMP3AudioFilePOPMRatingRounding) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  const auto popm_rating = [&r]() {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    if (!file.isValid() || !file.hasID3v2Tag()) return -1;
    const TagLib::ID3v2::FrameList frames = file.ID3v2Tag()->frameListMap()["POPM"];
    if (frames.isEmpty()) return -1;
    const TagLib::ID3v2::PopularimeterFrame *frame = dynamic_cast<TagLib::ID3v2::PopularimeterFrame*>(frames.front());
    return frame ? frame->rating() : -1;
  };

  const QList<QPair<float, int>> ratings = QList<QPair<float, int>>()
    << qMakePair(1.0F / 5.0F, 0x01)
    << qMakePair(2.0F / 5.0F, 0x40)
    << qMakePair(0.59999996F, 0x80)
    << qMakePair(3.0F / 5.0F, 0x80)
    << qMakePair(0.79999995F, 0xC0)
    << qMakePair(0.99999994F, 0xFF)
    << qMakePair(0.5F, 0x40);

  for (const QPair<float, int> &rating : ratings) {
    Song song;
    song.set_rating(rating.first);
    WriteSongRatingToFile(song, r.fileName());
    EXPECT_EQ(rating.second, popm_rating()) << "Rating " << rating.first;
  }

}

TEST_F(TagReaderTest, TestMP4AudioFileRating) {

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

  {
    Song song;
    song.set_rating(0.4F);
    WriteSongRatingToFile(song, r.fileName());
  }

  {
    Song song = ReadSongFromFile(r.fileName());
    EXPECT_EQ(0.4F, song.rating());
  }

}

TEST_F(TagReaderTest, TestFLACAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestWavPackAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.wv"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggFLACAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.oga"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggOpusAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.opus"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.spx"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestASFAudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestMP3AudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestMP4AudioFileSaveLyrics) {

  TemporaryResource r(u":/audio/strawberry.m4a"_s);
  TestSaveLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestFLACAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestWavPackAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.wv"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggFLACAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.oga"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggOpusAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.opus"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestOggSpeexAudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.spx"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestMP3AudioFileSaveSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  TestSaveSyncedLyrics(r.fileName());

}

TEST_F(TagReaderTest, TestMP3AudioFileSaveInvalidSyncedLyrics) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  // Synchronized lyrics are converted to SYLT frames for ID3v2, so they need to be valid LRC, and the existing ones are kept otherwise.
  EXPECT_TRUE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());
  EXPECT_FALSE(SaveSyncedLyricsToFile(u"Not LRC"_s, r.fileName()).success());

  const Song song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"[00:01.00]Line one"_s, song.synced_lyrics());

}

TEST_F(TagReaderTest, TestMP4AudioFileSaveSyncedLyricsUnsupported) {

  TemporaryResource r(u":/audio/strawberry.m4a"_s);
  EXPECT_FALSE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());

}

TEST_F(TagReaderTest, TestASFAudioFileSaveSyncedLyricsUnsupported) {

  TemporaryResource r(u":/audio/strawberry.asf"_s);
  EXPECT_FALSE(SaveSyncedLyricsToFile(u"[00:01.00]Line one"_s, r.fileName()).success());

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

TEST_F(TagReaderTest, TestCoverThatCannotBeLoadedKeepsExistingCover) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), cover_filename).success());

  // A cover file that does not exist.
  EXPECT_FALSE(WriteCoverToFile(r.fileName(), SaveTagCoverData(u"/nonexistent/strawberry-cover.png"_s)).success());
  EXPECT_EQ(ReadCoverFromFile(r.fileName()), original_image);

  // Cover data that is not an image.
  EXPECT_FALSE(WriteCoverToFile(r.fileName(), SaveTagCoverData(QByteArray("not an image"))).success());
  EXPECT_EQ(ReadCoverFromFile(r.fileName()), original_image);

  // Corrupt cover data with a JPEG or PNG mimetype.
  EXPECT_FALSE(WriteCoverToFile(r.fileName(), SaveTagCoverData(QByteArray("\xff\xd8\xff\xe0 not a JPEG"), u"image/jpeg"_s)).success());
  EXPECT_EQ(ReadCoverFromFile(r.fileName()), original_image);

  QFile cover_file(cover_filename);
  ASSERT_TRUE(cover_file.open(QIODevice::ReadOnly));
  const QByteArray png_data = cover_file.readAll();
  cover_file.close();
  EXPECT_FALSE(WriteCoverToFile(r.fileName(), SaveTagCoverData(png_data.left(png_data.size() / 2), u"image/png"_s)).success());
  EXPECT_EQ(ReadCoverFromFile(r.fileName()), original_image);

  // Data with the wrong mimetype is converted.
  EXPECT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData(png_data, u"image/jpeg"_s)).success());
  EXPECT_FALSE(ReadCoverFromFile(r.fileName()).isNull());

}

TEST_F(TagReaderTest, TestEmptyCoverRemovesCover) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), cover_filename).success());
  EXPECT_FALSE(ReadCoverFromFile(r.fileName()).isNull());

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  EXPECT_TRUE(ReadCoverFromFile(r.fileName()).isNull());

}

constexpr char kBackCoverData[] = "back cover";

TagLib::FLAC::Picture *CreateBackCoverPicture() {

  TagLib::FLAC::Picture *picture = new TagLib::FLAC::Picture();
  picture->setType(TagLib::FLAC::Picture::BackCover);
  picture->setMimeType("image/png");
  picture->setData(TagLib::ByteVector(kBackCoverData));
  return picture;

}

int BackCoverCount(const TagLib::List<TagLib::FLAC::Picture*> &pictures) {

  int count = 0;
  for (const TagLib::FLAC::Picture *picture : pictures) {
    if (picture->type() == TagLib::FLAC::Picture::BackCover && picture->data() == TagLib::ByteVector(kBackCoverData)) ++count;
  }
  return count;

}

int ID3v2BackCoverCount(const QString &filename) {

  TagLib::MPEG::File file(TagLibFileName(filename).data());
  if (!file.isValid() || !file.ID3v2Tag()) return -1;
  int count = 0;
  const TagLib::ID3v2::FrameList frames = file.ID3v2Tag()->frameList("APIC");
  for (TagLib::ID3v2::Frame *frame : frames) {
    const TagLib::ID3v2::AttachedPictureFrame *picture = dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(frame);
    if (picture && picture->type() == TagLib::ID3v2::AttachedPictureFrame::BackCover && picture->picture() == TagLib::ByteVector(kBackCoverData)) ++count;
  }
  return count;

}

// Setting and removing the cover only replaces the front cover, the other pictures, like the back cover, are kept.
TEST_F(TagReaderTest, TestMP3AudioFileCoverKeepsOtherPictures) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;
  QImage original_image;
  ASSERT_TRUE(original_image.load(cover_filename));

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame();
    frame->setType(TagLib::ID3v2::AttachedPictureFrame::BackCover);
    frame->setMimeType("image/png");
    frame->setPicture(TagLib::ByteVector(kBackCoverData));
    file.ID3v2Tag(true)->addFrame(frame);
    ASSERT_TRUE(file.save());
  }

  // The front cover is read, not the back cover before it.
  ASSERT_TRUE(WriteCoverToFile(r.fileName(), cover_filename).success());
  EXPECT_EQ(1, ID3v2BackCoverCount(r.fileName()));
  EXPECT_EQ(original_image, ReadCoverFromFile(r.fileName()));
  EXPECT_TRUE(ReadSongFromFile(r.fileName()).art_embedded());

  ASSERT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  EXPECT_EQ(1, ID3v2BackCoverCount(r.fileName()));
  EXPECT_TRUE(ReadCoverFromFile(r.fileName()).isNull());
  EXPECT_FALSE(ReadSongFromFile(r.fileName()).art_embedded());

}

// Returns a small PNG image in one color, to tell pictures apart.
QByteArray ColorPNGData(const QColor &color) {

  QImage image(8, 8, QImage::Format_RGB32);
  image.fill(color);
  QByteArray data;
  QBuffer buffer(&data);
  buffer.open(QIODevice::WriteOnly);
  image.save(&buffer, "PNG");
  return data;

}

void AddID3v2Picture(TagLib::ID3v2::Tag *tag, const TagLib::ID3v2::AttachedPictureFrame::Type type, const QByteArray &data) {

  TagLib::ID3v2::AttachedPictureFrame *frame = new TagLib::ID3v2::AttachedPictureFrame();
  frame->setType(type);
  frame->setMimeType("image/png");
  frame->setPicture(TagLib::ByteVector(data.constData(), static_cast<uint>(data.size())));
  tag->addFrame(frame);

}

// The front cover is preferred over a picture of type other before it, a picture of type other is used when there's no front cover.
TEST_F(TagReaderTest, TestMP3AudioFileCoverPrefersFrontCoverOverOther) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    AddID3v2Picture(file.ID3v2Tag(true), TagLib::ID3v2::AttachedPictureFrame::Other, ColorPNGData(Qt::red));
    AddID3v2Picture(file.ID3v2Tag(true), TagLib::ID3v2::AttachedPictureFrame::FrontCover, ColorPNGData(Qt::blue));
    ASSERT_TRUE(file.save());
  }
  QImage image = ReadCoverFromFile(r.fileName());
  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(QColor(Qt::blue), image.pixelColor(0, 0));

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    const TagLib::ID3v2::FrameList frames = file.ID3v2Tag()->frameList("APIC");
    for (TagLib::ID3v2::Frame *frame : frames) {
      const TagLib::ID3v2::AttachedPictureFrame *picture = dynamic_cast<const TagLib::ID3v2::AttachedPictureFrame*>(frame);
      if (picture && picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) file.ID3v2Tag()->removeFrame(frame);
    }
    ASSERT_TRUE(file.save());
  }
  image = ReadCoverFromFile(r.fileName());
  ASSERT_FALSE(image.isNull());
  EXPECT_EQ(QColor(Qt::red), image.pixelColor(0, 0));
  EXPECT_TRUE(ReadSongFromFile(r.fileName()).art_embedded());

}

// An empty front cover is not embedded art, it can't be loaded.
TEST_F(TagReaderTest, TestMP3AudioFileEmptyFrontCoverIsNotEmbeddedArt) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    AddID3v2Picture(file.ID3v2Tag(true), TagLib::ID3v2::AttachedPictureFrame::FrontCover, QByteArray());
    ASSERT_TRUE(file.save());
  }
  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    ASSERT_EQ(1U, file.ID3v2Tag()->frameList("APIC").size());
  }

  EXPECT_FALSE(ReadSongFromFile(r.fileName()).art_embedded());

}

TEST_F(TagReaderTest, TestFLACAudioFileCoverKeepsOtherPictures) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;
  QImage original_image;
  ASSERT_TRUE(original_image.load(cover_filename));

  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.addPicture(CreateBackCoverPicture());
    ASSERT_TRUE(file.save());
  }

  ASSERT_TRUE(WriteCoverToFile(r.fileName(), cover_filename).success());
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1, BackCoverCount(file.pictureList()));
  }
  EXPECT_EQ(original_image, ReadCoverFromFile(r.fileName()));

  ASSERT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1, BackCoverCount(file.pictureList()));
  }
  EXPECT_TRUE(ReadCoverFromFile(r.fileName()).isNull());

}

TEST_F(TagReaderTest, TestOggVorbisAudioFileCoverKeepsOtherPictures) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;
  QImage original_image;
  ASSERT_TRUE(original_image.load(cover_filename));

  {
    TagLib::Ogg::Vorbis::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->addPicture(CreateBackCoverPicture());
    ASSERT_TRUE(file.save());
  }

  ASSERT_TRUE(WriteCoverToFile(r.fileName(), cover_filename).success());
  {
    TagLib::Ogg::Vorbis::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1, BackCoverCount(file.tag()->pictureList()));
  }
  EXPECT_EQ(original_image, ReadCoverFromFile(r.fileName()));

  ASSERT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  {
    TagLib::Ogg::Vorbis::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1, BackCoverCount(file.tag()->pictureList()));
  }
  EXPECT_TRUE(ReadCoverFromFile(r.fileName()).isNull());

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

TEST_F(TagReaderTest, TestMP3AudioFileWithoutID3v2TagCover) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  const QString cover_filename = u":/pictures/strawberry.png"_s;

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    ASSERT_TRUE(file.strip(TagLib::MPEG::File::ID3v2));
  }

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    ASSERT_FALSE(file.hasID3v2Tag());
  }

  QImage original_image;
  EXPECT_TRUE(original_image.load(cover_filename));

  TagReaderResult result = WriteCoverToFile(r.fileName(), cover_filename);
  EXPECT_TRUE(result.success());

  const QImage new_image = ReadCoverFromFile(r.fileName());
  EXPECT_TRUE(!new_image.isNull());
  EXPECT_EQ(new_image, original_image);

}

TEST_F(TagReaderTest, TestMP3KeepsID3v2VersionWhenSaving) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  const auto id3v2_version = [&r]() {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    return file.isValid() && file.hasID3v2Tag() ? static_cast<int>(file.ID3v2Tag()->header()->majorVersion()) : 0;
  };

  // Convert the tag to ID3v2.3.
  Song song;
  song.set_title(u"Strawberry"_s);
  {
    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(r.fileName(), song, SaveTagsOption::Tags, SaveTagCoverData(), TagID3v2Version::V3);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    ASSERT_TRUE(reply->result().success());
  }
  ASSERT_EQ(3, id3v2_version());

  // Saving without an explicit version keeps ID3v2.3.
  song.set_artist(u"Strawberry Artist"_s);
  EXPECT_TRUE(WriteSongToFile(song, r.fileName()).success());
  EXPECT_EQ(3, id3v2_version());

  song.set_playcount(5);
  WriteSongPlaycountToFile(song, r.fileName());
  EXPECT_EQ(3, id3v2_version());

  {
    TagReaderReplyPtr reply = tagreader_client_->SaveSongRatingAsync(r.fileName(), 0.8F);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_TRUE(reply->result().success());
  }
  EXPECT_EQ(3, id3v2_version());

  EXPECT_TRUE(SaveLyricsToFile(u"strawberry lyrics"_s, r.fileName()).success());
  EXPECT_EQ(3, id3v2_version());

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), u":/pictures/strawberry.png"_s).success());
  EXPECT_EQ(3, id3v2_version());

  // An explicit version is still used.
  {
    TagReaderReplyPtr reply = tagreader_client_->WriteFileAsync(r.fileName(), song, SaveTagsOption::Tags, SaveTagCoverData(), TagID3v2Version::V4);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    EXPECT_TRUE(reply->result().success());
  }
  EXPECT_EQ(4, id3v2_version());

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

// Regression test: a loop sample count larger than the sample count in a VGM header must not make the length underflow.
TEST(TagReaderGMETest, VGMPlaybackLength) {

  const auto pack = [](const quint32 value) {
    QByteArray bytes(4, '\0');
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    return bytes;
  };

  quint64 length = 0;

  // 10 seconds without a loop.
  ASSERT_TRUE(GME::VGM::GetPlaybackLength(pack(441000), pack(0), length));
  EXPECT_EQ(10000ULL, length);

  // 2 seconds intro and 8 seconds loop, played twice, plus the fade out.
  ASSERT_TRUE(GME::VGM::GetPlaybackLength(pack(441000), pack(352800), length));
  EXPECT_EQ(2000ULL + (8000ULL * 2) + GME::VGM::GST_GME_LOOP_TIME_MS, length);

  // A loop longer than the song is ignored.
  ASSERT_TRUE(GME::VGM::GetPlaybackLength(pack(441000), pack(441001), length));
  EXPECT_EQ(10000ULL, length);

  EXPECT_FALSE(GME::VGM::GetPlaybackLength(pack(0), pack(0), length));

}

// Regression test: non-ASCII titles in SPC files were garbled, since they were decoded as Latin-1 twice.
TEST(TagReaderGMETest, SPCNonASCIIStrings) {

  QByteArray data(0x10200, '\0');
  data.replace(0, 33, "SNES-SPC700 Sound File Data v0.30");
  const auto set_field = [&data](const int offset, const QByteArray &value) { data.replace(offset, static_cast<int>(value.size()), value); };
  set_field(GME::SPC::SONG_TITLE_OFFSET, "Caf\xe9");
  set_field(GME::SPC::GAME_TITLE_OFFSET, "Gr\xfc\xdf" "e");
  set_field(GME::SPC::ARTIST_OFFSET, "Bj\xf6rk");

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"test.spc"_s);
  QFile file(filename);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_EQ(data.size(), file.write(data));
  file.close();

  Song song;
  ASSERT_TRUE(GME::ReadFile(QFileInfo(filename), &song).success());
  EXPECT_EQ(u"Café"_s, song.title());
  EXPECT_EQ(u"Grüße"_s, song.album());
  EXPECT_EQ(u"Björk"_s, song.artist());

}

// Regression test: non-ASCII titles in VGM files were garbled, since they were decoded as UTF-16, then encoded as UTF-8 and decoded as Latin-1.
TEST(TagReaderGMETest, VGMNonASCIIStrings) {

  const auto pack = [](const quint32 value) {
    QByteArray bytes(4, '\0');
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<char>((value >> (8 * i)) & 0xFF);
    return bytes;
  };

  // The GD3 strings are UTF-16LE, each terminated by a null character, in the order title, title (Japanese), album, album (Japanese), system, system (Japanese), artist, artist (Japanese), date, dumper and notes.
  const QStringList strings = { u"Café"_s, u"カフェ"_s, u"Grüße"_s, QString(), QString(), QString(), u"Björk"_s, QString(), u"1994/01/01"_s, QString(), QString() };
  QByteArray gd3_strings;
  for (const QString &string : strings) {
    for (const QChar c : string) {
      gd3_strings.append(static_cast<char>(c.unicode() & 0xFF));
      gd3_strings.append(static_cast<char>(c.unicode() >> 8));
    }
    gd3_strings.append(2, '\0');
  }

  QByteArray data(0x40, '\0');
  data.replace(0, 4, "Vgm ");
  data.replace(GME::VGM::SAMPLE_COUNT, 4, pack(441000));
  data.replace(GME::VGM::GD3_TAG_PTR, 4, pack(static_cast<quint32>(data.size() - GME::VGM::GD3_TAG_PTR)));
  data.append("Gd3 ");
  data.append(pack(0x100));
  data.append(pack(static_cast<quint32>(gd3_strings.size())));
  data.append(gd3_strings);

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString filename = temp_dir.filePath(u"test.vgm"_s);
  QFile file(filename);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  ASSERT_EQ(data.size(), file.write(data));
  file.close();

  Song song;
  ASSERT_TRUE(GME::ReadFile(QFileInfo(filename), &song).success());
  EXPECT_EQ(u"Café"_s, song.title());
  EXPECT_EQ(u"Grüße"_s, song.album());
  EXPECT_EQ(u"Björk"_s, song.artist());
  EXPECT_EQ(1994, song.year());

}

TEST_F(TagReaderTest, TestExitWritesQueuedRequests) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  // Block the tagreader thread, and queue the exit before the requests, so Exit() runs before the requests are processed, and has to write them itself.
  QSemaphore blocker;
  QMetaObject::invokeMethod(tagreader_client_, [&blocker]() { blocker.acquire(); }, Qt::QueuedConnection);

  // Wait for the exit without an event loop, since the queued ProcessRequests() is moved to this thread with the tagreader, and would otherwise process the requests here.
  QSemaphore exit_finished;
  QObject::connect(tagreader_client_, &TagReaderClient::ExitFinished, tagreader_client_, [&exit_finished]() { exit_finished.release(); }, Qt::DirectConnection);
  tagreader_client_->ExitAsync();

  TagReaderReadFileReplyPtr read_reply = tagreader_client_->ReadFileAsync(r.fileName());
  TagReaderReplyPtr playcount_reply = tagreader_client_->SaveSongPlaycountAsync(r.fileName(), 42);
  TagReaderReplyPtr lyrics_reply = tagreader_client_->SaveSongLyricsAsync(r.fileName(), u"Lyrics saved when exiting"_s);

  blocker.release();
  ASSERT_TRUE(exit_finished.tryAcquire(1, 10000));

  // Reading the file is not needed anymore when exiting, but the changes to the tags are written.
  EXPECT_FALSE(read_reply->finished());
  EXPECT_TRUE(playcount_reply->finished());
  EXPECT_TRUE(playcount_reply->success());
  EXPECT_TRUE(lyrics_reply->finished());
  EXPECT_TRUE(lyrics_reply->success());

  Song song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(r.fileName(), &song).success());
  EXPECT_EQ(42U, song.playcount());
  EXPECT_EQ(u"Lyrics saved when exiting"_s, song.lyrics());

}

TEST_F(TagReaderTest, TestAbortReadsKeepsWrites) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  // Block the tagreader thread, so both requests are queued when the reads are aborted.
  QSemaphore blocker;
  QMetaObject::invokeMethod(tagreader_client_, [&blocker]() { blocker.acquire(); }, Qt::QueuedConnection);

  tagreader_client_->AbortReads();
  TagReaderReadFileReplyPtr read_reply = tagreader_client_->ReadFileAsync(r.fileName());
  TagReaderReplyPtr playcount_reply = tagreader_client_->SaveSongPlaycountAsync(r.fileName(), 5);

  QEventLoop loop;
  QObject::connect(&*playcount_reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
  blocker.release();
  loop.exec();

  // The reads are dropped when the application starts exiting, before the tagreader exits, but the writes are still processed.
  EXPECT_FALSE(read_reply->finished());
  EXPECT_TRUE(playcount_reply->finished());
  EXPECT_TRUE(playcount_reply->success());

}

// The track and disc totals are not stored in the song, saving the tags keeps them.
TEST_F(TagReaderTest, TestMP3AudioFileSaveKeepsTrackAndDiscTotals) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
    tag->removeFrames("TRCK");
    tag->removeFrames("TPOS");
    TagLib::ID3v2::TextIdentificationFrame *track_frame = new TagLib::ID3v2::TextIdentificationFrame("TRCK", TagLib::String::UTF8);
    track_frame->setText("3/12");
    tag->addFrame(track_frame);
    TagLib::ID3v2::TextIdentificationFrame *disc_frame = new TagLib::ID3v2::TextIdentificationFrame("TPOS", TagLib::String::UTF8);
    disc_frame->setText("1/2");
    tag->addFrame(disc_frame);
    ASSERT_TRUE(file.save());
  }

  Song song;
  song.set_track(4);
  song.set_disc(2);
  ASSERT_TRUE(WriteSongToFile(song, r.fileName()).success());

  {
    TagLib::MPEG::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    TagLib::ID3v2::Tag *tag = file.ID3v2Tag();
    ASSERT_TRUE(tag);
    ASSERT_FALSE(tag->frameList("TRCK").isEmpty());
    ASSERT_FALSE(tag->frameList("TPOS").isEmpty());
    EXPECT_EQ(TagLib::String("4/12"), tag->frameList("TRCK").front()->toString());
    EXPECT_EQ(TagLib::String("2/2"), tag->frameList("TPOS").front()->toString());
  }

  const Song song_read = ReadSongFromFile(r.fileName());
  EXPECT_EQ(4, song_read.track());
  EXPECT_EQ(2, song_read.disc());

}

TEST_F(TagReaderTest, TestFLACAudioFileSaveKeepsTrackAndDiscTotals) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    TagLib::Ogg::XiphComment *vorbis_comment = file.xiphComment(true);
    vorbis_comment->addField("TRACKNUMBER", "3/12", true);
    vorbis_comment->addField("DISCNUMBER", "1/2", true);
    ASSERT_TRUE(file.save());
  }

  Song song;
  song.set_track(4);
  song.set_disc(2);
  ASSERT_TRUE(WriteSongToFile(song, r.fileName()).success());

  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    const TagLib::Ogg::FieldListMap fields = file.xiphComment()->fieldListMap();
    EXPECT_EQ(TagLib::StringList("4/12"), fields["TRACKNUMBER"]);
    EXPECT_EQ(TagLib::StringList("2/2"), fields["DISCNUMBER"]);
  }

  const Song song_read = ReadSongFromFile(r.fileName());
  EXPECT_EQ(4, song_read.track());
  EXPECT_EQ(2, song_read.disc());

}

TEST_F(TagReaderTest, TestWavPackAudioFileSaveKeepsTrackAndDiscTotals) {

  TemporaryResource r(u":/audio/strawberry.wv"_s);

  {
    TagLib::WavPack::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    TagLib::APE::Tag *tag = file.APETag(true);
    tag->addValue("TRACK", "3/12", true);
    tag->addValue("DISC", "1/2", true);
    ASSERT_TRUE(file.save());
  }

  Song song;
  song.set_track(4);
  song.set_disc(2);
  ASSERT_TRUE(WriteSongToFile(song, r.fileName()).success());

  {
    TagLib::WavPack::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    const TagLib::APE::ItemListMap items = file.APETag()->itemListMap();
    EXPECT_EQ(TagLib::String("4/12"), items["TRACK"].toString());
    EXPECT_EQ(TagLib::String("2/2"), items["DISC"].toString());
  }

  const Song song_read = ReadSongFromFile(r.fileName());
  EXPECT_EQ(4, song_read.track());
  EXPECT_EQ(2, song_read.disc());

}

TEST_F(TagReaderTest, TestM4AAudioFileSaveKeepsTrackAndDiscTotals) {

  TemporaryResource r(u":/audio/strawberry.m4a"_s);

  {
    TagLib::MP4::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    file.tag()->setItem("trkn", TagLib::MP4::Item(3, 12));
    file.tag()->setItem("disk", TagLib::MP4::Item(1, 2));
    ASSERT_TRUE(file.save());
  }

  Song song;
  song.set_track(4);
  song.set_disc(2);
  ASSERT_TRUE(WriteSongToFile(song, r.fileName()).success());

  {
    TagLib::MP4::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    const TagLib::MP4::Item::IntPair track = file.tag()->item("trkn").toIntPair();
    const TagLib::MP4::Item::IntPair disc = file.tag()->item("disk").toIntPair();
    EXPECT_EQ(4, track.first);
    EXPECT_EQ(12, track.second);
    EXPECT_EQ(2, disc.first);
    EXPECT_EQ(2, disc.second);
  }

  const Song song_read = ReadSongFromFile(r.fileName());
  EXPECT_EQ(4, song_read.track());
  EXPECT_EQ(2, song_read.disc());

}

TEST_F(TagReaderTest, TestRequestsAfterExit) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);

  QEventLoop loop;
  QObject::connect(tagreader_client_, &TagReaderClient::ExitFinished, &loop, &QEventLoop::quit);
  tagreader_client_->ExitAsync();
  loop.exec();

  // Requests made after exiting, from the thread the tagreader moved back to, for example when a write finished during exiting and the file is read again.
  // Reading is not done, writing is done directly.
  TagReaderReadFileReplyPtr read_reply = tagreader_client_->ReadFileAsync(r.fileName());
  TagReaderReplyPtr playcount_reply = tagreader_client_->SaveSongPlaycountAsync(r.fileName(), 9);

  EXPECT_FALSE(read_reply->finished());
  EXPECT_TRUE(playcount_reply->finished());
  EXPECT_TRUE(playcount_reply->success());

  Song song;
  ASSERT_TRUE(TagReaderTagLib().ReadFile(r.fileName(), &song).success());
  EXPECT_EQ(9U, song.playcount());

}

// Updating a file reads the tags from the file, and writes only the changed tags.
TEST_F(TagReaderTest, TestUpdateFile) {

  TemporaryResource r(u":/audio/strawberry.mp3"_s);
  Song song;
  song.set_title(u"Title"_s);
  song.set_artist(u"Artist"_s);
  song.set_composer(u"Composer"_s);
  ASSERT_TRUE(WriteSongToFile(song, r.fileName()).success());

  const auto update_file = [this, &r](const std::function<bool(Song *song)> &update_song) {
    TagReaderReplyPtr reply = tagreader_client_->UpdateFileAsync(r.fileName(), update_song);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->result();
  };

  // The update gets the tags of the file.
  QString composer;
  EXPECT_TRUE(update_file([&composer](Song *file_song) {
    composer = file_song->composer();
    file_song->set_artist(u"Updated artist"_s);
    return true;
  }).success());
  EXPECT_EQ(u"Composer"_s, composer);
  song = ReadSongFromFile(r.fileName());
  EXPECT_EQ(u"Updated artist"_s, song.artist());
  EXPECT_EQ(u"Title"_s, song.title());
  EXPECT_EQ(u"Composer"_s, song.composer());

  // The file isn't written when the update can't change the tags.
  EXPECT_FALSE(update_file([](Song *file_song) {
    file_song->set_title(u"Not written"_s);
    return false;
  }).success());
  EXPECT_EQ(u"Title"_s, ReadSongFromFile(r.fileName()).title());

}

// Adds a cover in the old base64 encoded COVERART field, TagLib reads it as a picture of the Vorbis comment.
void AddLegacyCoverArt(TagLib::Ogg::XiphComment *vorbis_comment) {
  QFile cover_file(u":/pictures/strawberry.png"_s);
  ASSERT_TRUE(cover_file.open(QIODevice::ReadOnly));
  const QByteArray cover_data = cover_file.readAll().toBase64();
  vorbis_comment->addField("COVERART", TagLib::String(cover_data.constData()), true);
  vorbis_comment->addField("COVERARTMIME", "image/png", true);
}

// Regression test: removing the cover left the COVERARTMIME field of the cover in the old COVERART field.
TEST_F(TagReaderTest, TestOggLegacyCoverArtRemoved) {

  TemporaryResource r(u":/audio/strawberry.ogg"_s);
  {
    TagLib::Ogg::Vorbis::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    AddLegacyCoverArt(file.tag());
    ASSERT_TRUE(file.save());
  }
  ASSERT_FALSE(ReadCoverFromFile(r.fileName()).isNull());

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  EXPECT_TRUE(ReadCoverFromFile(r.fileName()).isNull());
  EXPECT_FALSE(ReadSongFromFile(r.fileName()).art_embedded());

  TagLib::Ogg::Vorbis::File file(TagLibFileName(r.fileName()).data());
  ASSERT_TRUE(file.isValid());
  EXPECT_TRUE(file.tag()->pictureList().isEmpty());
  EXPECT_FALSE(file.tag()->contains("COVERART"));
  EXPECT_FALSE(file.tag()->contains("COVERARTMIME"));

}

// Regression test: the covers in the Vorbis comment of a FLAC file, like the cover in the old COVERART field, were not removed when the cover was removed or replaced, and other players show them.
TEST_F(TagReaderTest, TestFLACVorbisCommentCoverRemoved) {

  TemporaryResource r(u":/audio/strawberry.flac"_s);
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    AddLegacyCoverArt(file.xiphComment(true));
    ASSERT_TRUE(file.save());
  }
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    ASSERT_TRUE(file.xiphComment());
    ASSERT_EQ(1U, file.xiphComment()->pictureList().size());
  }

  // Replacing the cover saves it as a picture of the FLAC file, the old cover is removed.
  EXPECT_TRUE(WriteCoverToFile(r.fileName(), u":/pictures/strawberry.png"_s).success());
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_EQ(1U, file.pictureList().size());
    EXPECT_TRUE(!file.xiphComment() || file.xiphComment()->pictureList().isEmpty());
    EXPECT_TRUE(!file.xiphComment() || !file.xiphComment()->contains("COVERARTMIME"));
  }

  EXPECT_TRUE(WriteCoverToFile(r.fileName(), SaveTagCoverData()).success());
  {
    TagLib::FLAC::File file(TagLibFileName(r.fileName()).data());
    ASSERT_TRUE(file.isValid());
    EXPECT_TRUE(file.pictureList().isEmpty());
    EXPECT_TRUE(!file.xiphComment() || file.xiphComment()->pictureList().isEmpty());
  }

}

#if defined(HAVE_TAGLIB_DSFFILE) || defined(HAVE_TAGLIB_DSDIFFFILE)

// Writes a file with one block of DSD silence, the size of the samples is 1 bit.
constexpr quint32 kDSDSampleRate = 2822400;
constexpr quint32 kDSDBlockSize = 4096;
constexpr char kDSDSilence = 0x69;

// Checks that saving keeps the ID3v2 version of the file, and that an explicit version is used.
template <typename ID3v2VersionFunc>
void CheckKeepsID3v2Version(TagReaderClient *tagreader_client, const QString &filename, ID3v2VersionFunc id3v2_version) {

  const auto write_file = [tagreader_client, &filename](const Song &song, const TagID3v2Version tag_id3v2_version) {
    TagReaderReplyPtr reply = tagreader_client->WriteFileAsync(filename, song, SaveTagsOption::Tags, SaveTagCoverData(), tag_id3v2_version);
    QEventLoop loop;
    QObject::connect(&*reply, &TagReaderReply::Finished, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->result().success();
  };

  Song song;
  song.set_title(u"Strawberry"_s);
  ASSERT_TRUE(write_file(song, TagID3v2Version::V3));
  ASSERT_EQ(3, id3v2_version());

  song.set_artist(u"Strawberry Artist"_s);
  EXPECT_TRUE(write_file(song, TagID3v2Version::Default));
  EXPECT_EQ(3, id3v2_version());

  EXPECT_TRUE(write_file(song, TagID3v2Version::V4));
  EXPECT_EQ(4, id3v2_version());

}

#endif

#ifdef HAVE_TAGLIB_DSFFILE

QByteArray DSFFileData() {

  QByteArray data;
  QDataStream s(&data, QIODevice::WriteOnly);
  s.setByteOrder(QDataStream::LittleEndian);
  const quint64 data_chunk_size = 12 + (kDSDBlockSize * 2);
  const quint64 file_size = 28 + 52 + data_chunk_size;
  s.writeRawData("DSD ", 4);
  s << quint64(28) << file_size << quint64(0);
  s.writeRawData("fmt ", 4);
  // Version, DSD raw, stereo, 2 channels, sample rate, bits per sample, samples, block size per channel, reserved.
  s << quint64(52) << quint32(1) << quint32(0) << quint32(2) << quint32(2) << kDSDSampleRate << quint32(1) << quint64(kDSDBlockSize * 8) << kDSDBlockSize << quint32(0);
  s.writeRawData("data", 4);
  s << data_chunk_size;
  data.append(QByteArray(kDSDBlockSize * 2, kDSDSilence));
  return data;

}

// Regression test: DSF files were saved with ID3v2.4 tags, ID3v2.3 tags were converted.
TEST_F(TagReaderTest, TestDSFKeepsID3v2VersionWhenSaving) {

  TemporaryDataFile temporary_file(u"dsf"_s, DSFFileData());
  const QString filename = temporary_file.fileName();
  {
    TagLib::DSF::File file(TagLibFileName(filename).data());
    ASSERT_TRUE(file.isValid());
  }

  CheckKeepsID3v2Version(tagreader_client_, filename, [&filename]() {
    TagLib::DSF::File file(TagLibFileName(filename).data());
    return file.isValid() && file.tag() && !file.tag()->isEmpty() ? static_cast<int>(file.tag()->header()->majorVersion()) : 0;
  });

}

#endif

#ifdef HAVE_TAGLIB_DSDIFFFILE

// Writes a chunk of a DSDIFF file, the chunks have an even size.
void WriteDSDIFFChunk(QDataStream &s, const char *id, const QByteArray &chunk_data) {
  s.writeRawData(id, 4);
  s << quint64(chunk_data.size());
  s.writeRawData(chunk_data.constData(), static_cast<int>(chunk_data.size()));
  if (chunk_data.size() % 2 != 0) s << quint8(0);
}

QByteArray DSDIFFFileData() {

  QByteArray fver;
  QDataStream(&fver, QIODevice::WriteOnly) << quint32(0x01050000);

  QByteArray fs;
  QDataStream(&fs, QIODevice::WriteOnly) << kDSDSampleRate;
  QByteArray chnl;
  {
    QDataStream s(&chnl, QIODevice::WriteOnly);
    s << quint16(2);
    s.writeRawData("SLFT", 4);
    s.writeRawData("SRGT", 4);
  }
  QByteArray cmpr;
  {
    QDataStream s(&cmpr, QIODevice::WriteOnly);
    s.writeRawData("DSD ", 4);
    s << quint8(14);
    s.writeRawData("not compressed", 14);
  }
  QByteArray prop;
  {
    QDataStream s(&prop, QIODevice::WriteOnly);
    s.writeRawData("SND ", 4);
    WriteDSDIFFChunk(s, "FS  ", fs);
    WriteDSDIFFChunk(s, "CHNL", chnl);
    WriteDSDIFFChunk(s, "CMPR", cmpr);
  }

  QByteArray form;
  {
    QDataStream s(&form, QIODevice::WriteOnly);
    s.writeRawData("DSD ", 4);
    WriteDSDIFFChunk(s, "FVER", fver);
    WriteDSDIFFChunk(s, "PROP", prop);
    WriteDSDIFFChunk(s, "DSD ", QByteArray(kDSDBlockSize * 2, kDSDSilence));
  }

  QByteArray data;
  QDataStream s(&data, QIODevice::WriteOnly);
  WriteDSDIFFChunk(s, "FRM8", form);
  return data;

}

// Regression test: DSDIFF files were saved with ID3v2.4 tags, ID3v2.3 tags were converted.
TEST_F(TagReaderTest, TestDSDIFFKeepsID3v2VersionWhenSaving) {

  TemporaryDataFile temporary_file(u"dff"_s, DSDIFFFileData());
  const QString filename = temporary_file.fileName();
  {
    TagLib::DSDIFF::File file(TagLibFileName(filename).data());
    ASSERT_TRUE(file.isValid());
  }

  CheckKeepsID3v2Version(tagreader_client_, filename, [&filename]() {
    TagLib::DSDIFF::File file(TagLibFileName(filename).data());
    return file.isValid() && file.hasID3v2Tag() ? static_cast<int>(file.ID3v2Tag()->header()->majorVersion()) : 0;
  });

}

#endif

}  // namespace
