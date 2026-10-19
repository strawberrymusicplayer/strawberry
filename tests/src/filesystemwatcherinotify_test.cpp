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

#include "gtest_include.h"

#include <QString>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QCoreApplication>

#include "core/filesystemwatcherinotify.h"

using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

int MaxQueuedEvents() {

  QFile file(u"/proc/sys/fs/inotify/max_queued_events"_s);
  if (!file.open(QIODevice::ReadOnly)) return -1;
  return file.readAll().trimmed().toInt();

}

TEST(FileSystemWatcherInotifyTest, PathChanged) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  FileSystemWatcherInotify watcher;
  QSignalSpy spy_path_changed(&watcher, &FileSystemWatcherInterface::PathChanged);
  QSignalSpy spy_overflow(&watcher, &FileSystemWatcherInterface::Overflow);
  watcher.AddPath(temp_dir.path());

  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"New directory"_s));
  ASSERT_TRUE(spy_path_changed.wait(5000));
  EXPECT_EQ(temp_dir.path(), spy_path_changed.at(0).at(0).toString());
  EXPECT_EQ(0, spy_overflow.count());

}

// When the kernel's event queue overflows, the changes after it are lost, which is reported, so the paths can be scanned.
TEST(FileSystemWatcherInotifyTest, Overflow) {

  const int max_queued_events = MaxQueuedEvents();
  if (max_queued_events <= 0 || max_queued_events > 100000) {
    GTEST_SKIP() << "Can't overflow the inotify event queue, max_queued_events is" << max_queued_events;
  }

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  FileSystemWatcherInotify watcher;
  QSignalSpy spy_overflow(&watcher, &FileSystemWatcherInterface::Overflow);
  watcher.AddPath(temp_dir.path());

  // Each file gives a create and a close event, and the events are not read before the event loop runs, so the queue overflows.
  for (int i = 0; i < max_queued_events; ++i) {
    QFile file(QDir(temp_dir.path()).filePath(QString::number(i)));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  }

  ASSERT_TRUE(spy_overflow.wait(5000));
  EXPECT_EQ(1, spy_overflow.count());

}

// A directory which was deleted and created again while its events were not read is watched again by adding it again.
TEST(FileSystemWatcherInotifyTest, AddPathAgainWatchesRecreatedDirectory) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  const QString path = QDir(temp_dir.path()).filePath(u"Album"_s);
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"Album"_s));

  FileSystemWatcherInotify watcher;
  watcher.AddPath(path);

  ASSERT_TRUE(QDir(path).removeRecursively());
  ASSERT_TRUE(QDir(temp_dir.path()).mkdir(u"Album"_s));
  watcher.AddPath(path);

  // The events for the deleted directory don't remove the watch for the directory created again.
  QSignalSpy spy_path_changed(&watcher, &FileSystemWatcherInterface::PathChanged);
  QCoreApplication::processEvents();
  spy_path_changed.clear();

  QFile file(QDir(path).filePath(u"01.mp3"_s));
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.close();
  ASSERT_TRUE(spy_path_changed.wait(5000));
  EXPECT_EQ(path, spy_path_changed.at(0).at(0).toString());

}

// Adding the watched paths again doesn't give any events, so it doesn't overflow the event queue, even with more paths than fit in it.
TEST(FileSystemWatcherInotifyTest, AddPathsAgainWithMorePathsThanQueuedEvents) {

  const int max_queued_events = MaxQueuedEvents();
  if (max_queued_events <= 0 || max_queued_events > 100000) {
    GTEST_SKIP() << "Can't overflow the inotify event queue, max_queued_events is" << max_queued_events;
  }

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  QStringList paths;
  for (int i = 0; i <= max_queued_events; ++i) {
    const QString name = QString::number(i);
    ASSERT_TRUE(QDir(temp_dir.path()).mkdir(name));
    paths << QDir(temp_dir.path()).filePath(name);
  }

  FileSystemWatcherInotify watcher;
  QSignalSpy spy_overflow(&watcher, &FileSystemWatcherInterface::Overflow);
  watcher.AddPaths(paths);
  watcher.AddPaths(paths);

  // A change after adding the paths again is still reported, without an overflow before it.
  QSignalSpy spy_path_changed(&watcher, &FileSystemWatcherInterface::PathChanged);
  ASSERT_TRUE(QDir(paths.first()).mkdir(u"New"_s));
  ASSERT_TRUE(spy_path_changed.wait(5000));
  EXPECT_EQ(0, spy_overflow.count());

}

// When two watched directories were swapped while the event queue overflowed, adding them again watches both, the watch for one directory is given to the other path.
TEST(FileSystemWatcherInotifyTest, AddPathsAgainWatchesSwappedDirectories) {

  const int max_queued_events = MaxQueuedEvents();
  if (max_queued_events <= 0 || max_queued_events > 100000) {
    GTEST_SKIP() << "Can't overflow the inotify event queue, max_queued_events is" << max_queued_events;
  }

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());
  QDir root(temp_dir.path());
  ASSERT_TRUE(root.mkdir(u"A"_s));
  ASSERT_TRUE(root.mkdir(u"B"_s));
  const QString path_a = root.filePath(u"A"_s);
  const QString path_b = root.filePath(u"B"_s);

  const QStringList filenames_to_change = QStringList() << QDir(path_a).filePath(u"a.jpg"_s) << QDir(path_a).filePath(u"b.jpg"_s);
  for (const QString &filename : filenames_to_change) {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  }

  FileSystemWatcherInotify watcher;
  QSignalSpy spy_overflow(&watcher, &FileSystemWatcherInterface::Overflow);
  watcher.AddPaths(QStringList() << path_a << path_b);

  // Fill the event queue before the events are read, so the events for swapping the directories are lost.
  // The changes alternate between two files, the kernel only merges an event with the same event before it.
  for (int i = 0; i < max_queued_events; ++i) {
    const QFileDevice::Permissions permissions = (i / 2) % 2 == 0 ? (QFile::ReadOwner | QFile::WriteOwner) : (QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup);
    ASSERT_TRUE(QFile::setPermissions(filenames_to_change.value(i % 2), permissions));
  }

  ASSERT_TRUE(root.rename(u"A"_s, u"C"_s));
  ASSERT_TRUE(root.rename(u"B"_s, u"A"_s));
  ASSERT_TRUE(root.rename(u"C"_s, u"B"_s));
  watcher.AddPaths(QStringList() << path_a << path_b);

  QSignalSpy spy_path_changed(&watcher, &FileSystemWatcherInterface::PathChanged);
  ASSERT_TRUE(spy_overflow.wait(5000));
  spy_path_changed.clear();

  const QStringList paths = QStringList() << path_a << path_b;
  for (const QString &path : paths) {
    QFile file(QDir(path).filePath(u"01.mp3"_s));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();
    ASSERT_TRUE(spy_path_changed.wait(5000)) << path.toStdString();
    EXPECT_EQ(path, spy_path_changed.last().at(0).toString());
  }

}

}  // namespace
