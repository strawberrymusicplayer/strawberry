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
#include "test_utils.h"

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "core/song.h"
#include "includes/shared_ptr.h"
#include "playlistparsers/cueparser.h"
#include "playlistparsers/parserbase.h"
#include "tagreader/tagreaderclient.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

class CueParserTest : public ::testing::Test {
 protected:
  ~CueParserTest() override {
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

  // The shared_ptr uses a no-op deleter because tagreader_client_ lifetime is managed by the fixture (deleteLater in the destructor).
  CueParser MakeParser() const {
    return CueParser(SharedPtr<TagReaderClient>(tagreader_client_, [](TagReaderClient *tagreader_client) { Q_UNUSED(tagreader_client) }), nullptr);
  }

  QThread *tagreader_client_thread_ = nullptr;
  TagReaderClient *tagreader_client_ = nullptr;
};

// Regression test: a REM comment without a value in the header must not end the header, the album metadata after it used to be lost.
TEST_F(CueParserTest, RemWithoutValueKeepsHeader) {

  TemporaryResource audio(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(QFile::exists(audio.fileName()));
  const QFileInfo audio_info(audio.fileName());

  QByteArray cue;
  cue += "REM COMMENT\n";
  cue += "PERFORMER \"Album Artist\"\n";
  cue += "TITLE \"Album\"\n";
  cue += "REM DATE 1999\n";
  cue += "FILE \"" + audio_info.fileName().toUtf8() + "\" MP3\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"First\"\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Second\"\n";
  cue += "    INDEX 01 00:01:00\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const ParserBase::LoadResult result = parser.Load(&buffer, audio_info.dir().filePath(u"album.cue"_s), audio_info.dir(), false);

  ASSERT_GE(result.songs.size(), 1);
  EXPECT_EQ(u"First"_s, result.songs[0].title());
  EXPECT_EQ(u"Album"_s, result.songs[0].album());
  EXPECT_EQ(u"Album Artist"_s, result.songs[0].albumartist());
  EXPECT_EQ(1999, result.songs[0].year());

}

// A CUE sheet with several FILE sections returns the songs of each media file separately, so the collection watcher only adds the songs of the media file it's scanning.
TEST_F(CueParserTest, LoadByFileGroupsSongsByMediaFile) {

  TemporaryResource audio1(u":/audio/strawberry.mp3"_s);
  TemporaryResource audio2(u":/audio/strawberry.flac"_s);
  ASSERT_TRUE(QFile::exists(audio1.fileName()));
  ASSERT_TRUE(QFile::exists(audio2.fileName()));
  const QFileInfo audio1_info(audio1.fileName());
  const QFileInfo audio2_info(audio2.fileName());
  ASSERT_EQ(audio1_info.dir(), audio2_info.dir());

  QByteArray cue;
  cue += "TITLE \"Album\"\n";
  cue += "FILE \"" + audio1_info.fileName().toUtf8() + "\" MP3\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"First\"\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Second\"\n";
  cue += "    INDEX 01 00:01:00\n";
  cue += "FILE \"" + audio2_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "  TRACK 03 AUDIO\n";
  cue += "    TITLE \"Third\"\n";
  cue += "    INDEX 01 00:00:00\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const QMap<QString, SongList> songs_by_file = parser.LoadByFile(&buffer, audio1_info.dir().filePath(u"album.cue"_s), audio1_info.dir(), false);

  ASSERT_EQ(2, songs_by_file.count());

  const SongList songs1 = songs_by_file.value(audio1_info.absoluteFilePath());
  ASSERT_EQ(2, songs1.count());
  EXPECT_EQ(u"First"_s, songs1[0].title());
  EXPECT_EQ(u"Second"_s, songs1[1].title());

  const SongList songs2 = songs_by_file.value(audio2_info.absoluteFilePath());
  ASSERT_EQ(1, songs2.count());
  EXPECT_EQ(u"Third"_s, songs2[0].title());
  EXPECT_EQ(3, songs2[0].track());
  EXPECT_EQ(QUrl::fromLocalFile(audio2_info.absoluteFilePath()), songs2[0].url());

}

// Regression test: when the gaps are appended to the previous track, as EAC writes them for one file per track, the TRACK line and the pregap with INDEX 00 are in the previous file, so the track was only the pregap at the end of the previous file.
TEST_F(CueParserTest, TrackWithPregapInPreviousFile) {

  TemporaryResource audio1(u":/audio/strawberry.flac"_s);
  TemporaryResource audio2(u":/audio/strawberry.flac"_s);
  TemporaryResource audio3(u":/audio/strawberry.flac"_s);
  const QFileInfo audio1_info(audio1.fileName());
  const QFileInfo audio2_info(audio2.fileName());
  const QFileInfo audio3_info(audio3.fileName());
  ASSERT_TRUE(audio1_info.exists());
  ASSERT_TRUE(audio2_info.exists());
  ASSERT_TRUE(audio3_info.exists());
  ASSERT_EQ(audio1_info.dir(), audio2_info.dir());
  ASSERT_EQ(audio1_info.dir(), audio3_info.dir());

  QByteArray cue;
  cue += "TITLE \"Album\"\n";
  cue += "FILE \"" + audio1_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"First\"\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Second\"\n";
  cue += "    PERFORMER \"Second Artist\"\n";
  cue += "    INDEX 00 00:01:00\n";
  cue += "FILE \"" + audio2_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "  TRACK 03 AUDIO\n";
  cue += "    TITLE \"Third\"\n";
  cue += "    INDEX 00 00:01:00\n";
  cue += "FILE \"" + audio3_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "    INDEX 01 00:00:00\n";

  const QString cue_path = audio1_info.dir().filePath(u"album.cue"_s);
  CueParser parser = MakeParser();

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
  const ParserBase::LoadResult result = parser.Load(&buffer, cue_path, audio1_info.dir(), false);
  ASSERT_EQ(3, result.songs.size());

  const QStringList titles = QStringList() << u"First"_s << u"Second"_s << u"Third"_s;
  const QList<QFileInfo> audio_infos = QList<QFileInfo>() << audio1_info << audio2_info << audio3_info;
  for (qsizetype i = 0; i < result.songs.size(); ++i) {
    const Song &song = result.songs.at(i);
    EXPECT_EQ(titles.at(i), song.title());
    EXPECT_EQ(i + 1, song.track());
    EXPECT_EQ(QUrl::fromLocalFile(audio_infos.at(i).absoluteFilePath()), song.url());
    EXPECT_EQ(0, song.beginning_nanosec());
    // Each track is the whole file, the pregap of the next track is played with the track.
    EXPECT_GT(song.length_nanosec(), 1'500'000'000LL);
  }
  EXPECT_EQ(u"Second Artist"_s, result.songs.at(1).artist());

  buffer.seek(0);
  const QMap<QString, SongList> songs_by_file = parser.LoadByFile(&buffer, cue_path, audio1_info.dir(), false);
  ASSERT_EQ(3, songs_by_file.count());
  for (qsizetype i = 0; i < audio_infos.size(); ++i) {
    const SongList songs = songs_by_file.value(audio_infos.at(i).absoluteFilePath());
    ASSERT_EQ(1, songs.count());
    EXPECT_EQ(titles.at(i), songs.first().title());
  }

  buffer.seek(0);
  EXPECT_EQ(QStringList() << QDir::cleanPath(audio1_info.absoluteFilePath()) << QDir::cleanPath(audio2_info.absoluteFilePath()) << QDir::cleanPath(audio3_info.absoluteFilePath()), parser.MediaFiles(&buffer, audio1_info.dir()));

}

// Regression test: a track with only INDEX 00, followed by the FILE and TRACK of the next track, was dropped, since its index was taken as a pregap in the next file.
TEST_F(CueParserTest, TrackWithOnlyIndex00BeforeNextFile) {

  TemporaryResource audio1(u":/audio/strawberry.flac"_s);
  TemporaryResource audio2(u":/audio/strawberry.flac"_s);
  const QFileInfo audio1_info(audio1.fileName());
  const QFileInfo audio2_info(audio2.fileName());
  ASSERT_TRUE(audio1_info.exists());
  ASSERT_TRUE(audio2_info.exists());
  ASSERT_EQ(audio1_info.dir(), audio2_info.dir());

  QByteArray cue;
  cue += "FILE \"" + audio1_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"First\"\n";
  cue += "    INDEX 00 00:00:00\n";
  cue += "FILE \"" + audio2_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Second\"\n";
  cue += "    INDEX 01 00:00:00\n";

  const QString cue_path = audio1_info.dir().filePath(u"album.cue"_s);
  CueParser parser = MakeParser();

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));
  const ParserBase::LoadResult result = parser.Load(&buffer, cue_path, audio1_info.dir(), false);
  ASSERT_EQ(2, result.songs.size());
  EXPECT_EQ(u"First"_s, result.songs.at(0).title());
  EXPECT_EQ(QUrl::fromLocalFile(audio1_info.absoluteFilePath()), result.songs.at(0).url());
  EXPECT_EQ(u"Second"_s, result.songs.at(1).title());
  EXPECT_EQ(QUrl::fromLocalFile(audio2_info.absoluteFilePath()), result.songs.at(1).url());

  buffer.seek(0);
  EXPECT_EQ(QStringList() << QDir::cleanPath(audio1_info.absoluteFilePath()) << QDir::cleanPath(audio2_info.absoluteFilePath()), parser.MediaFiles(&buffer, audio1_info.dir()));

}

// An INDEX without a position after the FILE line is ignored, like other INDEX lines without a position, so the track stays in its own file.
TEST_F(CueParserTest, IndexWithoutPositionAfterFileIsIgnored) {

  TemporaryResource audio(u":/audio/strawberry.flac"_s);
  const QFileInfo audio_info(audio.fileName());
  ASSERT_TRUE(audio_info.exists());

  QByteArray cue;
  cue += "FILE \"" + audio_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"First\"\n";
  cue += "    INDEX 00 00:00:00\n";
  cue += "FILE \"second.flac\" WAVE\n";
  cue += "    INDEX 01\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const ParserBase::LoadResult result = parser.Load(&buffer, audio_info.dir().filePath(u"album.cue"_s), audio_info.dir(), false);
  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(u"First"_s, result.songs.first().title());
  EXPECT_EQ(QUrl::fromLocalFile(audio_info.absoluteFilePath()), result.songs.first().url());

}

// A track with the pregap in the previous file which is a data file is still loaded, the data file is only the previous file.
TEST_F(CueParserTest, TrackWithPregapInPreviousDataFile) {

  TemporaryResource audio(u":/audio/strawberry.flac"_s);
  const QFileInfo audio_info(audio.fileName());
  ASSERT_TRUE(audio_info.exists());

  QByteArray cue;
  cue += "FILE \"data.bin\" BINARY\n";
  cue += "  TRACK 01 MODE1/2352\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Audio\"\n";
  cue += "    INDEX 00 00:01:00\n";
  cue += "FILE \"" + audio_info.fileName().toUtf8() + "\" WAVE\n";
  cue += "    INDEX 01 00:00:00\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const ParserBase::LoadResult result = parser.Load(&buffer, audio_info.dir().filePath(u"album.cue"_s), audio_info.dir(), false);
  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(u"Audio"_s, result.songs.first().title());
  EXPECT_EQ(QUrl::fromLocalFile(audio_info.absoluteFilePath()), result.songs.first().url());

}

TEST_F(CueParserTest, TryMagicRecognisesCueSheets) {

  CueParser parser = MakeParser();

  // Like Exact Audio Copy writes it.
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\r\nREM DATE 1999\r\nREM DISCID 860B640B\r\nREM COMMENT \"ExactAudioCopy v1.6\"\r\nPERFORMER \"Artist\"\r\nTITLE \"Album\"\r\nFILE \"Album.flac\" WAVE\r\n  TRACK 01 AUDIO\r\n    INDEX 01 00:00:00\r\n"));
  // Without quotes and in lower case.
  EXPECT_TRUE(parser.TryMagic("performer Artist\ntitle Album\nfile album.wav wave\n  track 01 audio\n    index 01 00:00:00\n"));
  EXPECT_TRUE(parser.TryMagic("TITLE \"Album\"\n"));
  EXPECT_TRUE(parser.TryMagic("PERFORMER \"Artist\"\n"));
  EXPECT_TRUE(parser.TryMagic("FILE album.bin BINARY\n"));
  EXPECT_TRUE(parser.TryMagic("FILE \"Album.flac\" FLAC\n"));
  EXPECT_FALSE(parser.TryMagic("FILE album.flac FLAC\n"));
  EXPECT_TRUE(parser.TryMagic("  TRACK 02 MODE1/2352\n"));

}

