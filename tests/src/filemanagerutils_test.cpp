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

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QUrl>
#include <QList>
#include <QTemporaryDir>
#include <QDesktopServices>
#include <QElapsedTimer>
#include <QThread>

#include "utilities/filemanagerutils.h"
#include "test_utils.h"

using namespace Qt::Literals::StringLiterals;

namespace {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)

// Runs OpenInFileBrowser() with a fake xdg-mime and fake file managers, which record how they were started, and records the URLs opened with QDesktopServices.
class FileManagerUtilsTest : public ::testing::Test {
 protected:
  void SetUp() override {

    ASSERT_TRUE(temp_dir_.isValid());
    root_ = temp_dir_.path();
    for (const QString &dir : {u"bin"_s, u"fm"_s, u"data/applications"_s, u"empty"_s, u"music/a"_s, u"music/b"_s}) {
      ASSERT_TRUE(QDir(root_).mkpath(dir));
    }
    for (const QString &file : {u"music/a/1.flac"_s, u"music/b/2.flac"_s}) {
      QFile f(root_ + u'/' + file);
      ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    }

    SaveEnv("PATH");
    SaveEnv("XDG_DATA_HOME");
    SaveEnv("XDG_DATA_DIRS");
    qputenv("PATH", QByteArray(QString(root_ + u"/bin:"_s).toLocal8Bit() + saved_env_.value(0).second));
    qputenv("XDG_DATA_HOME", QString(root_ + u"/data"_s).toLocal8Bit());
    qputenv("XDG_DATA_DIRS", QString(root_ + u"/empty"_s).toLocal8Bit());

    WriteXdgMime(u"echo test-fm.desktop"_s);
    QDesktopServices::setUrlHandler(u"file"_s, &url_handler_, "OpenUrl");

  }

  void TearDown() override {

    QDesktopServices::unsetUrlHandler(u"file"_s);
    for (const QPair<QByteArray, QByteArray> &env : std::as_const(saved_env_)) {
      if (env.second.isNull()) qunsetenv(env.first.constData());
      else qputenv(env.first.constData(), env.second);
    }

  }

  void SaveEnv(const char *name) {
    saved_env_ << qMakePair(QByteArray(name), qEnvironmentVariableIsSet(name) ? qgetenv(name) : QByteArray());
  }

  static void WriteScript(const QString &filename, const QString &body) {
    QFile file(filename);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(QString(u"#!/bin/sh\n"_s + body + u'\n').toUtf8());
    file.close();
    ASSERT_TRUE(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
  }

  // The fake xdg-mime records each call, then runs the given command.
  void WriteXdgMime(const QString &command) const {
    WriteScript(root_ + u"/bin/xdg-mime"_s, u"echo call >> '%1/xdg-mime-calls'\n%2"_s.arg(root_, command));
  }

  void WriteDesktopFile(const QString &exec) const {
    QFile file(root_ + u"/data/applications/test-fm.desktop"_s);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write(QString(u"[Desktop Entry]\nType=Application\nName=Test\nExec="_s + exec + u'\n').toUtf8());
  }

  // A fake file manager in a directory other than /usr/bin, which writes its arguments, one per line, for each time it is started.
  QString WriteFileManager(const QString &name) const {
    const QString filename = root_ + u"/fm/"_s + name;
    WriteScript(filename, u"printf '%s\\n' \"$@\" --- >> '%1/fm-args'"_s.arg(root_));
    return filename;
  }

  // Returns the arguments of each start of the fake file managers, waiting until there are the given number of starts.
  QList<QStringList> WaitForFileManagerStarts(const int count) const {
    QElapsedTimer timer;
    timer.start();
    QList<QStringList> starts;
    while (timer.elapsed() < 5000) {
      starts.clear();
      QFile file(root_ + u"/fm-args"_s);
      if (file.open(QIODevice::ReadOnly)) {
        QStringList args;
        const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
          if (line == "---"_L1) {
            starts << args;
            args.clear();
          }
          else {
            args << line;
          }
        }
      }
      if (starts.count() >= count) break;
      QThread::msleep(20);
    }
    return starts;
  }

  int XdgMimeCalls() const {
    QFile file(root_ + u"/xdg-mime-calls"_s);
    if (!file.open(QIODevice::ReadOnly)) return 0;
    return static_cast<int>(file.readAll().count('\n'));
  }

  QString FilePath(const QString &file) const { return root_ + u"/music/"_s + file; }
  QUrl FileUrl(const QString &file) const { return QUrl::fromLocalFile(FilePath(file)); }
  QUrl DirectoryUrl(const QString &file) const { return QUrl::fromLocalFile(QFileInfo(FilePath(file)).dir().path()); }

