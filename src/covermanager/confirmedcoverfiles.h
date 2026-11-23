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

#ifndef CONFIRMEDCOVERFILES_H
#define CONFIRMEDCOVERFILES_H

#include <QMap>
#include <QByteArray>
#include <QString>
#include <QStringList>

// The cover files the user confirmed deleting, with a hash of the content of each file when the user was asked.
// A file which changed since then, like a cover saved to the same file while the user was asked, is another cover, so it's not deleted.
// The content is compared, since a file can change without changing its size and modification time, like on file systems where the modification time is only in seconds.
class ConfirmedCoverFiles {
 public:
  ConfirmedCoverFiles() = default;
  explicit ConfirmedCoverFiles(const QStringList &filenames);

  // Returns true when the user confirmed deleting the file, and the file is unchanged since.
  // A file which can't be read is not handled as unchanged.
  bool contains(const QString &filename) const;

  // The same with the hash of the content of the file from ContentHash(), so a file shared by many songs is only read once.
  bool contains(const QString &filename, const QByteArray &content_hash) const;

  // Returns an empty hash when the file can't be read.
  static QByteArray ContentHash(const QString &filename);

 private:

  QMap<QString, QByteArray> files_;
};

#endif  // CONFIRMEDCOVERFILES_H
