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

#include "lyrics/letraslyricsprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// The track name is decoded as a JSON string, the page can escape characters in it.
TEST(LetrasLyricsProviderTest, PageTrackNameIsDecoded) {

  EXPECT_EQ(u"Águas de Março (part. Tom Jobim)"_s, LetrasLyricsProvider::PageTrackName(u"window.__pageArgs={\"album_name\":\"Águas De Março\",\"track_name\":\"Águas de Março (part. Tom Jobim)\",\"url\":\"aguas-de-marco-part-tom-jobim\"}"_s));
  EXPECT_EQ(u"Café \"Live\" / Mix"_s, LetrasLyricsProvider::PageTrackName(uR"({"track_name":"Café \"Live\" \/ Mix","url":"cafe"})"_s));
  EXPECT_TRUE(LetrasLyricsProvider::PageTrackName(u"<html></html>"_s).isNull());

}

// Regression test: the requested title was compared with the title of the page as it is, so a title with different accents or a different Unicode normalization than on Letras was taken as a different track.
TEST(LetrasLyricsProviderTest, TitleMatchesAccentedTitle) {

  const QString page_title = u"Águas de Março (part. Tom Jobim)"_s;
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Águas de Março"_s, page_title));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"águas de março"_s, page_title));
  // Decomposed, like tags written on macOS.
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Águas de Março"_s.normalized(QString::NormalizationForm_D), page_title));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Aguas de Marco"_s, page_title));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Café"_s, LetrasLyricsProvider::PageTrackName(uR"({"track_name":"Café"})"_s)));

}

// A different track the slug resolved to doesn't match, the qualifiers Letras appends to the title do.
TEST(LetrasLyricsProviderTest, TitleMatchesOnlySameTrack) {

  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Biding My Time"_s));
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Timeless"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live)"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Minor Swing"_s, u"Minor Swing (feat. Stéphane Grappelli)"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live) [Remastered]"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time (Live)"_s, u"Time (Live)"_s));
  // Qualifiers with brackets in them.
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live (1974))"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live [Remastered])"_s));
  EXPECT_TRUE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live (1974)) [Remastered]"_s));
  // Without a complete qualifier at the end, nothing is removed.
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time Live (1974"_s));
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time Live 1974)"_s));
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time (Live) Session"_s));
  // Only the qualifiers in brackets are left out, not other words.
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Love?"_s, u"Love Story"_s));
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time After Time"_s));
  EXPECT_FALSE(LetrasLyricsProvider::TitleMatches(u"Time"_s, u"Time After Time (Live)"_s));

}

}  // namespace
