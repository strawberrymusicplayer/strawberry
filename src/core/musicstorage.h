/*
 * Strawberry Music Player
 * This file was part of Clementine.
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

#ifndef MUSICSTORAGE_H
#define MUSICSTORAGE_H

#include "config.h"

#include <QtGlobal>

#include <functional>
#include <memory>
#include <optional>

#include <QMetaType>
#include <QString>
#include <QList>
#include <QImage>

#include "includes/shared_ptr.h"
#include "song.h"

class MusicStorage {
 public:
  explicit MusicStorage();
  virtual ~MusicStorage() = default;

  enum Role {
    Role_Storage = Qt::UserRole + 100,
    Role_StorageForceConnect,
    Role_Capacity,
    Role_FreeSpace,
  };

  // Values are saved in the database - don't change
  enum class TranscodeMode {
    Transcode_Always = 1,
    Transcode_Never = 2,
    Transcode_Unsupported = 3,
  };

  using ProgressFunction = std::function<void(float progress)>;

  struct CopyJob {
    CopyJob() : overwrite_(false), remove_original_(false), albumcover_(false) {}
    QString source_;
    QString destination_;
    Song metadata_;
    bool overwrite_;
    bool remove_original_;
    bool albumcover_;
    QString cover_source_;
    QString cover_dest_;
    QImage cover_image_;
    ProgressFunction progress_;
    QString playlist_;
  };

  struct DeleteJob {
    DeleteJob() : use_trash_(false) {}
    Song metadata_;
    bool use_trash_;
  };

  virtual Song::Source source() const = 0;
  virtual QString LocalPath() const { return QString(); }
  virtual std::optional<int> collection_directory_id() const { return std::optional<int>(); }

  virtual TranscodeMode GetTranscodeMode() const { return TranscodeMode::Transcode_Never; }
  virtual Song::FileType GetTranscodeFormat() const { return Song::FileType::Unknown; }
  virtual bool GetSupportedFiletypes(QList<Song::FileType> *ret) { Q_UNUSED(ret); return true; }

  virtual bool StartCopy(QList<Song::FileType> *supported_types) { Q_UNUSED(supported_types); return true; }
  virtual bool CopyToStorage(const CopyJob &job, QString &error_text) = 0;
  virtual bool FinishCopy(bool success, QString &error_text) { Q_UNUSED(error_text); return success; }

  virtual void StartDelete() {}
  virtual bool DeleteFromStorage(const DeleteJob &job) = 0;
  virtual bool FinishDelete(bool success, QString &error_text) { Q_UNUSED(error_text); return success; }

  virtual void Eject() {}

 private:
  Q_DISABLE_COPY(MusicStorage)
};

Q_DECLARE_METATYPE(MusicStorage*)
Q_DECLARE_METATYPE(SharedPtr<MusicStorage>)

#endif  // MUSICSTORAGE_H
