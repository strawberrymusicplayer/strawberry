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

#include "gtest_include.h"

#include <QString>
#include <QRegularExpression>

#include "lyrics/htmllyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

class HtmlLyricsProviderTest : public ::testing::Test {
 protected:
  static QString Parse(const QString &content, const bool multiple) {
    // The same tags as most of the HTML lyrics providers use.
    static const QRegularExpression start_tag(u"<div[^>]*>"_s);
    static const QRegularExpression end_tag(u"</div>"_s);
    static const QRegularExpression lyrics_start(u"<div class=\"lyrics\">"_s);
    return HtmlLyricsProvider::ParseLyricsFromHTML(content, start_tag, end_tag, lyrics_start, multiple);
  }
};

TEST_F(HtmlLyricsProviderTest, ParsesNestedTags) {

  EXPECT_EQ(u"Line 1\nLine 2"_s, Parse(u"<html><div class=\"lyrics\">Line 1<br><div>Line 2</div></div></html>"_s, false));

}

TEST_F(HtmlLyricsProviderTest, ParsesMultipleBlocks) {

  EXPECT_EQ(u"One\nTwo"_s, Parse(u"<div class=\"lyrics\">One</div><p>Ad</p><div class=\"lyrics\">Two</div>"_s, true));

}

TEST_F(HtmlLyricsProviderTest, UnclosedStartTagAfterLyrics) {

  // A start tag after the lyrics with no end tag left used to loop forever.
  EXPECT_TRUE(Parse(u"<div class=\"lyrics\">Line 1<div>Line 2"_s, false).isEmpty());
  EXPECT_TRUE(Parse(u"<div class=\"lyrics\">Line 1<div>Line 2"_s, true).isEmpty());

}

TEST_F(HtmlLyricsProviderTest, UnclosedSecondBlock) {

  // An unclosed second block used to be searched again forever when parsing multiple blocks.
  EXPECT_EQ(u"One"_s, Parse(u"<div class=\"lyrics\">One</div><div class=\"lyrics\">Two"_s, true));
  EXPECT_EQ(u"One"_s, Parse(u"<div class=\"lyrics\">One</div><div class=\"lyrics\">Two<div>Three"_s, true));

}

}  // namespace
