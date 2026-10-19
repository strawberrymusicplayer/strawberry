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
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTemporaryDir>
#include <QRegularExpression>

#include "core/temporaryfile.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Regression test: every X character in the path was replaced, also in the directory, so the file could not be created when the directory had an X in its name.
TEST(TemporaryFileTest, OnlyFilenameTemplateIsReplaced) {

  QTemporaryDir temp_dir(QDir::tempPath() + u"/Xavier-XXXXXX"_s);
  ASSERT_TRUE(temp_dir.isValid());
  ASSERT_TRUE(QDir(temp_dir.path()).dirName().startsWith("Xavier-"_L1));

  QString filename;
  {
    TemporaryFile temporary_file(temp_dir.path() + u"/Xtrack-XXXXXX.jpg"_s);
    filename = temporary_file.filename();
    ASSERT_FALSE(filename.isEmpty());

    const QFileInfo fileinfo(filename);
    EXPECT_EQ(QDir(temp_dir.path()).absolutePath(), fileinfo.absolutePath());
    EXPECT_TRUE(QRegularExpression(u"^Xtrack-[a-z0-9]{6}\\.jpg$"_s).match(fileinfo.fileName()).hasMatch()) << fileinfo.fileName().toStdString();

    QFile file(filename);
    EXPECT_TRUE(file.open(QIODevice::WriteOnly));
    file.close();
  }

  // The file is removed when the temporary file is destroyed.
  EXPECT_FALSE(QFile::exists(filename));

}

TEST(TemporaryFileTest, FilenameWithoutTemplateIsKept) {

  QTemporaryDir temp_dir(QDir::tempPath() + u"/Xavier-XXXXXX"_s);
  ASSERT_TRUE(temp_dir.isValid());

  TemporaryFile temporary_file(temp_dir.path() + u"/cover.jpg"_s);
  EXPECT_EQ(temp_dir.path() + u"/cover.jpg"_s, temporary_file.filename());

}

#ifndef Q_OS_WIN32
// A backslash is part of the file name on POSIX systems, so the template before it is replaced.
TEST(TemporaryFileTest, BackslashIsPartOfFilename) {

  QTemporaryDir temp_dir;
  ASSERT_TRUE(temp_dir.isValid());

  TemporaryFile temporary_file(temp_dir.path() + u"/reportXXX\\draft.txt"_s);
  const QFileInfo fileinfo(temporary_file.filename());
  EXPECT_EQ(QDir(temp_dir.path()).absolutePath(), fileinfo.absolutePath());
  EXPECT_TRUE(QRegularExpression(u"^report[a-z0-9]{3}\\\\draft\\.txt$"_s).match(fileinfo.fileName()).hasMatch()) << fileinfo.fileName().toStdString();

}
#endif

}  // namespace
