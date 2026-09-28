/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
 */

#include "gtest_include.h"

#include <QString>
#include <QList>

#include "collection/collectionwatcher.h"

using namespace Qt::Literals::StringLiterals;

TEST(LrcTest, ParseLrcStandard) {

  const QString lrc = u"[00:12.34]Line 1\n[00:15.80]Line 2\n[00:20.00]Line 3"_s;
  const QList<CollectionWatcher::LrcLine> lines = CollectionWatcher::ParseLrc(lrc);

  ASSERT_EQ(lines.size(), 3);
  EXPECT_EQ(lines[0].timestamp_ms, 12340);
  EXPECT_EQ(lines[0].text, u"Line 1"_s);
  EXPECT_EQ(lines[1].timestamp_ms, 15800);
  EXPECT_EQ(lines[1].text, u"Line 2"_s);
  EXPECT_EQ(lines[2].timestamp_ms, 20000);
  EXPECT_EQ(lines[2].text, u"Line 3"_s);

}

TEST(LrcTest, ParseLrcMultipleTimestampsAndOffset) {

  const QString lrc = u"[offset: 500]\n[00:10.00][01:00.00]Repeated line"_s;
  const QList<CollectionWatcher::LrcLine> lines = CollectionWatcher::ParseLrc(lrc);

  ASSERT_EQ(lines.size(), 2);
  EXPECT_EQ(lines[0].timestamp_ms, 9500);
  EXPECT_EQ(lines[0].text, u"Repeated line"_s);
  EXPECT_EQ(lines[1].timestamp_ms, 59500);
  EXPECT_EQ(lines[1].text, u"Repeated line"_s);

}

TEST(LrcTest, ExtractPlainLyrics) {

  const QString lrc = u"[ti:Song Title]\n[ar:Artist]\n[00:10.00]First line\n[00:20.00]Second line\n"_s;
  const QString plain = CollectionWatcher::ExtractPlainLyrics(lrc);

  EXPECT_EQ(plain, u"First line\nSecond line"_s);

}