  QTemporaryDir temp_dir_;
  QString root_;
  QList<QPair<QByteArray, QByteArray>> saved_env_;
  TestUrlHandler url_handler_;
};

// Regression test: only file managers in /usr/bin were recognized, others were started with the music file instead of selecting it.
TEST_F(FileManagerUtilsTest, KnownFileManagerInOtherDirectorySelectsFile) {

  WriteDesktopFile(WriteFileManager(u"dolphin"_s) + u" --new-window %U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  const QList<QStringList> starts = WaitForFileManagerStarts(1);
  ASSERT_EQ(1, starts.count());
  EXPECT_EQ(QStringList() << u"--new-window"_s << u"--select"_s << FilePath(u"a/1.flac"_s), starts.first());
  EXPECT_TRUE(url_handler_.urls().isEmpty());

}

TEST_F(FileManagerUtilsTest, FileManagerWithDirectoryArgumentOpensDirectory) {

  WriteDesktopFile(WriteFileManager(u"thunar"_s) + u" %F"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  const QList<QStringList> starts = WaitForFileManagerStarts(1);
  ASSERT_EQ(1, starts.count());
  EXPECT_EQ(QStringList() << DirectoryUrl(u"a/1.flac"_s).toLocalFile(), starts.first());

}

// Regression test: the default file manager was queried with xdg-mime for each directory.
TEST_F(FileManagerUtilsTest, DefaultFileManagerIsQueriedOnce) {

  WriteDesktopFile(WriteFileManager(u"nautilus"_s) + u" %U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s) << FileUrl(u"b/2.flac"_s));

  EXPECT_EQ(2, WaitForFileManagerStarts(2).count());
  EXPECT_EQ(1, XdgMimeCalls());

}

// Regression test: kfmclient was given --select, which it does not have, so it exited without opening anything.
TEST_F(FileManagerUtilsTest, KfmclientOpensDirectory) {

  WriteDesktopFile(WriteFileManager(u"kfmclient"_s) + u" openURL %u"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  EXPECT_EQ(QList<QUrl>() << DirectoryUrl(u"a/1.flac"_s), url_handler_.urls());
  QThread::msleep(200);
  EXPECT_FALSE(QFile::exists(root_ + u"/fm-args"_s));

}

// Regression test: an Exec with only field codes crashed, since there was no program.
TEST_F(FileManagerUtilsTest, ExecWithOnlyFieldCodesOpensDirectory) {

  WriteDesktopFile(u"%U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  EXPECT_EQ(QList<QUrl>() << DirectoryUrl(u"a/1.flac"_s), url_handler_.urls());

}

// Regression test: file managers which are not known were started with the music file.
TEST_F(FileManagerUtilsTest, UnknownFileManagerOpensDirectory) {

  WriteDesktopFile(WriteFileManager(u"nemo"_s) + u" %U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  EXPECT_EQ(QList<QUrl>() << DirectoryUrl(u"a/1.flac"_s), url_handler_.urls());
  QThread::msleep(200);
  EXPECT_FALSE(QFile::exists(root_ + u"/fm-args"_s));

}

// Regression test: nothing happened when the file manager could not be started.
TEST_F(FileManagerUtilsTest, FileManagerWhichCannotBeStartedOpensDirectory) {

  WriteDesktopFile(root_ + u"/missing/nautilus %U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  EXPECT_EQ(QList<QUrl>() << DirectoryUrl(u"a/1.flac"_s), url_handler_.urls());

}

// Regression test: xdg-mime was waited for up to 30 seconds for each directory.
TEST_F(FileManagerUtilsTest, XdgMimeWhichDoesNotFinishOpensDirectory) {

  WriteDesktopFile(WriteFileManager(u"nautilus"_s) + u" %U"_s);
  WriteXdgMime(u"exec sleep 30"_s);

  QElapsedTimer timer;
  timer.start();
  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/1.flac"_s));

  EXPECT_LT(timer.elapsed(), 15000);
  EXPECT_EQ(QList<QUrl>() << DirectoryUrl(u"a/1.flac"_s), url_handler_.urls());

}

// Regression test: the default file manager was queried even when there were no directories to open.
TEST_F(FileManagerUtilsTest, NoDirectoriesDoesNotQueryDefaultFileManager) {

  WriteDesktopFile(WriteFileManager(u"nautilus"_s) + u" %U"_s);

  Utilities::OpenInFileBrowser(QList<QUrl>() << FileUrl(u"a/missing.flac"_s) << QUrl(u"https://www.strawberrymusicplayer.org/song.flac"_s));

  EXPECT_EQ(0, XdgMimeCalls());
  EXPECT_TRUE(url_handler_.urls().isEmpty());

}

#endif

}  // namespace
