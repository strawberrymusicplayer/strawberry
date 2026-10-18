/*
 * Strawberry Music Player
 * Copyright 2024-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QString>
#include <QFile>
#include <QRandomGenerator>

#include "core/logging.h"

#include "temporaryfile.h"

using namespace Qt::Literals::StringLiterals;

TemporaryFile::TemporaryFile(const QString &filename_pattern) {

  int i = 0;
  do {
    filename_ = GenerateFilename(filename_pattern);
    ++i;
  } while (QFile::exists(filename_) && i < 100);

  if (QFile::exists(filename_)) {
    qLog(Error) << "Could not get a filename from pattern" << filename_pattern;
    filename_.clear();
  }
  else {
    qLog(Debug) << "Temporary file" << filename_ << "available";
  }

}

TemporaryFile::~TemporaryFile() {

  if (!filename_.isEmpty() && QFile::exists(filename_)) {
    qLog(Debug) << "Deleting temporary file" << filename_;
    if (!QFile::remove(filename_)) {
      qLog(Debug) << "Could not delete temporary file" << filename_;
    }
  }

}

QString TemporaryFile::GenerateFilename(const QString &filename_pattern) const {

  static const QString random_chars = u"abcdefghijklmnopqrstuvwxyz0123456789"_s;

  QString filename = filename_pattern;

  // Only replace the last run of X characters in the file name, the directory can also contain X characters, like a home directory of a user named Xavier.
  // A backslash is only a directory separator on Windows, elsewhere it can be part of the file name.
#ifdef Q_OS_WIN32
  const qsizetype filename_start = std::max(filename.lastIndexOf(u'/'), filename.lastIndexOf(u'\\')) + 1;
#else
  const qsizetype filename_start = filename.lastIndexOf(u'/') + 1;
#endif
  const qsizetype template_end = filename.lastIndexOf(u'X');
  if (template_end < filename_start) {
    return filename;
  }

  for (qsizetype i = template_end; i >= filename_start && filename.at(i) == u'X'; --i) {
    const qint64 index = QRandomGenerator::global()->bounded(0, random_chars.length());
    filename[i] = random_chars.at(index);
  }

  return filename;

}
