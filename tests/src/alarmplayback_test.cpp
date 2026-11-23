// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "alarm/alarmplayback_android.h"

using namespace Qt::Literals::StringLiterals;

namespace {

using Action = AlarmPlaybackSession::Action;
using Event = AlarmPlaybackSession::Event;
using Phase = AlarmPlaybackSession::Phase;

TEST(AlarmPlaybackTest, ClaimsOnlyOnceThePlayerPlays) {

  AlarmPlaybackSession session;
  EXPECT_FALSE(session.Start(u"a"_s));
  EXPECT_EQ(session.HandleEvent(Event::Playing), Action::None);
  EXPECT_TRUE(session.SongsReady(u"a"_s));
  EXPECT_EQ(session.HandleEvent(Event::Stopped), Action::None);
  EXPECT_EQ(session.HandleEvent(Event::Paused), Action::None);
  EXPECT_EQ(session.HandleEvent(Event::Playing), Action::Claim);
  EXPECT_EQ(session.phase(), Phase::Playing);
  EXPECT_EQ(session.HandleEvent(Event::Playing), Action::None);

}

TEST(AlarmPlaybackTest, StoppingAfterTheClaimHandsBack) {

  for (const Event event : {Event::Paused, Event::Stopped, Event::Finished}) {
    AlarmPlaybackSession session;
    session.Start(u"a"_s);
    session.SongsReady(u"a"_s);
    session.HandleEvent(Event::Playing);
    EXPECT_EQ(session.HandleEvent(event), Action::Fail);
  }

}

TEST(AlarmPlaybackTest, RunningOutOfSongsBeforeTheClaimFails) {

  AlarmPlaybackSession session;
  session.Start(u"a"_s);
  session.SongsReady(u"a"_s);
  EXPECT_EQ(session.HandleEvent(Event::Finished), Action::Fail);

}

TEST(AlarmPlaybackTest, StaleTokensAreIgnored) {

  AlarmPlaybackSession session;
  session.Start(u"a"_s);
  EXPECT_TRUE(session.Start(u"b"_s));
  EXPECT_FALSE(session.SongsReady(u"a"_s));
  EXPECT_FALSE(session.Stop(u"a"_s));
  EXPECT_TRUE(session.IsCurrent(u"b"_s));
  EXPECT_TRUE(session.Stop(u"b"_s));
  EXPECT_EQ(session.phase(), Phase::Idle);
  // Songs found after a stop are not played.
  EXPECT_FALSE(session.SongsReady(u"b"_s));
  EXPECT_EQ(session.HandleEvent(Event::Playing), Action::None);

}

TEST(AlarmPlaybackTest, FadeVolumeFollowsTheJavaCurve) {

  EXPECT_EQ(AlarmPlayback::FadeVolume(1000, 1000, 10000), 1U);
  EXPECT_EQ(AlarmPlayback::FadeVolume(0, 1000, 10000), 1U);
  EXPECT_EQ(AlarmPlayback::FadeVolume(6000, 1000, 10000), 25U);
  EXPECT_EQ(AlarmPlayback::FadeVolume(11000, 1000, 10000), 100U);
  EXPECT_EQ(AlarmPlayback::FadeVolume(50000, 1000, 10000), 100U);
  EXPECT_EQ(AlarmPlayback::FadeVolume(0, 1000, 0), 100U);

}

TEST(AlarmPlaybackTest, ReachableFilesSkipsTheNasOnceItFails) {

  const QStringList files{u"/nas/1"_s, u"/local/2"_s, u"/nas/3"_s, QString(), u"/local/5"_s};
  const QSet<QString> existing{u"/local/2"_s, u"/nas/3"_s};
  quint64 serial = 0;
  QStringList checked;
  const auto exists = [&](const QString &file) {
    checked << file;
    if (file.startsWith("/nas/"_L1) && !existing.contains(file)) ++serial;
    return existing.contains(file);
  };
  const auto is_nas = [](const QString &file) { return file.startsWith("/nas/"_L1); };
  const auto failure_serial = [&serial]() { return serial; };

  EXPECT_EQ(AlarmPlayback::ReachableFiles(files, 10, 10, exists, is_nas, failure_serial), (QList<int>{1, 3}));
  EXPECT_EQ(checked, (QStringList{u"/nas/1"_s, u"/local/2"_s, u"/local/5"_s}));

}

TEST(AlarmPlaybackTest, ReachableFilesKeepsTheNasWhenOneFileIsMissing) {

  const QStringList files{u"/nas/1"_s, u"/nas/2"_s, u"/nas/3"_s};
  const auto exists = [](const QString &file) { return file != "/nas/1"_L1; };
  const auto is_nas = [](const QString &) { return true; };
  const auto failure_serial = []() { return quint64(7); };
  EXPECT_EQ(AlarmPlayback::ReachableFiles(files, 10, 10, exists, is_nas, failure_serial), (QList<int>{1, 2}));
  EXPECT_EQ(AlarmPlayback::ReachableFiles(files, 1, 1, exists, is_nas, failure_serial), (QList<int>{1}));

}

TEST(AlarmPlaybackTest, ReachableFilesKeepsTheRestUncheckedOnceEnoughExist) {

  const QStringList files{u"/nas/1"_s, u"/nas/2"_s, u"/nas/3"_s, u"/nas/4"_s, u"/nas/5"_s};
  QStringList checked;
  const auto exists = [&checked](const QString &file) {
    checked << file;
    return file != "/nas/1"_L1;
  };
  const auto is_nas = [](const QString &) { return true; };
  const auto failure_serial = []() { return quint64(0); };
  EXPECT_EQ(AlarmPlayback::ReachableFiles(files, 4, 2, exists, is_nas, failure_serial), (QList<int>{1, 2, 3, 4}));
  EXPECT_EQ(checked, (QStringList{u"/nas/1"_s, u"/nas/2"_s, u"/nas/3"_s}));

}

}  // namespace
