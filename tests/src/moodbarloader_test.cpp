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

#include <memory>
#include <algorithm>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QList>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QIODevice>
#include <QTemporaryDir>
#include <QAbstractNetworkCache>
#include <QNetworkDiskCache>
#include <QNetworkCacheMetaData>

#include "moodbar/moodbarloader.h"

#ifdef Q_OS_UNIX
#  include <csignal>
#  include <sys/resource.h>
#endif

using namespace Qt::Literals::StringLiterals;

namespace {

// A device which writes at most the given number of bytes each time, like a device which is full.
class ShortWriteDevice : public QBuffer {
 public:
  explicit ShortWriteDevice(const qint64 max_write) : max_write_(max_write) {}

 protected:
  qint64 writeData(const char *data, const qint64 len) override {
    return QBuffer::writeData(data, std::min(len, max_write_));
  }

 private:
  const qint64 max_write_;
};

// A cache which gives a device that can't be written to, or which only writes part of the data, and keeps track of the savings which are inserted and removed.
class FakeDeviceCache : public QAbstractNetworkCache {
 public:
  // With a maximum write size, the device only writes part of the data, otherwise it can't be written to.
  explicit FakeDeviceCache(const qint64 max_write = -1) : max_write_(max_write) {}
  qint64 cacheSize() const override { return 0; }
  QIODevice *data(const QUrl &url) override {
    Q_UNUSED(url)
    return nullptr;
  }
  void insert(QIODevice *inserted_device) override {
    inserted << inserted_device;
  }
  QNetworkCacheMetaData metaData(const QUrl &url) override {
    Q_UNUSED(url)
    return QNetworkCacheMetaData();
  }
  QIODevice *prepare(const QNetworkCacheMetaData &metadata) override {
    Q_UNUSED(metadata)
    if (max_write_ >= 0) {
      device = std::make_unique<ShortWriteDevice>(max_write_);
      device->open(QIODevice::WriteOnly);
    }
    else {
      device = std::make_unique<QBuffer>();
      device->open(QIODevice::ReadOnly);
    }
    return &*device;
  }
  bool remove(const QUrl &url) override {
    removed << url;
    return true;
  }
  void updateMetaData(const QNetworkCacheMetaData &metadata) override { Q_UNUSED(metadata) }
  void clear() override {}

  std::unique_ptr<QBuffer> device;
  QList<QIODevice*> inserted;
  QList<QUrl> removed;

 private:
  const qint64 max_write_;
};

QByteArray ReadFile(const QString &filename) {
  QFile file(filename);
  if (!file.open(QIODevice::ReadOnly)) return QByteArray();
  return file.readAll();
}

TEST(MoodbarLoaderTest, SaveToCache) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  QNetworkDiskCache cache;
  cache.setCacheDirectory(temp_dir.path());

  const QString filename = u"/music/song.flac"_s;
  ASSERT_TRUE(MoodbarLoader::SaveToCache(&cache, filename, "moodbar"));

  const QUrl url(QString::fromLatin1(QUrl::toPercentEncoding(filename)));
  std::unique_ptr<QIODevice> device(cache.data(url));
  ASSERT_NE(nullptr, device);
  EXPECT_EQ("moodbar", device->readAll());

}

// Regression test: when the data wasn't written to the cache, the saving was neither inserted nor removed, so the cache kept it.
TEST(MoodbarLoaderTest, SaveToCacheRemovesSavingWhenWriteFails) {

  FakeDeviceCache cache;
  EXPECT_FALSE(MoodbarLoader::SaveToCache(&cache, u"/music/song.flac"_s, "moodbar"));
  EXPECT_TRUE(cache.inserted.isEmpty());
  ASSERT_EQ(1, cache.removed.count());
  EXPECT_EQ(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(u"/music/song.flac"_s))), cache.removed.first());

}

