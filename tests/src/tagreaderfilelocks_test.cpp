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

#include <memory>
#include <future>
#include <chrono>
#include <thread>
#include <list>

#include <QtGlobal>

#ifdef Q_OS_UNIX
#  include <unistd.h>
#endif

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

#include "gtest_include.h"

#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "tagreader/tagreaderfilelocks.h"

using namespace Qt::Literals::StringLiterals;
using namespace std::chrono_literals;

namespace {

class TagReaderFileLocksTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
  }

  // Locks the file in another thread, the future is ready when the lock was taken, then it's released right away.
  // The future waits for the thread when it's destroyed, so the locks of the test are released before, otherwise a test which fails would hang.
  std::future<void> LockInOtherThread(const QString &filename, const TagReaderFileLocks::Mode mode) {
    return std::async(std::launch::async, [this, filename, mode]() {
      const TagReaderFileLocks::Locker l(&file_locks_, filename, mode);
    });
  }

  static bool Finishes(const std::future<void> &future) {
    return future.wait_for(10s) == std::future_status::ready;
  }

  static bool Waits(const std::future<void> &future) {
    return future.wait_for(200ms) == std::future_status::timeout;
  }

  // The files are in a temporary directory, so no other files are used.
  QString Path(const QString &filename) const { return temp_dir_.filePath(filename); }

  static void CreateFile(const QString &filename) {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();
  }

  static bool CreateHardLink(const QString &filename, const QString &link_filename) {
#ifdef Q_OS_WIN
    return ::CreateHardLinkW(reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(link_filename).utf16()), reinterpret_cast<const wchar_t*>(QDir::toNativeSeparators(filename).utf16()), nullptr) != 0;
#else
    return ::link(QFile::encodeName(filename).constData(), QFile::encodeName(link_filename).constData()) == 0;
#endif
  }

  // Replaces the file by renaming another file over it, like FileWriteGuard does.
  void ReplaceFile(const QString &filename) const {
    CreateFile(Path(u"new.flac"_s));
    ASSERT_TRUE(QFile::remove(filename));
    ASSERT_TRUE(QFile::rename(Path(u"new.flac"_s), filename));
  }

  // Holds a lock in another thread until it's released, all locks are released before the threads are joined, also when an assertion fails.
  class HeldLock {
   public:
    explicit HeldLock(TagReaderFileLocks *file_locks, const QString &filename, const TagReaderFileLocks::Mode mode)
        : locked_future_(locked_.get_future()),
          release_future_(release_.get_future()),
          thread_([this, file_locks, filename, mode]() {
            const TagReaderFileLocks::Locker l(file_locks, filename, mode);
            locked_.set_value();
            release_future_.wait();
          }) {}
    ~HeldLock() {
      Release();
      thread_.join();
    }
    bool IsLocked(const std::chrono::milliseconds timeout) const { return locked_future_.wait_for(timeout) == std::future_status::ready; }
    void Release() {
      if (released_) return;
      released_ = true;
      release_.set_value();
    }

   private:
    Q_DISABLE_COPY_MOVE(HeldLock)
    std::promise<void> locked_;
    std::future<void> locked_future_;
    std::promise<void> release_;
    std::future<void> release_future_;
    bool released_ = false;
    std::thread thread_;
  };

  HeldLock *HoldLock(const QString &filename, const TagReaderFileLocks::Mode mode) {
    held_locks_.push_back(std::make_unique<HeldLock>(&file_locks_, filename, mode));
    return held_locks_.back().get();
  }

  void TearDown() override {
    for (const std::unique_ptr<HeldLock> &held_lock : held_locks_) held_lock->Release();
    held_locks_.clear();
  }

  QTemporaryDir temp_dir_;
  TagReaderFileLocks file_locks_;
  std::list<std::unique_ptr<HeldLock>> held_locks_;
};

