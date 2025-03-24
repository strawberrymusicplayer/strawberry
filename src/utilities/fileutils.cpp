/*
 * Strawberry Music Player
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>

#include <memory>

#include <QByteArray>
#include <QString>
#include <QIODevice>
#include <QDir>
#include <QFile>

#include "core/logging.h"
#include "includes/scoped_ptr.h"

#include "fileutils.h"

namespace Utilities {

using std::unique_ptr;

QByteArray ReadDataFromFile(const QString &filename) {

  QFile file(filename);
  QByteArray data;
  if (file.open(QIODevice::ReadOnly)) {
    data = file.readAll();
    file.close();
  }
  else {
    qLog(Error) << "Failed to open file" << filename << "for reading:" << file.errorString();
  }
  return data;

}

bool Copy(QIODevice *source, QIODevice *destination) {

  if (!source->open(QIODevice::ReadOnly)) return false;

  if (!destination->open(QIODevice::WriteOnly)) return false;

  const qint64 bytes = source->size();
  unique_ptr<char[]> data(new char[static_cast<size_t>(bytes)]);
  qint64 pos = 0;

  qint64 bytes_read = 0;
  do {
    bytes_read = source->read(data.get() + pos, bytes - pos);
    if (bytes_read == -1) return false;

    pos += bytes_read;
  } while (bytes_read > 0 && pos != bytes);

  pos = 0;
  qint64 bytes_written = 0;
  do {
    bytes_written = destination->write(data.get() + pos, bytes - pos);
    if (bytes_written == -1) return false;

    pos += bytes_written;
  } while (bytes_written > 0 && pos != bytes);

  return true;

}

bool CopyRecursive(const QString &source, const QString &destination) {

  // Make the destination directory
  QString dir_name = source.section(u'/', -1, -1);
  QString dest_path = destination + QLatin1Char('/') + dir_name;
  QDir().mkpath(dest_path);

  QDir dir(source);
  const QStringList children_dirs = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
  for (const QString &child : children_dirs) {
    if (!CopyRecursive(source + QLatin1Char('/') + child, dest_path)) {
      qLog(Warning) << "Failed to copy dir" << source + QLatin1Char('/') + child << "to" << dest_path;
      return false;
    }
  }

  const QStringList children_files = dir.entryList(QDir::NoDotAndDotDot | QDir::Files);
  for (const QString &child : children_files) {
    if (!QFile::copy(source + QLatin1Char('/') + child, dest_path + QLatin1Char('/') + child)) {
      qLog(Warning) << "Failed to copy file" << source + QLatin1Char('/') + child << "to" << dest_path;
      return false;
    }
  }

  return true;

}

bool RemoveRecursive(const QString &path) {

  QDir dir(path);
  const QStringList children_dirs = dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Hidden);
  for (const QString &child : children_dirs) {
    if (!RemoveRecursive(path + QLatin1Char('/') + child)) {
      return false;
    }
  }

  const QStringList children_files = dir.entryList(QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden);
  for (const QString &child : children_files) {
    if (!QFile::remove(path + QLatin1Char('/') + child)) {
      return false;
    }
  }

  return dir.rmdir(path);

}

}  // namespace Utilities
