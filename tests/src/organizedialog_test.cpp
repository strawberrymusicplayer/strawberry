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

#include <thread>
#include <atomic>
#include <chrono>

#include "gtest_include.h"

#include <QString>
#include <QStringList>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QUrl>
#include <QVariant>
#include <QTextEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardItem>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/musicstorage.h"
#include "core/filesystemmusicstorage.h"
#include "tagreader/tagreaderclient.h"
#include "organize/organizedialog.h"

#ifdef Q_OS_UNIX
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// clazy:excludeall=non-pod-global-static

namespace {

// Deleting the dialog while the songs are loaded in the background, for example when quitting, stops loading them, and waits for it to stop.
TEST(OrganizeDialogTest, DeletingWhileLoadingStopsLoading) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  QStringList filenames;
  for (int i = 0; i < 2000; ++i) {
    const QString filename = temp_dir.filePath(QStringLiteral("%1.mp3").arg(i));
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    filenames << filename;
  }

  // Reading all the songs takes as long as loading them.
  SharedPtr<TagReaderClient> tagreader_client = make_shared<TagReaderClient>();
  QElapsedTimer timer_read;
  timer_read.start();
  for (const QString &filename : std::as_const(filenames)) {
    Song song;
    ASSERT_TRUE(tagreader_client->ReadFileBlocking(filename, &song).success());
  }
  const qint64 read_msec = timer_read.elapsed();

  ASSERT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  OrganizeDialog *dialog = new OrganizeDialog(make_shared<TaskManager>(), tagreader_client);
  ASSERT_TRUE(dialog->SetFilenames(filenames));
  QElapsedTimer timer_delete;
  timer_delete.start();
  delete dialog;
  const qint64 delete_msec = timer_delete.elapsed();

  // Only the file being read when it was deleted is read.
  EXPECT_LT(delete_msec, read_msec / 4) << "Reading the songs took " << read_msec << " ms.";
  // The pool thread can still finish its own bookkeeping after the loading finished.
  EXPECT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));

}

#ifdef Q_OS_UNIX
// Deleting the dialog while a file is being read waits for the read to finish, and the remaining files are not read.
// The first file is a FIFO, reading it blocks until it's opened for writing, which only succeeds when it's opened for reading, so the dialog is deleted while the file is read.
TEST(OrganizeDialogTest, DeletingWhileReadingWaitsForRead) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  const QString fifo_filename = temp_dir.filePath(u"0.mp3"_s);
  ASSERT_EQ(0, ::mkfifo(QFile::encodeName(fifo_filename).constData(), 0600));
  QStringList filenames = QStringList() << fifo_filename;
  for (int i = 1; i < 2000; ++i) {
    const QString filename = temp_dir.filePath(QStringLiteral("%1.mp3").arg(i));
    ASSERT_TRUE(QFile::copy(u":/audio/strawberry.mp3"_s, filename));
    filenames << filename;
  }

  SharedPtr<TagReaderClient> tagreader_client = make_shared<TagReaderClient>();
  QElapsedTimer timer_read;
  timer_read.start();
  for (int i = 1; i < filenames.count(); ++i) {
    Song song;
    ASSERT_TRUE(tagreader_client->ReadFileBlocking(filenames[i], &song).success());
  }
  const qint64 read_msec = timer_read.elapsed();

  ASSERT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  OrganizeDialog *dialog = new OrganizeDialog(make_shared<TaskManager>(), tagreader_client);
  ASSERT_TRUE(dialog->SetFilenames(filenames));

  // Wait until the FIFO is being read.
  int fifo_fd = -1;
  QElapsedTimer timer_open;
  timer_open.start();
  while (fifo_fd == -1 && timer_open.elapsed() < 10000) {
    fifo_fd = ::open(QFile::encodeName(fifo_filename).constData(), O_WRONLY | O_NONBLOCK);
    if (fifo_fd == -1) std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  // When the FIFO is not read, it's still opened and closed while deleting the dialog, so the FIFO doesn't block the dialog's background read when the read starts later, and the test fails without waiting forever.
  if (fifo_fd == -1) {
    std::atomic<bool> deleted = false;
    std::thread unblock_thread([&fifo_filename, &deleted]() {
      while (!deleted) {
        const int fd = ::open(QFile::encodeName(fifo_filename).constData(), O_WRONLY | O_NONBLOCK);
        if (fd != -1) ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    });
    delete dialog;
    deleted = true;
    unblock_thread.join();
    FAIL() << "The FIFO was not read.";
  }

  // The read finishes when the FIFO is closed.
  static constexpr qint64 kReadMsec = 500;
  std::thread close_thread([fifo_fd]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(kReadMsec));
    ::close(fifo_fd);
  });

  QElapsedTimer timer_delete;
  timer_delete.start();
  delete dialog;
  const qint64 delete_msec = timer_delete.elapsed();
  close_thread.join();

  EXPECT_GE(delete_msec, kReadMsec - 50);
  EXPECT_LT(delete_msec, kReadMsec + (read_msec / 4)) << "Reading the songs took " << read_msec << " ms.";
  EXPECT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));

}
#endif

