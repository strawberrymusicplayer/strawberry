/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QRegularExpression>
#include <QtDebug>

#include "test_utils.h"
#include "utilities/envutils.h"
#include "utilities/strutils.h"
#include "utilities/timeutils.h"
#include "utilities/randutils.h"
#include "utilities/cryptutils.h"
#include "utilities/colorutils.h"
#include "utilities/transliterate.h"
#include "core/logging.h"
#include "core/temporaryfile.h"

using namespace Qt::Literals::StringLiterals;

TEST(UtilitiesTest, PrettyTimeDelta) {

  ASSERT_EQ(Utilities::PrettyTimeDelta(60), u"+1:00"_s);

  ASSERT_EQ(Utilities::PrettyTimeDelta(3600), u"+1:00:00"_s);

  ASSERT_EQ(Utilities::PrettyTimeDelta(9600), u"+2:40:00"_s);

}

TEST(UtilitiesTest, PrettyTime) {

  ASSERT_EQ(Utilities::PrettyTime(60), u"1:00"_s);

  ASSERT_EQ(Utilities::PrettyTime(3600), u"1:00:00"_s);

  ASSERT_EQ(Utilities::PrettyTime(9600), u"2:40:00"_s);

}

TEST(UtilitiesTest, PrettyTimeNanosec) {}

TEST(UtilitiesTest, WordyTime) {

  ASSERT_EQ(Utilities::WordyTime(787200), u"9 days 2:40:00"_s);

}

TEST(UtilitiesTest, WordyTimeNanosec) {}

TEST(UtilitiesTest, Ago) {

  ASSERT_EQ(Utilities::Ago(QDateTime::currentSecsSinceEpoch() - 604800, QLocale()), u"7 days ago"_s);

}

TEST(UtilitiesTest, PrettyFutureDate) {}

TEST(UtilitiesTest, PrettySize) {

  ASSERT_EQ(Utilities::PrettySize(787200), u"787.2 KB"_s);

}

TEST(UtilitiesTest, ColorToRgba) {

  ASSERT_EQ(Utilities::ColorToRgba(QColor(33, 22, 128)), u"rgba(33, 22, 128, 255)"_s);

}

TEST(UtilitiesTest, HmacFunctions) {

  QString key(u"key"_s);
  QString data(u"The quick brown fox jumps over the lazy dog"_s);

  // Test Hmac MD5
  QString result_hash_md5 = QString::fromLatin1(Utilities::HmacMd5(key.toLocal8Bit(), data.toLocal8Bit()).toHex());
  bool result_md5 = result_hash_md5 == u"80070713463e7749b90c2dc24911e275"_s;
  EXPECT_TRUE(result_md5);

  // Test Hmac SHA256
  QString result_hash_sha256 = QString::fromLatin1(Utilities::HmacSha256(key.toLocal8Bit(), data.toLocal8Bit()).toHex());
  bool result_sha256 = result_hash_sha256 == u"f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8"_s;
  EXPECT_TRUE(result_sha256);

}

TEST(UtilitiesTest, PrettySize2) {

  ASSERT_EQ(Utilities::PrettySize(QSize(22, 32)), u"22x32"_s);

}

TEST(UtilitiesTest, ParseRFC822DateTime) {

  QDateTime result_DateTime = Utilities::ParseRFC822DateTime(u"22 Feb 2008 00:16:17 GMT"_s);
  EXPECT_TRUE(result_DateTime.isValid());

  result_DateTime = Utilities::ParseRFC822DateTime(u"Thu, 13 Dec 2012 13:27:52 +0000"_s);
  EXPECT_TRUE(result_DateTime.isValid());

  result_DateTime = Utilities::ParseRFC822DateTime(u"Mon, 12 March 2012 20:00:00 +0100"_s);
  EXPECT_TRUE(result_DateTime.isValid());

}

TEST(UtilitiesTest, DecodeHtmlEntities) {

  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&amp;"_s), u"&"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&#38;"_s), u"&"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&quot;"_s), u"\""_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&#34;"_s), u"\""_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&apos;"_s), u"'"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&#39;"_s), u"'"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&lt;"_s), u"<"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&#60;"_s), u"<"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&gt;"_s), u">"_s);
  ASSERT_EQ(Utilities::DecodeHtmlEntities(u"&#62;"_s), u">"_s);

}

TEST(UtilitiesTest, PathWithoutFilenameExtension) {

  ASSERT_EQ(Utilities::PathWithoutFilenameExtension(u"/home/jonas/test/filename.txt"_s), u"/home/jonas/test/filename"_s);

}

TEST(UtilitiesTest, FiddleFileExtension) {

  ASSERT_EQ(Utilities::FiddleFileExtension(u"/home/jonas/test/filename.txt"_s, u"db"_s), u"/home/jonas/test/filename.db"_s);

}

TEST(UtilitiesTest, SetEnvGetEnv) {

  QString var = u"STRAWBERRY_UNIT_TEST_"_s + Utilities::GetRandomStringWithCharsAndNumbers(20);
  QString value = u"STRAWBERRY_UNIT_TEST_"_s + Utilities::GetRandomStringWithCharsAndNumbers(20);

  Utilities::SetEnv(var.toUtf8().constData(), value);
  ASSERT_EQ(Utilities::GetEnv(var), value);
  Utilities::SetEnv(var.toUtf8().constData(), ""_L1);

}

TEST(UtilitiesTest, Random) {

  EXPECT_FALSE(Utilities::GetRandomStringWithChars(20) == Utilities::GetRandomStringWithChars(20));

  EXPECT_FALSE(Utilities::GetRandomStringWithCharsAndNumbers(20) == Utilities::GetRandomStringWithCharsAndNumbers(20));

  EXPECT_FALSE(Utilities::CryptographicRandomString(20) == Utilities::CryptographicRandomString(20));

  EXPECT_FALSE(Utilities::GetRandomString(20, u"&%XVBGQ"_s) == Utilities::GetRandomString(20, u"&%XVBGQ"_s));

}

