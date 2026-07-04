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

#ifndef FILEWRITEGUARD_H
#define FILEWRITEGUARD_H

#include "config.h"

#include <QtGlobal>
#include <QString>
#include <QTemporaryFile>

// Helper for writing to files on filesystems that do not support the random, in-place writes TagLib performs when a tag's size changes, currently GVFS mounts (GNOME's virtual filesystem, e.g. /run/user/<uid>/gvfs/... or the older ~/.gvfs/...).
// Those mounts expose backends such as sftp, smb and mtp where the whole file has to be rewritten instead.
// For such files this edits a local temporary copy (with the same extension so TagLib detects the type) and, once TagLib is done, the caller copies it back over the original with a full sequential write via Commit().
// For ordinary files (and other FUSE filesystems like sshfs, which do support in-place writes) it is a no-op passthrough. The temporary file is removed when the guard goes out of scope.
class FileWriteGuard {
 public:
  explicit FileWriteGuard(const QString &filename);

  // Whether the file needs to be edited via a temporary copy.
  bool active() const { return active_; }

  // Prepares the working file (copies the original to a local temporary file when needed). Returns false on failure.
  bool Init();

  // The file TagLib should open (a local temporary copy when needed, otherwise the original).
  const QString &working_filename() const { return working_filename_; }

  // Copies the edited temporary file back over the original. Only call after the TagLib stream has been closed so all data is flushed. A no-op for ordinary files.
  bool Commit();

 private:
  QString filename_;
  QString working_filename_;
  bool active_;
  QTemporaryFile temp_file_;

  Q_DISABLE_COPY(FileWriteGuard)
};

#endif  // FILEWRITEGUARD_H
