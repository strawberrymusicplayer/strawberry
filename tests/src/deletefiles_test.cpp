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

#include <atomic>

#include <QObject>
#include <QThread>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QTemporaryDir>
#include <QEventLoop>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/deletefiles.h"
#include "core/filesystemmusicstorage.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static,returning-void-expression

namespace {

// DeleteFiles deletes itself in the original thread when it finished, and its thread is deleted when the thread finished.
// The deletion can't be timed, so it's repeated, the thread being deleted while it's still running aborts.
TEST(DeleteFilesTest, DeletesItselfAndThreadWhenFinished) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  SharedPtr<TaskManager> task_manager = make_shared<TaskManager>();
  SharedPtr<MusicStorage> storage = make_shared<FilesystemMusicStorage>(Song::Source::Collection, temp_dir.path());

  for (int i = 0; i < 100; ++i) {

    const QString filename = temp_dir.filePath(u"%1.flac"_s.arg(i));
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();

    DeleteFiles *delete_files = new DeleteFiles(task_manager, storage, false);

    std::atomic<bool> thread_finished = false;
    bool thread_finished_before_deleted = false;
    bool thread_deleted = false;
    bool delete_files_deleted = false;

    QEventLoop loop;
    const auto quit_when_deleted = [&loop, &thread_deleted, &delete_files_deleted]() {
      if (thread_deleted && delete_files_deleted) loop.quit();
    };

    // Finished is emitted in the thread of DeleteFiles, before it finishes.
    QObject::connect(delete_files, &DeleteFiles::Finished, delete_files, [&]() {
      QThread *thread = QThread::currentThread();
      QObject::connect(thread, &QThread::finished, thread, [&thread_finished]() { thread_finished = true; }, Qt::DirectConnection);
      QObject::connect(thread, &QObject::destroyed, &loop, [&]() {
        thread_finished_before_deleted = thread_finished;
        thread_deleted = true;
        quit_when_deleted();
      }, Qt::DirectConnection);
    }, Qt::DirectConnection);
    QObject::connect(delete_files, &QObject::destroyed, &loop, [&]() {
      delete_files_deleted = true;
      quit_when_deleted();
    });

    delete_files->Start(QStringList() << filename);

    QTimer::singleShot(5000, &loop, &QEventLoop::quit);
    loop.exec();

    ASSERT_TRUE(delete_files_deleted);
    ASSERT_TRUE(thread_deleted);
    EXPECT_TRUE(thread_finished_before_deleted);
    EXPECT_FALSE(QFileInfo::exists(filename));

  }

}

}  // namespace