// Only the start of the data is checked, so the line after long REM lines can be cut off.
TEST_F(CueParserTest, TryMagicRecognisesCutOffLine) {

  CueParser parser = MakeParser();

  QByteArray data;
  while (data.size() < 450) {
    data += "REM COMMENT \"A long comment which fills the start of the CUE sheet\"\r\n";
  }
  data += "PERFORMER \"An artist with a very long name which doesn't fit in the start of the CUE sheet\"\r\nTITLE \"Album\"\r\n";
  const QByteArray magic = data.left(512);
  ASSERT_FALSE(magic.contains("TITLE"));
  EXPECT_TRUE(parser.TryMagic(magic));

  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nTITLE \"Alb"));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nFILE \"Album.fl"));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nFILE \"Album.flac\" "));

  // Complete lines at the end without a newline.
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nTITLE \"Album\""));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nFILE \"Album.flac\" WAVE"));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nTRACK 01 AUDIO"));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nFILE album.wav WAVE"));
  EXPECT_TRUE(parser.TryMagic("REM GENRE Rock\nFILE album.bin BINARY"));

  // The start of the data ends right after an unquoted FILE line.
  QByteArray unquoted_data;
  while (unquoted_data.size() < 450) {
    unquoted_data += "REM COMMENT \"A long comment which fills the start of the CUE sheet\"\r\n";
  }
  unquoted_data += "FILE album.wav WAVE";
  EXPECT_TRUE(parser.TryMagic(unquoted_data));

  // Only the last line can be cut off.
  EXPECT_FALSE(parser.TryMagic("REM GENRE Rock\nTITLE \"Alb\n"));
  EXPECT_FALSE(parser.TryMagic("TITLE \"Alb\nREM GENRE Rock"));

}