// Empty data is not saved in the cache.
TEST(MoodbarLoaderTest, SaveToCacheIgnoresEmptyData) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  QNetworkDiskCache cache;
  cache.setCacheDirectory(temp_dir.path());

  const QString filename = u"/music/song.flac"_s;
  EXPECT_FALSE(MoodbarLoader::SaveToCache(&cache, filename, QByteArray()));
  EXPECT_FALSE(cache.metaData(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(filename)))).isValid());

}

// Regression test: data which was partially written to the cache was inserted.
TEST(MoodbarLoaderTest, SaveToCacheRemovesSavingWhenDataIsPartiallyWritten) {

  FakeDeviceCache cache(3);
  EXPECT_FALSE(MoodbarLoader::SaveToCache(&cache, u"/music/song.flac"_s, "moodbar"));
  EXPECT_TRUE(cache.inserted.isEmpty());
  ASSERT_EQ(1, cache.removed.count());
  EXPECT_EQ(QUrl(QString::fromLatin1(QUrl::toPercentEncoding(u"/music/song.flac"_s))), cache.removed.first());

}

TEST(MoodbarLoaderTest, SaveMoodFile) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString mood_filename = temp_dir.filePath(u".song.mood"_s);

  ASSERT_TRUE(MoodbarLoader::SaveMoodFile(mood_filename, "moodbar"));
  EXPECT_EQ("moodbar", ReadFile(mood_filename));

  // An existing mood file is replaced.
  ASSERT_TRUE(MoodbarLoader::SaveMoodFile(mood_filename, "new"));
  EXPECT_EQ("new", ReadFile(mood_filename));

  // No temporary file is left.
  EXPECT_EQ(QStringList() << u".song.mood"_s, QDir(temp_dir.path()).entryList(QDir::Files | QDir::Hidden));

}

TEST(MoodbarLoaderTest, SaveMoodFileFailsInMissingDirectory) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString mood_filename = temp_dir.filePath(u"missing/.song.mood"_s);

  EXPECT_FALSE(MoodbarLoader::SaveMoodFile(mood_filename, "moodbar"));
  EXPECT_FALSE(QFile::exists(mood_filename));

}

#ifdef Q_OS_UNIX

// Limits the size of the files the process can write while this exists, writing more fails instead of the signal stopping the process.
class ScopedFileSizeLimit {
 public:
  explicit ScopedFileSizeLimit(const rlim_t size) : old_handler_(std::signal(SIGXFSZ, SIG_IGN)) {
    getrlimit(RLIMIT_FSIZE, &old_limit_);
    rlimit limit = old_limit_;
    limit.rlim_cur = size;
    setrlimit(RLIMIT_FSIZE, &limit);
  }
  ~ScopedFileSizeLimit() {
    setrlimit(RLIMIT_FSIZE, &old_limit_);
    std::signal(SIGXFSZ, old_handler_);
  }
  Q_DISABLE_COPY_MOVE(ScopedFileSizeLimit)

 private:
  rlimit old_limit_;
  void (*old_handler_)(int);
};

// Regression test: the mood file was written directly, so when writing failed, for example when the disk was full, the mood file was left partially written, and it was loaded as the moodbar.
TEST(MoodbarLoaderTest, SaveMoodFileKeepsExistingFileWhenWritingFails) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString mood_filename = temp_dir.filePath(u".song.mood"_s);
  ASSERT_TRUE(MoodbarLoader::SaveMoodFile(mood_filename, "old"));

  bool saved = true;
  {
    ScopedFileSizeLimit file_size_limit(4);
    saved = MoodbarLoader::SaveMoodFile(mood_filename, "new moodbar");
  }

  EXPECT_FALSE(saved);
  EXPECT_EQ("old", ReadFile(mood_filename));
  EXPECT_EQ(QStringList() << u".song.mood"_s, QDir(temp_dir.path()).entryList(QDir::Files | QDir::Hidden));

}

#endif  // Q_OS_UNIX

}  // namespace
