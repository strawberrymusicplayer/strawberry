// SPDX-License-Identifier: GPL-3.0-or-later

#include "gtest_include.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include "alarm/alarmtracks_android.h"

namespace {

constexpr qint64 kMegabyte = 1024 * 1024;

TEST(AlarmTracksTest, NormalizedReplacesValuesThatAreNoChoice) {

  AlarmTrackSettings settings;
  settings.copy = false;
  settings.tracks_per_alarm = 7;
  settings.maximum_total_mb = 0;
  const AlarmTrackSettings normalized = settings.Normalized();
  EXPECT_FALSE(normalized.copy);
  EXPECT_EQ(normalized.tracks_per_alarm, AlarmTrackSettings::kDefaultTracksPerAlarm);
  EXPECT_EQ(normalized.maximum_total_mb, AlarmTrackSettings::kDefaultMaximumTotalMb);

  settings.tracks_per_alarm = 10;
  settings.maximum_total_mb = 1024;
  EXPECT_EQ(settings.Normalized().tracks_per_alarm, 10);
  EXPECT_EQ(settings.Normalized().maximum_total_bytes(), 1024 * kMegabyte);

}

TEST(AlarmTracksTest, CopyFitsHonoursTrackCountAndTotalSize) {

  AlarmTrackSettings settings;
  settings.tracks_per_alarm = 3;
  settings.maximum_total_mb = 50;
  EXPECT_TRUE(AlarmTracks::CopyFits(10 * kMegabyte, 0, 0, settings));
  EXPECT_TRUE(AlarmTracks::CopyFits(10 * kMegabyte, 2, 40 * kMegabyte, settings));
  EXPECT_FALSE(AlarmTracks::CopyFits(10 * kMegabyte, 3, 0, settings));
  EXPECT_FALSE(AlarmTracks::CopyFits(11 * kMegabyte, 2, 40 * kMegabyte, settings));
  EXPECT_FALSE(AlarmTracks::CopyFits(0, 0, 0, settings));

  // A huge song is not copied even when the total allows it.
  settings.maximum_total_mb = 1024;
  EXPECT_FALSE(AlarmTracks::CopyFits(100 * kMegabyte, 0, 0, settings));

}

TEST(AlarmTracksTest, SourcesForCopiesAndOriginals) {

  const QUrl local = QUrl::fromLocalFile(QStringLiteral("/storage/emulated/0/Music/a b.mp3"));
  const QUrl nas = QUrl::fromLocalFile(QStringLiteral("/strawberry-nas/home/Music/a.flac"));
  const QUrl content(QStringLiteral("content://com.android.externalstorage.documents/document/primary%3AMusic%2Fa.mp3"));
  const QUrl http(QStringLiteral("http://example.com/stream.mp3"));
  const QUrl tidal(QStringLiteral("tidal:123"));

  EXPECT_EQ(AlarmTracks::CopySource(local), QStringLiteral("/storage/emulated/0/Music/a b.mp3"));
  EXPECT_EQ(AlarmTracks::CopySource(nas), QStringLiteral("/strawberry-nas/home/Music/a.flac"));
  EXPECT_EQ(AlarmTracks::CopySource(content), content.toString(QUrl::FullyEncoded));
  EXPECT_TRUE(AlarmTracks::CopySource(http).isEmpty());
  EXPECT_TRUE(AlarmTracks::CopySource(tidal).isEmpty());

  EXPECT_EQ(AlarmTracks::OriginalSource(local), QStringLiteral("/storage/emulated/0/Music/a b.mp3"));
  EXPECT_EQ(AlarmTracks::OriginalSource(content), content.toString(QUrl::FullyEncoded));
  EXPECT_EQ(AlarmTracks::OriginalSource(http), QStringLiteral("http://example.com/stream.mp3"));
  EXPECT_TRUE(AlarmTracks::OriginalSource(tidal).isEmpty());

}

TEST(AlarmTracksTest, DirectorySizeCountsNestedFiles) {

  QTemporaryDir directory(QDir::currentPath() + QStringLiteral("/alarmtracks_test-XXXXXX"));
  ASSERT_TRUE(directory.isValid());
  EXPECT_EQ(AlarmTracks::DirectorySize(directory.path()), 0);
  EXPECT_EQ(AlarmTracks::DirectorySize(directory.filePath(QStringLiteral("missing"))), 0);

  ASSERT_TRUE(QDir(directory.path()).mkpath(QStringLiteral("alarm1")));
  QFile first(directory.filePath(QStringLiteral("alarm1/1_0.mp3")));
  ASSERT_TRUE(first.open(QIODevice::WriteOnly));
  first.write(QByteArray(1000, 'a'));
  first.close();
  QFile second(directory.filePath(QStringLiteral("2_0.ogg")));
  ASSERT_TRUE(second.open(QIODevice::WriteOnly));
  second.write(QByteArray(24, 'b'));
  second.close();
  EXPECT_EQ(AlarmTracks::DirectorySize(directory.path()), 1024);

}

}  // namespace