// Regression test: an audio file with a CUE sheet in its tags near the start was taken as a CUE sheet, so it was loaded without songs.
TEST_F(CueParserTest, TryMagicDoesNotRecogniseAudioFileWithCueSheetTag) {

  CueParser parser = MakeParser();

  // A FLAC file with the STREAMINFO block, and a VORBIS_COMMENT block with a CUESHEET tag.
  const QByteArray vendor = "reference libFLAC 1.4.3 20230623";
  const QByteArray tag = "CUESHEET=REM GENRE Rock\r\nPERFORMER \"Artist\"\r\nTITLE \"Album\"\r\nFILE \"Album.flac\" WAVE\r\n  TRACK 01 AUDIO\r\n    INDEX 01 00:00:00\r\n";
  QByteArray vorbis_comment;
  vorbis_comment += QByteArray(1, static_cast<char>(vendor.size())) + QByteArray(3, '\0') + vendor;
  vorbis_comment += QByteArray(1, '\1') + QByteArray(3, '\0');
  vorbis_comment += QByteArray(1, static_cast<char>(tag.size())) + QByteArray(3, '\0') + tag;
  QByteArray data = "fLaC";
  data += QByteArray("\x00\x00\x00\x22", 4) + QByteArray(34, '\x11');
  data += QByteArray(1, '\x84') + QByteArray(1, '\0') + QByteArray(1, '\0') + QByteArray(1, static_cast<char>(vorbis_comment.size())) + vorbis_comment;
  ASSERT_LT(data.size(), 512);
  ASSERT_TRUE(data.contains("\r\nTITLE \"Album\"\r\n"));

  EXPECT_FALSE(parser.TryMagic(data));

}

