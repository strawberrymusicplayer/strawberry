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

#include <cstdio>

#include <QtGlobal>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QString>

#include "core/logging.h"
#include "utilities/fileutils.h"
#include "filewriteguard.h"

using namespace Qt::Literals::StringLiterals;

FileWriteGuard::FileWriteGuard(const QString &filename)
    : filename_(filename),
      working_filename_(filename),
      active_(Utilities::FilenameOnGVFS(filename)) {}

bool FileWriteGuard::Init() {

  if (!active_) return true;

  temp_file_.setFileTemplate(QDir::tempPath() + "/strawberry_tag_XXXXXX."_L1 + QFileInfo(filename_).suffix());
  if (!temp_file_.open()) {
    qLog(Error) << "Failed to create temporary file for" << filename_ << ":" << temp_file_.errorString();
    return false;
  }
  working_filename_ = temp_file_.fileName();
  temp_file_.close();

  qLog(Debug) << "File" << filename_ << "is on a GVFS mount, editing via temporary file" << working_filename_;

  return Utilities::CopyFileContents(filename_, working_filename_);

}

bool FileWriteGuard::Commit() {

  if (!active_) return true;

  // Write to a temporary file in the same directory as the destination and then atomically rename it into place.
  // Copying directly over the destination would truncate it up front and leave it corrupted if the write failed partway; renaming means the original is only ever replaced by a complete file.
  // active_ is only set for GVFS paths (Linux), so std::rename here always operates on POSIX, where it atomically replaces an existing destination on the same filesystem.
  const QFileInfo fileinfo(filename_);

  // Capture the original file's permissions so they can be restored: QTemporaryFile creates the temporary file with restrictive (owner-only) permissions, and it would otherwise carry those over to the destination once renamed into place.
  const QFile::Permissions permissions = QFile::permissions(filename_);

  QTemporaryFile temp_file(fileinfo.absolutePath() + "/.strawberry_tag_XXXXXX"_L1);
  if (!temp_file.open()) {
    qLog(Error) << "Failed to create temporary file next to" << filename_ << ":" << temp_file.errorString();
    return false;
  }
  const QString temp_filename = temp_file.fileName();
  temp_file.close();

  if (!Utilities::CopyFileContents(working_filename_, temp_filename)) {
    return false;
  }

  // Restore the destination's original permissions on the temporary file before it replaces the destination.
  if (permissions != QFile::Permissions() && !QFile::setPermissions(temp_filename, permissions)) {
    qLog(Warning) << "Failed to restore permissions on" << temp_filename;
  }

  if (std::rename(QFile::encodeName(temp_filename).constData(), QFile::encodeName(filename_).constData()) != 0) {
    qLog(Error) << "Failed to rename temporary file" << temp_filename << "to" << filename_;
    return false;
  }

  // The temporary file has been renamed over the destination, so don't try to remove it on destruction.
  temp_file.setAutoRemove(false);

  return true;

}
