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

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QString>
#include <QTemporaryDir>

#include "core/song.h"
#include "playlistparsers/xspfparser.h"
#include "playlistparsers/parserbase.h"
#include "playlistparsers/playlistparser.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

TEST(XSPFParserTest, SaveWithExplicitFormatWithoutFilenameExtension) {

  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  PlaylistParser parser(nullptr);
  QSignalSpy errors(&parser, &PlaylistParser::Error);
  const QString filename = directory.filePath(u"document-123"_s);
  parser.Save(u"My Playlist"_s, SongList(), filename, PlaylistSettings::PathType::Absolute, u"xspf"_s);
  EXPECT_EQ(0, errors.count());
  QFile file(filename);
  ASSERT_TRUE(file.open(QIODevice::ReadOnly));
  EXPECT_TRUE(file.readAll().contains("<title>My Playlist</title>"));

}

TEST(XSPFParserTest, SaveRejectsUnknownExplicitFormat) {

  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  PlaylistParser parser(nullptr);
  QSignalSpy errors(&parser, &PlaylistParser::Error);
  const QString filename = directory.filePath(u"playlist.xspf"_s);
  parser.Save(u"My Playlist"_s, SongList(), filename, PlaylistSettings::PathType::Absolute, u"unsupported"_s);
  EXPECT_EQ(1, errors.count());
  EXPECT_FALSE(QFile::exists(filename));

}

ParserBase::LoadResult LoadXSPF(QByteArray data) {

  XSPFParser parser(nullptr, nullptr);
  QBuffer buffer(&data);
  if (!buffer.open(QIODevice::ReadOnly)) return ParserBase::LoadResult();
  return parser.Load(&buffer, QString(), QDir::temp(), false);

}

TEST(XSPFParserTest, PlaylistTitle) {

  const QByteArray data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n"
                          "  <title>My Playlist</title>\n"
                          "  <trackList>\n"
                          "    <track><location>http://example.com/one.mp3</location><title>Track One</title></track>\n"
                          "  </trackList>\n"
                          "</playlist>\n";

  const ParserBase::LoadResult result = LoadXSPF(data);
  EXPECT_EQ(u"My Playlist"_s, result.playlist_name);
  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(u"Track One"_s, result.songs[0].title());

}

// A playlist without a title has no playlist name, it used to get the title of the first track.
TEST(XSPFParserTest, PlaylistWithoutTitleHasNoName) {

  const QByteArray data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                          "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">\n"
                          "  <creator>Someone</creator>\n"
                          "  <trackList>\n"
                          "    <track><location>http://example.com/one.mp3</location><title>Track One</title></track>\n"
                          "    <track><location>http://example.com/two.mp3</location><title>Track Two</title></track>\n"
                          "  </trackList>\n"
                          "</playlist>\n";

  const ParserBase::LoadResult result = LoadXSPF(data);
  EXPECT_TRUE(result.playlist_name.isEmpty());
  ASSERT_EQ(2, result.songs.size());
  EXPECT_EQ(u"Track One"_s, result.songs[0].title());
  EXPECT_EQ(u"Track Two"_s, result.songs[1].title());

}

// The title can be after the track list.
TEST(XSPFParserTest, PlaylistTitleAfterTrackList) {

  const QByteArray data = "<playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\">"
                          "<trackList><track><location>http://example.com/one.mp3</location><title>Track One</title></track></trackList>"
                          "<title>My Playlist</title>"
                          "</playlist>";

  const ParserBase::LoadResult result = LoadXSPF(data);
  EXPECT_EQ(u"My Playlist"_s, result.playlist_name);
  ASSERT_EQ(1, result.songs.size());

}

}  // namespace
