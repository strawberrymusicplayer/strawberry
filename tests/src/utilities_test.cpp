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

#include <gtest/gtest.h>

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

using namespace Qt::StringLiterals;

TEST(UtilitiesTest, PrettyTimeDelta) {

  ASSERT_EQ(Utilities::PrettyTimeDelta(60), QStringLiteral("+1:00"));

  ASSERT_EQ(Utilities::PrettyTimeDelta(3600), QStringLiteral("+1:00:00"));

  ASSERT_EQ(Utilities::PrettyTimeDelta(9600), QStringLiteral("+2:40:00"));

}

TEST(UtilitiesTest, PrettyTime) {

  ASSERT_EQ(Utilities::PrettyTime(60), QStringLiteral("1:00"));

  ASSERT_EQ(Utilities::PrettyTime(3600), QStringLiteral("1:00:00"));

  ASSERT_EQ(Utilities::PrettyTime(9600), QStringLiteral("2:40:00"));

}

TEST(UtilitiesTest, PrettyTimeNanosec) {}

TEST(UtilitiesTest, WordyTime) {

  ASSERT_EQ(Utilities::WordyTime(787200), QStringLiteral("9 days 2:40:00"));

}

TEST(UtilitiesTest, WordyTimeNanosec) {}

TEST(UtilitiesTest, Ago) {

  ASSERT_EQ(Utilities::Ago(QDateTime::currentSecsSinceEpoch() - 604800, QLocale()), QStringLiteral("7 days ago"));

}

TEST(UtilitiesTest, PrettyFutureDate) {}

TEST(UtilitiesTest, PrettySize) {

  ASSERT_EQ(Utilities::PrettySize(787200), QStringLiteral("787.2 KB"));

}

TEST(UtilitiesTest, ColorToRgba) {

  ASSERT_EQ(Utilities::ColorToRgba(QColor(33, 22, 128)), QStringLiteral("rgba(33, 22, 128, 255)"));

}

TEST(UtilitiesTest, HmacFunctions) {

  QString key(QStringLiteral("key"));
  QString data(QStringLiteral("The quick brown fox jumps over the lazy dog"));

  // Test Hmac MD5
  QString result_hash_md5 = QString::fromLatin1(Utilities::HmacMd5(key.toLocal8Bit(), data.toLocal8Bit()).toHex());
  bool result_md5 = result_hash_md5 == QStringLiteral("80070713463e7749b90c2dc24911e275");
  EXPECT_TRUE(result_md5);

  // Test Hmac SHA256
  QString result_hash_sha256 = QString::fromLatin1(Utilities::HmacSha256(key.toLocal8Bit(), data.toLocal8Bit()).toHex());
  bool result_sha256 = result_hash_sha256 == QStringLiteral("f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
  EXPECT_TRUE(result_sha256);

}

TEST(UtilitiesTest, PrettySize2) {

  ASSERT_EQ(Utilities::PrettySize(QSize(22, 32)), QStringLiteral("22x32"));

}

TEST(UtilitiesTest, ParseRFC822DateTime) {

  QDateTime result_DateTime = Utilities::ParseRFC822DateTime(QStringLiteral("22 Feb 2008 00:16:17 GMT"));
  EXPECT_TRUE(result_DateTime.isValid());

  result_DateTime = Utilities::ParseRFC822DateTime(QStringLiteral("Thu, 13 Dec 2012 13:27:52 +0000"));
  EXPECT_TRUE(result_DateTime.isValid());

  result_DateTime = Utilities::ParseRFC822DateTime(QStringLiteral("Mon, 12 March 2012 20:00:00 +0100"));
  EXPECT_TRUE(result_DateTime.isValid());

}

TEST(UtilitiesTest, DecodeHtmlEntities) {

  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&amp;")), QStringLiteral("&"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&#38;")), QStringLiteral("&"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&quot;")), QStringLiteral("\""));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&#34;")), QStringLiteral("\""));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&apos;")), QStringLiteral("'"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&#39;")), QStringLiteral("'"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&lt;")), QStringLiteral("<"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&#60;")), QStringLiteral("<"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&gt;")), QStringLiteral(">"));
  ASSERT_EQ(Utilities::DecodeHtmlEntities(QStringLiteral("&#62;")), QStringLiteral(">"));

}

TEST(UtilitiesTest, PathWithoutFilenameExtension) {

  ASSERT_EQ(Utilities::PathWithoutFilenameExtension(QStringLiteral("/home/jonas/test/filename.txt")), QStringLiteral("/home/jonas/test/filename"));

}

TEST(UtilitiesTest, FiddleFileExtension) {

  ASSERT_EQ(Utilities::FiddleFileExtension(QStringLiteral("/home/jonas/test/filename.txt"), QStringLiteral("db")), QStringLiteral("/home/jonas/test/filename.db"));

}