TEST(UtilitiesTest, Transliterate) {

  ASSERT_EQ(Utilities::Transliterate(u"ÆØÅ"_s), u"AEOA"_s);

}

TEST(UtilitiesTest, ReplaceVariable) {

  Song song;
  song.set_title(Utilities::GetRandomStringWithChars(8));
  song.set_titlesort(Utilities::GetRandomStringWithChars(8));
  song.set_album(Utilities::GetRandomStringWithChars(8));
  song.set_albumsort(Utilities::GetRandomStringWithChars(8));
  song.set_artist(Utilities::GetRandomStringWithChars(8));
  song.set_artistsort(Utilities::GetRandomStringWithChars(8));
  song.set_albumartist(Utilities::GetRandomStringWithChars(8));
  song.set_albumartistsort(Utilities::GetRandomStringWithChars(8));
  song.set_track(5);
  song.set_disc(2);
  song.set_year(1999);
  song.set_originalyear(2000);
  song.set_genre(Utilities::GetRandomStringWithChars(8));
  song.set_composer(Utilities::GetRandomStringWithChars(8));
  song.set_composersort(Utilities::GetRandomStringWithChars(8));
  song.set_performer(Utilities::GetRandomStringWithChars(8));
  song.set_performersort(Utilities::GetRandomStringWithChars(8));
  song.set_grouping(Utilities::GetRandomStringWithChars(8));
  song.set_length_nanosec(900000000000);
  song.set_url(QUrl(u"file:///home/jonas/Music/test_song.flac"_s));
  song.set_skipcount(20);
  song.set_playcount(90);
  song.set_rating(1.0);

  ASSERT_EQ(Utilities::ReplaceVariable(u"%title%"_s, song, ""_L1), song.title());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%titlesort%"_s, song, ""_L1), song.titlesort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%album%"_s, song, ""_L1), song.album());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%albumsort%"_s, song, ""_L1), song.albumsort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%artist%"_s, song, ""_L1), song.artist());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%artistsort%"_s, song, ""_L1), song.artistsort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%albumartist%"_s, song, ""_L1), song.effective_albumartist());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%albumartistsort%"_s, song, ""_L1), song.albumartistsort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%track%"_s, song, ""_L1), QString::number(song.track()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%disc%"_s, song, ""_L1), QString::number(song.disc()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%year%"_s, song, ""_L1), QString::number(song.year()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%originalyear%"_s, song, ""_L1), QString::number(song.originalyear()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%genre%"_s, song, ""_L1), song.genre());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%composer%"_s, song, ""_L1), song.composer());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%composersort%"_s, song, ""_L1), song.composersort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%performer%"_s, song, ""_L1), song.performer());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%performersort%"_s, song, ""_L1), song.performersort());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%grouping%"_s, song, ""_L1), song.grouping());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%length%"_s, song, ""_L1), song.PrettyLength());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%filename%"_s, song, ""_L1), song.basefilename());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%url%"_s, song, ""_L1), song.url().toString());
  ASSERT_EQ(Utilities::ReplaceVariable(u"%playcount%"_s, song, ""_L1), QString::number(song.playcount()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%skipcount%"_s, song, ""_L1), QString::number(song.skipcount()));
  ASSERT_EQ(Utilities::ReplaceVariable(u"%rating%"_s, song, ""_L1), song.PrettyRating());

}

TEST(UtilitiesTest, ReplaceMessage) {

  Song song;
  song.set_title(Utilities::GetRandomStringWithChars(8));
  song.set_album(Utilities::GetRandomStringWithChars(8));
  song.set_artist(Utilities::GetRandomStringWithChars(8));
  song.set_artistsort(Utilities::GetRandomStringWithChars(8));
  song.set_albumartist(Utilities::GetRandomStringWithChars(8));
  song.set_albumartistsort(Utilities::GetRandomStringWithChars(8));
  song.set_track(5);
  song.set_disc(2);
  song.set_year(1999);
  song.set_originalyear(2000);
  song.set_genre(Utilities::GetRandomStringWithChars(8));
  song.set_composer(Utilities::GetRandomStringWithChars(8));
  song.set_performer(Utilities::GetRandomStringWithChars(8));
  song.set_grouping(Utilities::GetRandomStringWithChars(8));
  song.set_length_nanosec(900000000000);
  song.set_url(QUrl(u"file:///home/jonas/Music/test_song.flac"_s));
  song.set_skipcount(20);
  song.set_playcount(90);
  song.set_rating(1.0);

  ASSERT_EQ(Utilities::ReplaceMessage(u"%title% - %artist%"_s, song, ""_L1), song.title() + u" - "_s + song.artist());
  ASSERT_EQ(Utilities::ReplaceMessage(u"%artistsort% - %albumartistsort%"_s, song, ""_L1), song.artistsort() + u" - "_s + song.albumartistsort());

}

TEST(UtilitiesTest, TemporaryFile) {

  QString filename_pattern = u"/tmp/test-XXXX.jpg"_s;

  TemporaryFile temp_file(filename_pattern);

  EXPECT_FALSE(temp_file.filename().isEmpty());

  EXPECT_FALSE(temp_file.filename() == filename_pattern);

  static const QRegularExpression regex_temp_filename(u"^\\/tmp\\/test-....\\.jpg$"_s);

  EXPECT_TRUE(regex_temp_filename.match(temp_file.filename()).hasMatch());

}