// Regression test: one lock was used for all files, so reading a file, also from the GUI thread, waited for writing any other file.
TEST_F(TagReaderFileLocksTest, ReadingOtherFileDoesNotWaitForWrite) {

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);

  std::future<void> read = LockInOtherThread(Path(u"b.flac"_s), TagReaderFileLocks::Mode::Read);
  EXPECT_TRUE(Finishes(read));
  std::future<void> write = LockInOtherThread(Path(u"b.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Finishes(write));

  l.reset();

}

TEST_F(TagReaderFileLocksTest, ReadingFileWaitsForWritingIt) {

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);

  std::future<void> read = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  EXPECT_TRUE(Waits(read));

  l.reset();
  EXPECT_TRUE(Finishes(read));

}

TEST_F(TagReaderFileLocksTest, WritingFileWaitsForReadingIt) {

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);

  std::future<void> write = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(write));

  l.reset();
  EXPECT_TRUE(Finishes(write));

}

TEST_F(TagReaderFileLocksTest, FileCanBeReadByTwoThreads) {

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);

  std::future<void> read = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  EXPECT_TRUE(Finishes(read));

  l.reset();

}

TEST_F(TagReaderFileLocksTest, SameFileWithOtherPathUsesSameLock) {

  ASSERT_TRUE(QDir().mkpath(Path(u"album"_s)));
  CreateFile(Path(u"a.flac"_s));

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"album/../a.flac"_s), TagReaderFileLocks::Mode::Write);

  std::future<void> read = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  EXPECT_TRUE(Waits(read));

  l.reset();
  EXPECT_TRUE(Finishes(read));

}

#ifdef Q_OS_UNIX
// Regression test: the links were not resolved, so a file could be written through a link while it was written or read through another path.
TEST_F(TagReaderFileLocksTest, PathThroughLinkUsesSameLock) {

  ASSERT_TRUE(QDir().mkpath(Path(u"music"_s)));
  CreateFile(Path(u"music/a.flac"_s));
  ASSERT_TRUE(QFile::link(Path(u"music"_s), Path(u"link"_s)));

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"link/a.flac"_s), TagReaderFileLocks::Mode::Write);

  std::future<void> write = LockInOtherThread(Path(u"music/a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(write));

  l.reset();
  EXPECT_TRUE(Finishes(write));

}
#endif

// Regression test: hard links to the same file had different locks, so the file could be written through one while it was written or read through the other.
TEST_F(TagReaderFileLocksTest, HardLinkUsesSameLock) {

  CreateFile(Path(u"a.flac"_s));
  ASSERT_TRUE(CreateHardLink(Path(u"a.flac"_s), Path(u"b.flac"_s)));

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);

  std::future<void> write = LockInOtherThread(Path(u"b.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(write));

  l.reset();
  EXPECT_TRUE(Finishes(write));

}

// Regression test: the file was only locked by its identity, so when it was replaced while it was written, like on GVFS, a write which came after used the lock of the new file, and didn't wait for a write which was waiting for the lock of the old file.
TEST_F(TagReaderFileLocksTest, ReplacedFileUsesSameLock) {

  CreateFile(Path(u"a.flac"_s));
  // The futures are declared before the lock, so the lock is released first when an assertion fails, otherwise the futures would wait for it.
  std::future<void> waiting_write;
  std::future<void> write;
  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);

  waiting_write = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(waiting_write));

  // Replaced like FileWriteGuard does, by renaming another file over it.
  ReplaceFile(Path(u"a.flac"_s));

  write = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(write));

  l.reset();
  EXPECT_TRUE(Finishes(waiting_write));
  EXPECT_TRUE(Finishes(write));

}