// Songs are loaded for the files in the background, when other songs are set while loading, the loaded songs must not replace them.
class OrganizeDialogLoadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.isValid());
    ASSERT_TRUE(QDir().mkpath(temp_dir_.filePath(u"collection"_s)));
    tagreader_client_ = make_shared<TagReaderClient>();
    dialog_ = new OrganizeDialog(make_shared<TaskManager>(), tagreader_client_);
    QStandardItem *item = new QStandardItem(u"Collection"_s);
    item->setData(QVariant::fromValue(SharedPtr<MusicStorage>(make_shared<FilesystemMusicStorage>(Song::Source::Collection, temp_dir_.filePath(u"collection"_s)))), MusicStorage::Role_Storage);
    destination_model_.appendRow(item);
    dialog_->SetDestinationModel(&destination_model_);
    QTextEdit *naming = dialog_->findChild<QTextEdit*>(u"naming"_s);
    ASSERT_TRUE(naming);
    naming->setPlainText(u"%title.%extension"_s);
    preview_ = dialog_->findChild<QListWidget*>(u"preview"_s);
    preview_stack_ = dialog_->findChild<QStackedWidget*>(u"preview_stack"_s);
    ASSERT_TRUE(preview_);
    ASSERT_TRUE(preview_stack_);
  }

  void TearDown() override {
    delete dialog_;
  }

  // Many files, so loading them takes much longer than loading the other songs.
  QStringList CreateFiles(const QString &name, const int count) {
    QStringList filenames;
    for (int i = 0; i < count; ++i) {
      const QString filename = temp_dir_.filePath(QStringLiteral("%1-%2.mp3").arg(name).arg(i));
      if (!QFile::copy(u":/audio/strawberry.mp3"_s, filename) || !QFile::setPermissions(filename, QFile::ReadOwner | QFile::WriteOwner)) return QStringList();
      Song song(Song::Source::LocalFile);
      if (!tagreader_client_->ReadFileBlocking(filename, &song).success()) return QStringList();
      song.set_title(QStringLiteral("%1 %2").arg(name).arg(i));
      if (!tagreader_client_->WriteFileBlocking(filename, song, TagReaderClient::SaveOption::Tags, SaveTagCoverData()).success()) return QStringList();
      filenames << filename;
    }
    return filenames;
  }

  // Waits for all the songs to be loaded, and their results to be received.
  static void WaitForLoading() {
    QThreadPool::globalInstance()->waitForDone(30000);
    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
  }

  QStringList PreviewFilenames() const {
    QStringList filenames;
    for (int i = 0; i < preview_->count(); ++i) {
      filenames << QFileInfo(preview_->item(i)->text()).fileName();
    }
    return filenames;
  }

  QTemporaryDir temp_dir_;
  SharedPtr<TagReaderClient> tagreader_client_;
  QStandardItemModel destination_model_;
  OrganizeDialog *dialog_ = nullptr;
  QListWidget *preview_ = nullptr;
  QStackedWidget *preview_stack_ = nullptr;
};

TEST_F(OrganizeDialogLoadTest, FilesSetWhileLoadingReplaceLoadingFiles) {

  const QStringList files_a = CreateFiles(u"A"_s, 2000);
  ASSERT_EQ(2000, files_a.count());
  const QStringList files_b = CreateFiles(u"B"_s, 1);
  ASSERT_EQ(1, files_b.count());

  ASSERT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  dialog_->SetFilenames(files_a);
  dialog_->SetFilenames(files_b);
  WaitForLoading();

  EXPECT_EQ(QStringList() << u"B 0.mp3"_s, PreviewFilenames());
  EXPECT_EQ(u"preview_page"_s, preview_stack_->currentWidget()->objectName());

}

TEST_F(OrganizeDialogLoadTest, SongsSetWhileLoadingReplaceLoadingFiles) {

  const QStringList files_a = CreateFiles(u"A"_s, 2000);
  ASSERT_EQ(2000, files_a.count());
  const QStringList files_b = CreateFiles(u"B"_s, 1);
  ASSERT_EQ(1, files_b.count());
  Song song_b(Song::Source::LocalFile);
  ASSERT_TRUE(tagreader_client_->ReadFileBlocking(files_b.first(), &song_b).success());

  ASSERT_TRUE(QThreadPool::globalInstance()->waitForDone(10000));
  dialog_->SetFilenames(files_a);
  dialog_->SetSongs(SongList() << song_b);
  EXPECT_EQ(QStringList() << u"B 0.mp3"_s, PreviewFilenames());
  WaitForLoading();

  EXPECT_EQ(QStringList() << u"B 0.mp3"_s, PreviewFilenames());
  EXPECT_EQ(u"preview_page"_s, preview_stack_->currentWidget()->objectName());

}

}  // namespace