// Regression test: a plain list of files without a header, like an M3U playlist, was taken as a CUE sheet when a line started with one of the CUE words, so it was loaded without songs.
TEST_F(CueParserTest, TryMagicDoesNotRecognisePlainFileList) {

  CueParser parser = MakeParser();

  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nTracks/02.flac\n"));
  EXPECT_FALSE(parser.TryMagic("Title Fight/Song.mp3\n"));
  EXPECT_FALSE(parser.TryMagic("Track 01.mp3\nTrack 02.mp3\n"));
  EXPECT_FALSE(parser.TryMagic("Track 1 - Intro.mp3\n"));
  EXPECT_FALSE(parser.TryMagic("Track 01 Intro.flac\n"));
  EXPECT_FALSE(parser.TryMagic("file:///home/user/Music/song.mp3\n"));
  EXPECT_FALSE(parser.TryMagic("File 01.mp3\n"));
  EXPECT_FALSE(parser.TryMagic("Performer - Song.flac\n"));
  EXPECT_FALSE(parser.TryMagic("/music/Title Track.ogg\nC:\\Music\\Track 01.mp3\n"));
  // The start of a longer list, which can be cut off anywhere.
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nTrack 01"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nTitle Fight/So"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nfile:///music/so"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nTrack 01 I"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nTrack 01 Intro"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nFile 01 Intro"));
  EXPECT_FALSE(parser.TryMagic("Tracks/01.flac\nFile 01 Introduction"));
  EXPECT_FALSE(parser.TryMagic("File 01 Introduction\nTracks/01.flac\n"));
  EXPECT_FALSE(parser.TryMagic("Track 01 Intro\nTracks/01.flac\n"));

}

