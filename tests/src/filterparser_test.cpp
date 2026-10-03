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

}  // namespace
