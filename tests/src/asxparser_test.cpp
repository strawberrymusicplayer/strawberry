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
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "playlistparsers/asxparser.h"
#include "playlistparsers/parserbase.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

ParserBase::LoadResult LoadASX(QByteArray data) {

  ASXParser parser(nullptr, nullptr);
  QBuffer buffer(&data);
  if (!buffer.open(QIODevice::ReadOnly)) return ParserBase::LoadResult();
  return parser.Load(&buffer, QString(), QDir::temp(), false);

}

// Unescaped "&" in URLs are escaped, also after and in UTF-8 characters, which used to put the offsets out of step.
TEST(ASXParserTest, EscapesAmpersandsWithUTF8) {

  // Ñandú (UTF-8) and ñandú in the URL.
  const QByteArray data = "<asx version=\"3.0\">\n"
                          "<entry><title>\xc3\x91" "and\xc3\xba</title><ref href=\"http://example.com/\xc3\xb1" "and\xc3\xba?a=1&b=2\"/></entry>\n"
                          "<entry><title>Second</title><ref href=\"http://example.com/second?c=3&d=4\"/></entry>\n"
                          "</asx>\n";

  const ParserBase::LoadResult result = LoadASX(data);
  ASSERT_EQ(2, result.songs.size());
  EXPECT_EQ(u"Ñandú"_s, result.songs[0].title());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/ñandú?a=1&b=2"_s), result.songs[0].url());
  EXPECT_EQ(u"Second"_s, result.songs[1].title());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/second?c=3&d=4"_s), result.songs[1].url());

}

// Ampersands that are already escaped are kept.
TEST(ASXParserTest, KeepsEscapedAmpersands) {

  const QByteArray data = "<asx version=\"3.0\"><entry><title>One</title><ref href=\"http://example.com/one?a=1&amp;b=2\"/></entry></asx>";

  const ParserBase::LoadResult result = LoadASX(data);
  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/one?a=1&b=2"_s), result.songs[0].url());

}

// Numeric character references are kept, they used to be escaped, so the URL got the reference text instead of the character.
TEST(ASXParserTest, KeepsNumericCharacterReferences) {

  const QByteArray data = "<asx version=\"3.0\">"
                          "<entry><title>Decimal</title><ref href=\"http://example.com/decimal?a=1&#38;b=2\"/></entry>"
                          "<entry><title>Hexadecimal</title><ref href=\"http://example.com/hexadecimal?a=1&#x26;b=2\"/></entry>"
                          "</asx>";

  const ParserBase::LoadResult result = LoadASX(data);
  ASSERT_EQ(2, result.songs.size());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/decimal?a=1&b=2"_s), result.songs[0].url());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/hexadecimal?a=1&b=2"_s), result.songs[1].url());

}

// A playlist in another encoding is kept as it is for the XML reader, the "&" in a URL with a Latin-1 character used to stay unescaped, so the playlist failed to load.
TEST(ASXParserTest, EscapesAmpersandsWithLatin1) {

  // ñandú in ISO-8859-1.
  const QByteArray data = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
                          "<asx version=\"3.0\"><entry><title>\xf1" "and\xfa</title><ref href=\"http://example.com/\xf1?a=1&b=2\"/></entry></asx>";

  const ParserBase::LoadResult result = LoadASX(data);
  ASSERT_EQ(1, result.songs.size());
  EXPECT_EQ(u"ñandú"_s, result.songs[0].title());
  EXPECT_EQ(QUrl::fromUserInput(u"http://example.com/ñ?a=1&b=2"_s), result.songs[0].url());

}

}  // namespace