// The media files of a CUE sheet can be found without loading the songs, data files and media files without tracks are not included.
TEST_F(CueParserTest, MediaFiles) {

  QByteArray cue;
  cue += "TITLE \"Album\"\r\n";
  cue += "FILE \"CD1.flac\" WAVE\r\n";
  cue += "  TRACK 01 AUDIO\r\n";
  cue += "    INDEX 01 00:00:00\r\n";
  cue += "FILE \"data.bin\" BINARY\r\n";
  cue += "  TRACK 02 MODE1/2352\r\n";
  cue += "    INDEX 01 00:00:00\r\n";
  cue += "FILE \"/music/CD2.flac\" WAVE\r\n";
  cue += "  TRACK 03 AUDIO\r\n";
  cue += "    INDEX 01 00:00:00\r\n";
  // Media files without a track, or without an index, can't have songs, so they are not included.
  cue += "FILE \"no-tracks.flac\" WAVE\r\n";
  cue += "FILE \"no-index.flac\" WAVE\r\n";
  cue += "  TRACK 04 AUDIO\r\n";
  cue += "FILE \"index-without-position.flac\" WAVE\r\n";
  cue += "  TRACK 05 AUDIO\r\n";
  cue += "    INDEX 01\r\n";
  cue += "FILE \"invalid-index.flac\" WAVE\r\n";
  cue += "  TRACK 06 AUDIO\r\n";
  cue += "    INDEX 01 invalid\r\n";
  // The "01" index is used, even when an earlier index is valid.
  cue += "FILE \"invalid-index-01.flac\" WAVE\r\n";
  cue += "  TRACK 07 AUDIO\r\n";
  cue += "    INDEX 00 00:00:00\r\n";
  cue += "    INDEX 01 invalid\r\n";
  // The track before the last track of a media file also needs the index of the next track as its end, so neither track is valid.
  cue += "FILE \"invalid-next-index.flac\" WAVE\r\n";
  cue += "  TRACK 09 AUDIO\r\n";
  cue += "    INDEX 01 00:00:00\r\n";
  cue += "  TRACK 10 AUDIO\r\n";
  cue += "    INDEX 01 invalid\r\n";
  // Without a "01" index, the first index is used.
  cue += "FILE \"index-00.flac\" WAVE\r\n";
  cue += "  TRACK 08 AUDIO\r\n";
  cue += "    INDEX 00 00:00:00\r\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const QStringList media_files = parser.MediaFiles(&buffer, QDir(u"/albums/album"_s));

  EXPECT_EQ(QStringList() << u"/albums/album/CD1.flac"_s << u"/music/CD2.flac"_s << u"/albums/album/index-00.flac"_s, media_files);

}

// Regression test: tracks of a media file that doesn't exist used to be returned as valid songs without a URL.
TEST_F(CueParserTest, TracksWithMissingMediaFileAreSkipped) {

  TemporaryResource audio(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(QFile::exists(audio.fileName()));
  const QFileInfo audio_info(audio.fileName());

  QByteArray cue;
  cue += "TITLE \"Album\"\n";
  cue += "FILE \"does-not-exist.flac\" WAVE\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"Missing\"\n";
  cue += "    INDEX 01 00:00:00\n";
  cue += "FILE \"" + audio_info.fileName().toUtf8() + "\" MP3\n";
  cue += "  TRACK 02 AUDIO\n";
  cue += "    TITLE \"Existing\"\n";
  cue += "    INDEX 01 00:00:00\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const ParserBase::LoadResult result = parser.Load(&buffer, audio_info.dir().filePath(u"album.cue"_s), audio_info.dir(), false);

  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(u"Existing"_s, result.songs[0].title());
  EXPECT_EQ(QUrl::fromLocalFile(audio_info.absoluteFilePath()), result.songs[0].url());

}

// Regression test: a track before any FILE line has no media file, it used to be returned as a valid song without a URL.
TEST_F(CueParserTest, TrackWithoutFileIsSkipped) {

  TemporaryResource audio(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(QFile::exists(audio.fileName()));
  const QFileInfo audio_info(audio.fileName());

  QByteArray cue;
  cue += "TITLE \"Album\"\n";
  cue += "  TRACK 01 AUDIO\n";
  cue += "    TITLE \"No file\"\n";
  cue += "    INDEX 01 00:00:00\n";

  QBuffer buffer(&cue);
  ASSERT_TRUE(buffer.open(QIODevice::ReadOnly));

  CueParser parser = MakeParser();
  const ParserBase::LoadResult result = parser.Load(&buffer, audio_info.dir().filePath(u"album.cue"_s), audio_info.dir(), false);

  EXPECT_TRUE(result.songs.isEmpty());

}

}  // namespace