TEST(UtilitiesTest, SetEnvGetEnv) {

  QString var = QStringLiteral("STRAWBERRY_UNIT_TEST_") + Utilities::GetRandomStringWithCharsAndNumbers(20);
  QString value = QStringLiteral("STRAWBERRY_UNIT_TEST_") + Utilities::GetRandomStringWithCharsAndNumbers(20);

  Utilities::SetEnv(var.toUtf8().constData(), value);
  ASSERT_EQ(Utilities::GetEnv(var), value);
  Utilities::SetEnv(var.toUtf8().constData(), ""_L1);

}

TEST(UtilitiesTest, Random) {

  EXPECT_FALSE(Utilities::GetRandomStringWithChars(20) == Utilities::GetRandomStringWithChars(20));

  EXPECT_FALSE(Utilities::GetRandomStringWithCharsAndNumbers(20) == Utilities::GetRandomStringWithCharsAndNumbers(20));

  EXPECT_FALSE(Utilities::CryptographicRandomString(20) == Utilities::CryptographicRandomString(20));

  EXPECT_FALSE(Utilities::GetRandomString(20, QStringLiteral("&%XVBGQ")) == Utilities::GetRandomString(20, QStringLiteral("&%XVBGQ")));

}

TEST(UtilitiesTest, Transliterate) {

  ASSERT_EQ(Utilities::Transliterate(QStringLiteral("ÆØÅ")), QStringLiteral("AEOA"));

}

TEST(UtilitiesTest, ReplaceVariable) {

  Song song;
  song.set_title(Utilities::GetRandomStringWithChars(8));
  song.set_album(Utilities::GetRandomStringWithChars(8));
  song.set_artist(Utilities::GetRandomStringWithChars(8));
  song.set_albumartist(Utilities::GetRandomStringWithChars(8));
  song.set_track(5);
  song.set_disc(2);
  song.set_year(1999);
  song.set_originalyear(2000);
  song.set_genre(Utilities::GetRandomStringWithChars(8));
  song.set_composer(Utilities::GetRandomStringWithChars(8));
  song.set_performer(Utilities::GetRandomStringWithChars(8));
  song.set_grouping(Utilities::GetRandomStringWithChars(8));
  song.set_length_nanosec(900000000000);
  song.set_url(QUrl(QStringLiteral("file:///home/jonas/Music/test_song.flac")));
  song.set_skipcount(20);
  song.set_playcount(90);
  song.set_rating(1.0);

  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%title%"), song, ""_L1), song.title());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%album%"), song, ""_L1), song.album());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%artist%"), song, ""_L1), song.artist());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%albumartist%"), song, ""_L1), song.effective_albumartist());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%track%"), song, ""_L1), QString::number(song.track()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%disc%"), song, ""_L1), QString::number(song.disc()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%year%"), song, ""_L1), QString::number(song.year()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%originalyear%"), song, ""_L1), QString::number(song.originalyear()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%genre%"), song, ""_L1), song.genre());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%composer%"), song, ""_L1), song.composer());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%performer%"), song, ""_L1), song.performer());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%grouping%"), song, ""_L1), song.grouping());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%length%"), song, ""_L1), song.PrettyLength());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%filename%"), song, ""_L1), song.basefilename());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%url%"), song, ""_L1), song.url().toString());
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%playcount%"), song, ""_L1), QString::number(song.playcount()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%skipcount%"), song, ""_L1), QString::number(song.skipcount()));
  ASSERT_EQ(Utilities::ReplaceVariable(QStringLiteral("%rating%"), song, ""_L1), song.PrettyRating());

}

TEST(UtilitiesTest, ReplaceMessage) {

  Song song;
  song.set_title(Utilities::GetRandomStringWithChars(8));
  song.set_album(Utilities::GetRandomStringWithChars(8));
  song.set_artist(Utilities::GetRandomStringWithChars(8));
  song.set_albumartist(Utilities::GetRandomStringWithChars(8));
  song.set_track(5);
  song.set_disc(2);
  song.set_year(1999);
  song.set_originalyear(2000);
  song.set_genre(Utilities::GetRandomStringWithChars(8));
  song.set_composer(Utilities::GetRandomStringWithChars(8));
  song.set_performer(Utilities::GetRandomStringWithChars(8));
  song.set_grouping(Utilities::GetRandomStringWithChars(8));
  song.set_length_nanosec(900000000000);
  song.set_url(QUrl(QStringLiteral("file:///home/jonas/Music/test_song.flac")));
  song.set_skipcount(20);
  song.set_playcount(90);
  song.set_rating(1.0);

  ASSERT_EQ(Utilities::ReplaceMessage(QStringLiteral("%title% - %artist%"), song, ""_L1), song.title() + QStringLiteral(" - ") + song.artist());

}

TEST(UtilitiesTest, TemporaryFile) {

  QString filename_pattern = QStringLiteral("/tmp/test-XXXX.jpg");

  TemporaryFile temp_file(filename_pattern);

  EXPECT_FALSE(temp_file.filename().isEmpty());

  EXPECT_FALSE(temp_file.filename() == filename_pattern);

  static const QRegularExpression regex_temp_filename(QStringLiteral("^\\/tmp\\/test-....\\.jpg$"));

  EXPECT_TRUE(regex_temp_filename.match(temp_file.filename()).hasMatch());

}