// Regression test: a write which waited while the file was replaced kept the lock of the old file, so a hard link to the new file could be written at the same time.
TEST_F(TagReaderFileLocksTest, WriteWaitingWhileFileIsReplacedLocksNewFile) {

  CreateFile(Path(u"a.flac"_s));
  HeldLock *first_write = HoldLock(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  ASSERT_TRUE(first_write->IsLocked(10s));

  HeldLock *waiting_write = HoldLock(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_FALSE(waiting_write->IsLocked(200ms));

  ReplaceFile(Path(u"a.flac"_s));
  ASSERT_TRUE(CreateHardLink(Path(u"a.flac"_s), Path(u"b.flac"_s)));

  first_write->Release();
  ASSERT_TRUE(waiting_write->IsLocked(10s));

  HeldLock *hard_link_write = HoldLock(Path(u"b.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_FALSE(hard_link_write->IsLocked(200ms));

  waiting_write->Release();
  EXPECT_TRUE(hard_link_write->IsLocked(10s));

}

// The file can also be replaced again while the write waits for the lock of the new file, then it's locked again with the newest file.
TEST_F(TagReaderFileLocksTest, WriteWaitingWhileFileIsReplacedTwiceLocksNewestFile) {

  CreateFile(Path(u"a.flac"_s));
  HeldLock *first_write = HoldLock(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  ASSERT_TRUE(first_write->IsLocked(10s));

  HeldLock *waiting_write = HoldLock(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_FALSE(waiting_write->IsLocked(200ms));

  // The new file is locked through a hard link, so the write waits for it after the first write.
  ReplaceFile(Path(u"a.flac"_s));
  ASSERT_TRUE(CreateHardLink(Path(u"a.flac"_s), Path(u"b.flac"_s)));
  HeldLock *hard_link_write = HoldLock(Path(u"b.flac"_s), TagReaderFileLocks::Mode::Write);
  ASSERT_TRUE(hard_link_write->IsLocked(10s));

  first_write->Release();
  EXPECT_FALSE(waiting_write->IsLocked(200ms));

  ReplaceFile(Path(u"a.flac"_s));
  ASSERT_TRUE(CreateHardLink(Path(u"a.flac"_s), Path(u"c.flac"_s)));

  hard_link_write->Release();
  ASSERT_TRUE(waiting_write->IsLocked(10s));

  HeldLock *other_hard_link_write = HoldLock(Path(u"c.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_FALSE(other_hard_link_write->IsLocked(200ms));

  waiting_write->Release();
  EXPECT_TRUE(other_hard_link_write->IsLocked(10s));

}

#ifdef Q_OS_UNIX
// Regression test: the path of a file which doesn't exist kept the link of its directory, so when the file was created through the directory the link links to, it had another lock.
TEST_F(TagReaderFileLocksTest, FileCreatedThroughLinkedDirectoryUsesSameLock) {

  ASSERT_TRUE(QDir().mkpath(Path(u"music"_s)));
  ASSERT_TRUE(QFile::link(Path(u"music"_s), Path(u"link"_s)));

  HeldLock *write = HoldLock(Path(u"link/a.flac"_s), TagReaderFileLocks::Mode::Write);
  ASSERT_TRUE(write->IsLocked(10s));

  CreateFile(Path(u"music/a.flac"_s));
  HeldLock *other_write = HoldLock(Path(u"music/a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_FALSE(other_write->IsLocked(200ms));

  write->Release();
  EXPECT_TRUE(other_write->IsLocked(10s));

}
#endif

TEST_F(TagReaderFileLocksTest, RelativePathUsesLockOfAbsolutePath) {

  auto l = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, u"a.flac"_s, TagReaderFileLocks::Mode::Write);

  std::future<void> read = LockInOtherThread(QDir::current().absoluteFilePath(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  EXPECT_TRUE(Waits(read));

  l.reset();
  EXPECT_TRUE(Finishes(read));

}

// The lock of a file is kept while another thread still has it.
TEST_F(TagReaderFileLocksTest, LockIsKeptWhileUsed) {

  auto l1 = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  auto l2 = std::make_unique<TagReaderFileLocks::Locker>(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
  l1.reset();

  std::future<void> write = LockInOtherThread(Path(u"a.flac"_s), TagReaderFileLocks::Mode::Write);
  EXPECT_TRUE(Waits(write));

  l2.reset();
  EXPECT_TRUE(Finishes(write));

}

TEST_F(TagReaderFileLocksTest, LockIsRemovedWhenNotUsed) {

  {
    const TagReaderFileLocks::Locker l1(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
    const TagReaderFileLocks::Locker l2(&file_locks_, Path(u"a.flac"_s), TagReaderFileLocks::Mode::Read);
    const TagReaderFileLocks::Locker l3(&file_locks_, Path(u"b.flac"_s), TagReaderFileLocks::Mode::Read);
    EXPECT_EQ(2, file_locks_.count());
  }

  EXPECT_EQ(0, file_locks_.count());

}

}  // namespace
