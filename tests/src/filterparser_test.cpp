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

#include <QString>

#include "includes/scoped_ptr.h"
#include "core/song.h"
#include "filterparser/filterparser.h"
#include "filterparser/filtertree.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

Song SongWithTitle(const QString &title) {

  Song song;
  song.set_title(title);
  return song;

}

bool Accepts(const QString &filter, const Song &song) {

  const ScopedPtr<FilterTree> tree(FilterParser(filter).parse());
  return tree->accept(song);

}

TEST(FilterParserTest, QuoteValueEscapesBackslashesAndQuotes) {

  EXPECT_EQ(u"\"Title\""_s, FilterParser::QuoteValue(u"Title"_s));
  EXPECT_EQ(u"\"12\\\" Mix\""_s, FilterParser::QuoteValue(u"12\" Mix"_s));
  EXPECT_EQ(u"\"AC\\\\DC\""_s, FilterParser::QuoteValue(u"AC\\DC"_s));

}

TEST(FilterParserTest, QuotedValueWithEscapedQuote) {

  // title:"12\" Mix"
  EXPECT_TRUE(Accepts(u"title:\"12\\\" Mix\""_s, SongWithTitle(u"12\" Mix"_s)));
  EXPECT_FALSE(Accepts(u"title:\"12\\\" Mix\""_s, SongWithTitle(u"12 Mix"_s)));

}

TEST(FilterParserTest, QuotedValueWithEscapedBackslash) {

  // title:"AC\\DC"
  EXPECT_TRUE(Accepts(u"title:\"AC\\\\DC\""_s, SongWithTitle(u"AC\\DC"_s)));

}

TEST(FilterParserTest, QuotedValueKeepsUnescapedBackslash) {

  // title:"AC\DC", a backslash that doesn't escape a quote or backslash is kept as it is.
  EXPECT_TRUE(Accepts(u"title:\"AC\\DC\""_s, SongWithTitle(u"AC\\DC"_s)));

}

TEST(FilterParserTest, QuoteValueRoundTrip) {

  const QString title = u"A \"quoted\" \\ title\\"_s;
  EXPECT_TRUE(Accepts(u"title:"_s + FilterParser::QuoteValue(title), SongWithTitle(title)));
  EXPECT_FALSE(Accepts(u"title:"_s + FilterParser::QuoteValue(title), SongWithTitle(u"A quoted title"_s)));

}

TEST(FilterParserTest, QuotedValueWithoutEscapes) {

  EXPECT_TRUE(Accepts(u"title:\"foo bar\""_s, SongWithTitle(u"Foo Bar"_s)));
  EXPECT_FALSE(Accepts(u"title:\"foo bar\""_s, SongWithTitle(u"Foo"_s)));

}

Song SongWithArtist(const QString &artist) {
  Song song;
  song.set_artist(artist);
  return song;
}

TEST(FilterParserTest, HyphenInsideTermIsPartOfValue) {
  EXPECT_TRUE(Accepts(u"blink-182"_s, SongWithArtist(u"blink-182"_s)));
  EXPECT_TRUE(Accepts(u"artist:blink-182"_s, SongWithArtist(u"blink-182"_s)));
  EXPECT_FALSE(Accepts(u"blink-182"_s, SongWithArtist(u"blink"_s)));
  EXPECT_FALSE(Accepts(u"artist:blink-182"_s, SongWithArtist(u"blink"_s)));
}

TEST(FilterParserTest, HyphenAtStartOfTermNegates) {
  EXPECT_TRUE(Accepts(u"-foo"_s, SongWithTitle(u"Bar"_s)));
  EXPECT_FALSE(Accepts(u"-foo"_s, SongWithTitle(u"Foo"_s)));
  EXPECT_TRUE(Accepts(u"bar -foo"_s, SongWithTitle(u"Bar"_s)));
  EXPECT_FALSE(Accepts(u"bar -foo"_s, SongWithTitle(u"Bar Foo"_s)));
  EXPECT_FALSE(Accepts(u"bar (-foo)"_s, SongWithTitle(u"Bar Foo"_s)));
  EXPECT_FALSE(Accepts(u"bar OR-foo"_s, SongWithTitle(u"Foo"_s)));
  EXPECT_TRUE(Accepts(u"bar OR-foo"_s, SongWithTitle(u"Baz"_s)));
}

Song SongWithYears(const int year, const int originalyear) {
  Song song;
  song.set_year(year);
  song.set_originalyear(originalyear);
  return song;
}

TEST(FilterParserTest, OriginalYear) {
  EXPECT_TRUE(Accepts(u"originalyear:1971"_s, SongWithYears(2011, 1971)));
  EXPECT_FALSE(Accepts(u"originalyear:2011"_s, SongWithYears(2011, 1971)));
  EXPECT_FALSE(Accepts(u"year:1971"_s, SongWithYears(2011, 1971)));
  // Without an original year, it's the year.
  EXPECT_TRUE(Accepts(u"originalyear:2011"_s, SongWithYears(2011, -1)));
}

TEST(FilterParserTest, UnknownYear) {
  // "Search for this" uses this for the songs without a year, which have year -1 or 0.
  EXPECT_TRUE(Accepts(u"year:<=0"_s, SongWithYears(-1, -1)));
  EXPECT_TRUE(Accepts(u"year:<=0"_s, SongWithYears(0, -1)));
  EXPECT_FALSE(Accepts(u"year:<=0"_s, SongWithYears(2011, -1)));
  EXPECT_TRUE(Accepts(u"originalyear:<=0"_s, SongWithYears(-1, -1)));
  EXPECT_FALSE(Accepts(u"originalyear:<=0"_s, SongWithYears(-1, 1971)));
}

}  // namespace
