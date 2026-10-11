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
#include <QFileInfo>
#include <QString>
#include <QThread>

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
  ASSERT_TRUE(audio.isOpen());
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

}  // namespace
