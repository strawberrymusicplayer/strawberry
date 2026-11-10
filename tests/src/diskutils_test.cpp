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

#include <QtGlobal>
#include <QString>
#include <QStorageInfo>
#include <QTemporaryDir>

#include "utilities/diskutils.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Free space can change between the two reads, while other programs write, so it's compared with a margin.
constexpr qint64 kFreeSpaceMargin = 64LL * 1024LL * 1024LL;

// Regression test: the capacity was counted in f_bsize instead of f_frsize units, which is the preferred I/O size on macOS and FreeBSD, so the capacity was 8 to 256 times too large there.
TEST(DiskUtilsTest, CapacityMatchesStorageInfo) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QStorageInfo storage_info(temp_dir.path());
  ASSERT_TRUE(storage_info.isValid());
  ASSERT_GT(storage_info.bytesTotal(), 0);

  EXPECT_EQ(static_cast<quint64>(storage_info.bytesTotal()), Utilities::FileSystemCapacity(temp_dir.path()));

}

// Regression test: the free space was counted in f_bsize instead of f_frsize units, so organizing to a nearly full disk wasn't stopped on macOS and FreeBSD.
TEST(DiskUtilsTest, FreeSpaceMatchesStorageInfo) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QStorageInfo storage_info(temp_dir.path());
  ASSERT_TRUE(storage_info.isValid());

  const qint64 free_space = static_cast<qint64>(Utilities::FileSystemFreeSpace(temp_dir.path()));
  EXPECT_LE(qAbs(storage_info.bytesAvailable() - free_space), kFreeSpaceMargin) << "QStorageInfo: " << storage_info.bytesAvailable() << ", FileSystemFreeSpace: " << free_space;
  EXPECT_LE(free_space, static_cast<qint64>(Utilities::FileSystemCapacity(temp_dir.path())));

}

// A path which doesn't exist has no capacity or free space.
TEST(DiskUtilsTest, MissingPathIsZero) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString missing_path = temp_dir.path() + u"/missing/directory"_s;

  EXPECT_EQ(0U, Utilities::FileSystemCapacity(missing_path));
  EXPECT_EQ(0U, Utilities::FileSystemFreeSpace(missing_path));

}

}  // namespace
