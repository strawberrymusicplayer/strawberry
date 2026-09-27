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

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QThread>
#include <QUrl>

#include "core/song.h"
#include "constants/timeconstants.h"
#include "includes/shared_ptr.h"
#include "playlistparsers/plsparser.h"
#include "playlistparsers/parserbase.h"
#include "tagreader/tagreaderclient.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// Writes raw data to a file.
bool WriteFile(const QString &path, const QByteArray &data) {
  QFile f(path);
  if (!f.open(QIODevice::WriteOnly)) return false;
  return f.write(data) == data.length();
}

class PLSParserTest : public ::testing::Test {
 protected:
  ~PLSParserTest() override {
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

  // Constructs a parser with the test TagReaderClient and no collection backend.
  // The shared_ptr uses a no-op deleter because tagreader_client_ lifetime is managed by the fixture (deleteLater in the destructor).
  PLSParser MakeParser() const {
    return PLSParser(SharedPtr<TagReaderClient>(tagreader_client_, [](TagReaderClient *tagreader_client) { Q_UNUSED(tagreader_client) }), nullptr);
  }

  // Loads a playlist file via PLSParser::Load, mirroring the real SongLoader call site.
  static ParserBase::LoadResult Load(PLSParser &parser, const QString &playlist_path) {
    QFile file(playlist_path);
    if (!file.open(QIODevice::ReadOnly)) return ParserBase::LoadResult();
    return parser.Load(&file, playlist_path, QFileInfo(playlist_path).dir(), /*collection_lookup=*/false);
  }

  QThread *tagreader_client_thread_ = nullptr;
  TagReaderClient *tagreader_client_ = nullptr;
};

// PLSParser loads the entries in order with their titles and lengths.
TEST_F(PLSParserTest, LoadsEntriesWithTitleAndLength) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf1(u":/audio/strawberry.mp3"_s);
  TemporaryResource leaf2(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf1.isOpen());
  ASSERT_TRUE(leaf2.isOpen());

  const QString playlist = tmp.filePath(u"playlist.pls"_s);
  ASSERT_TRUE(WriteFile(playlist, u"[playlist]\r\nFile1=%1\r\nTitle1=First\r\nLength1=180\r\nFile2=%2\r\nTitle2=Second\r\nLength2=240\r\nNumberOfEntries=2\r\nVersion=2\r\n"_s.arg(leaf1.fileName(), leaf2.fileName()).toUtf8()));

  PLSParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, playlist);
  ASSERT_EQ(result.songs.size(), 2);
  EXPECT_EQ(result.songs[0].url(), QUrl::fromLocalFile(leaf1.fileName()));
  EXPECT_EQ(result.songs[0].title(), u"First"_s);
  EXPECT_EQ(result.songs[0].length_nanosec(), 180 * kNsecPerSec);
  EXPECT_EQ(result.songs[1].url(), QUrl::fromLocalFile(leaf2.fileName()));
  EXPECT_EQ(result.songs[1].title(), u"Second"_s);
  EXPECT_EQ(result.songs[1].length_nanosec(), 240 * kNsecPerSec);
}

// PLS playlists that are not in UTF-8 are decoded with the detected encoding, here Cyrillic text in windows-1251.
TEST_F(PLSParserTest, Windows1251TitleIsDecoded) {
  QTemporaryDir tmp;
  ASSERT_TRUE(tmp.isValid());

  TemporaryResource leaf(u":/audio/strawberry.mp3"_s);
  ASSERT_TRUE(leaf.isOpen());

  // Title1=Гражданская Оборона - Мы идём
  const QString playlist = tmp.filePath(u"playlist.pls"_s);
  ASSERT_TRUE(WriteFile(playlist, QByteArray("[playlist]\r\nFile1=") + leaf.fileName().toUtf8() + QByteArray("\r\nTitle1=\xc3\xf0\xe0\xe6\xe4\xe0\xed\xf1\xea\xe0\xff \xce\xe1\xee\xf0\xee\xed\xe0 - \xcc\xfb \xe8\xe4\xb8\xec\r\nLength1=180\r\nNumberOfEntries=1\r\n")));

  PLSParser parser = MakeParser();
  const ParserBase::LoadResult result = Load(parser, playlist);
  ASSERT_EQ(result.songs.size(), 1);
  EXPECT_EQ(result.songs[0].title(), u"Гражданская Оборона - Мы идём"_s);
}

}  // namespace
