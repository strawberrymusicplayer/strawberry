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
#include "constants/timeconstants.h"
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

// Regression test: a word with a colon was taken as a column and the value, and when the column was unknown, only the value was searched for, so "12:30" found every song with "30".
TEST(FilterParserTest, UnknownColumnIsPartOfValue) {
  EXPECT_TRUE(Accepts(u"12:30"_s, SongWithTitle(u"Live at 12:30"_s)));
  EXPECT_FALSE(Accepts(u"12:30"_s, SongWithTitle(u"30 Seconds"_s)));
  EXPECT_TRUE(Accepts(u"re:zero"_s, SongWithTitle(u"Re:Zero Theme"_s)));
  EXPECT_FALSE(Accepts(u"re:zero"_s, SongWithTitle(u"Zero"_s)));
  // With a space after the colon, the words are searched for, like other words.
  EXPECT_TRUE(Accepts(u"re: zero"_s, SongWithTitle(u"Re: Zero"_s)));
  EXPECT_FALSE(Accepts(u"re: zero"_s, SongWithTitle(u"Zero"_s)));
  // A word ending with a colon is searched for too.
  EXPECT_FALSE(Accepts(u"foo:"_s, SongWithTitle(u"Bar"_s)));
  // Known columns are still columns, also in upper case, and with a space after the colon.
  EXPECT_TRUE(Accepts(u"TITLE:zero"_s, SongWithTitle(u"Zero"_s)));
  EXPECT_TRUE(Accepts(u"title: zero"_s, SongWithTitle(u"Zero"_s)));
  EXPECT_FALSE(Accepts(u"artist:zero"_s, SongWithTitle(u"Zero"_s)));
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

Song SongWithLength(const qint64 length_nanosec) {
  Song song;
  song.set_length_nanosec(length_nanosec);
  return song;
}

TEST(FilterParserTest, LengthMatchesShownLength) {

  // 3:45.6 is shown as 3:45.
  const Song song = SongWithLength((225 * kNsecPerSec) + (600 * kNsecPerMsec));
  EXPECT_TRUE(Accepts(u"length:3:45"_s, song));
  EXPECT_TRUE(Accepts(u"length:=225"_s, song));
  EXPECT_FALSE(Accepts(u"length:3:46"_s, song));
  EXPECT_FALSE(Accepts(u"length:!=3:45"_s, song));

}

TEST(FilterParserTest, LengthComparisons) {

  const Song song = SongWithLength((225 * kNsecPerSec) + (600 * kNsecPerMsec));
  EXPECT_TRUE(Accepts(u"length:>=3:45"_s, song));
  EXPECT_TRUE(Accepts(u"length:<=3:45"_s, song));
  EXPECT_FALSE(Accepts(u"length:>3:45"_s, song));
  EXPECT_FALSE(Accepts(u"length:<3:45"_s, song));
  EXPECT_TRUE(Accepts(u"length:<3:46"_s, song));
  EXPECT_TRUE(Accepts(u"length:>3:44"_s, song));
  EXPECT_TRUE(Accepts(u"length:<1:00:00"_s, song));

}

TEST(FilterParserTest, UnknownLength) {

  // An unknown length is -1, which is shown as an empty length, it's not 0.
  const Song song = SongWithLength(-1);
  EXPECT_FALSE(Accepts(u"length:0"_s, song));
  EXPECT_FALSE(Accepts(u"length:0:00"_s, song));
  EXPECT_TRUE(Accepts(u"length:<0"_s, song));
  EXPECT_TRUE(Accepts(u"length:0"_s, SongWithLength(500 * kNsecPerMsec)));

}

}  // namespace
