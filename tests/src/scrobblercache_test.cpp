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
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "includes/scoped_ptr.h"
#include "constants/timeconstants.h"
#include "core/song.h"
#include "core/standardpaths.h"
#include "scrobbler/scrobblercache.h"
#include "scrobbler/scrobblercacheitem.h"

using namespace Qt::Literals::StringLiterals;

namespace {

class ScrobblerCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // The cache file is in a unique directory in the cache location, which is removed after the test, so the tests don't share or leave a cache file on any platform.
    QStandardPaths::setTestModeEnabled(true);
    const QString cache_location = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation);
    ASSERT_TRUE(QDir().mkpath(cache_location));
    cache_dir_.reset(new QTemporaryDir(cache_location + u"/scrobblercachetest-XXXXXX"_s));
    ASSERT_TRUE(cache_dir_->isValid());
    cache_filename_ = QDir(cache_location).relativeFilePath(cache_dir_->filePath(u"scrobblercache.json"_s));
  }

  static Song TestSong() {
    Song song;
    song.set_artist(u"Artist"_s);
    song.set_title(u"Title"_s);
    song.set_length_nanosec(180 * kNsecPerSec);
    return song;
  }

  ScopedPtr<QTemporaryDir> cache_dir_;
  QString cache_filename_;
};

TEST_F(ScrobblerCacheTest, UnsavedScrobblesAreWrittenOnDestruction) {

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    cache.Add(TestSong(), 1700000000);
  }

  ScrobblerCache cache(cache_filename_, nullptr);
  EXPECT_EQ(1, cache.Count());

}

// Regression test: when all cached scrobbles were sent, the cache file was only rewritten by the flush timer, so when the cache was destroyed before the timer elapsed, the file kept the sent scrobbles, and they were sent again.
TEST_F(ScrobblerCacheTest, SentScrobblesAreRemovedFromFileOnDestruction) {

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    cache.Add(TestSong(), 1700000000);
    cache.WriteCache();
  }

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    ASSERT_EQ(1, cache.Count());
    cache.Flush(cache.List());
    EXPECT_EQ(0, cache.Count());
  }

  ScrobblerCache cache(cache_filename_, nullptr);
  EXPECT_EQ(0, cache.Count());

}

TEST_F(ScrobblerCacheTest, RemovedScrobblesAreRemovedFromFileOnDestruction) {

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    cache.Add(TestSong(), 1700000000);
    cache.Add(TestSong(), 1700000300);
    cache.WriteCache();
  }

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    ASSERT_EQ(2, cache.Count());
    cache.Remove(cache.List().first());
  }

  ScrobblerCache cache(cache_filename_, nullptr);
  EXPECT_EQ(1, cache.Count());

}

// Regression test: when writing the cache file failed, the cache was still marked as saved, so it was not written on exit either, and the scrobbles were lost.
TEST_F(ScrobblerCacheTest, FailedWriteIsRetriedOnDestruction) {

  {
    ScrobblerCache cache(cache_filename_, nullptr);
    cache.Add(TestSong(), 1700000000);

    // Writing fails while the directory of the cache file is missing.
    ASSERT_TRUE(cache_dir_->remove());
    cache.WriteCache();
    ASSERT_TRUE(QDir().mkpath(cache_dir_->path()));
  }

  ScrobblerCache cache(cache_filename_, nullptr);
  EXPECT_EQ(1, cache.Count());

}

}  // namespace
