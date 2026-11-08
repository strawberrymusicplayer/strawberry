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

#include <QChar>
#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "discord/discordrpc.h"
#include "discord/discordpresence.h"

using namespace Qt::Literals::StringLiterals;

class DiscordRPCTest : public ::testing::Test {
 protected:
  // The activity sent to Discord for the presence.
  static QJsonObject Activity(const DiscordPresence &presence) {
    DiscordRPC discord_rpc(u"1"_s);
    const QJsonDocument json_document = QJsonDocument::fromJson(discord_rpc.CreatePresenceMessage(presence));
    return json_document.object().value("args"_L1).toObject().value("activity"_L1).toObject();
  }
};

namespace {

constexpr qsizetype kMaxLength = 128;

DiscordPresence Presence(const QString &title, const QString &artist, const QString &album) {

  DiscordPresence presence;
  presence.type = 2;
  presence.details = title;
  presence.state = artist;
  presence.large_image_key = u"embedded_cover"_s;
  presence.large_image_text = u"on "_s + album;
  return presence;

}

}  // namespace

// Regression test: Discord rejects the whole activity when a text is longer than 128 characters, so nothing was shown for songs with a long title, artist or album.
TEST_F(DiscordRPCTest, LongTextsAreShortened) {

  const QString long_text = QString(200, u'a');
  const QJsonObject activity = Activity(Presence(long_text, long_text, long_text));

  const QString details = activity.value("details"_L1).toString();
  const QString state = activity.value("state"_L1).toString();
  const QString large_text = activity.value("assets"_L1).toObject().value("large_text"_L1).toString();

  for (const QString &text : {details, state, large_text}) {
    EXPECT_EQ(kMaxLength, text.size());
    EXPECT_EQ(QChar(0x2026), text.back());
  }
  EXPECT_EQ(QString(kMaxLength - 1, u'a') + QChar(0x2026), details);
  EXPECT_TRUE(large_text.startsWith(u"on aaa"_s));

}

TEST_F(DiscordRPCTest, TextsWithinTheLimitAreUnchanged) {

  const QString title = QString(kMaxLength, u'a');
  const QJsonObject activity = Activity(Presence(title, u"Artist"_s, u"Album"_s));

  EXPECT_EQ(title, activity.value("details"_L1).toString());
  EXPECT_EQ(u"Artist"_s, activity.value("state"_L1).toString());
  EXPECT_EQ(u"on Album"_s, activity.value("assets"_L1).toObject().value("large_text"_L1).toString());

}

// A character which needs two UTF-16 code units, like an emoji, at the limit is not split.
TEST_F(DiscordRPCTest, LongTextsDontSplitSurrogatePairs) {

  // The emoji takes code units 126 and 127, and code unit 127 is needed for the ellipsis, so cutting at 127 code units would keep only half of it.
  const QString title = QString(kMaxLength - 2, u'a') + QString::fromUtf8("\xF0\x9F\x8D\x93") + QString(10, u'b');
  ASSERT_TRUE(title.at(kMaxLength - 2).isHighSurrogate());

  const QString details = Activity(Presence(title, u"Artist"_s, u"Album"_s)).value("details"_L1).toString();

  EXPECT_EQ(QString(kMaxLength - 2, u'a') + QChar(0x2026), details);
  for (const QChar c : details) {
    EXPECT_FALSE(c.isSurrogate());
  }

}

// Discord also rejects the whole activity when a text is shorter than 2 characters, this was only handled for the artist.
TEST_F(DiscordRPCTest, ShortTextsArePadded) {

  const QJsonObject activity = Activity(Presence(u"X"_s, u"Y"_s, u"Album"_s));

  EXPECT_EQ(u"X "_s, activity.value("details"_L1).toString());
  EXPECT_EQ(u"Y "_s, activity.value("state"_L1).toString());

}
