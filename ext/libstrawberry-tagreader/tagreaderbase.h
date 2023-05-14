/* This file is part of Strawberry.
   Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TAGREADERBASE_H
#define TAGREADERBASE_H

#include "config.h"

#include <string>

#include <QByteArray>
#include <QString>

#include "tagreadermessages.pb.h"

/*
 * This class holds all useful methods to read and write tags from/to files.
 * You should not use it directly in the main process but rather use a TagReaderWorker process (using TagReaderClient)
 */
class TagReaderBase {
 public:
  explicit TagReaderBase();
  ~TagReaderBase();

  class Cover {
   public:
    explicit Cover(const QByteArray &_data = QByteArray(), const QString &_mime_type = QString()) : data(_data), mime_type(_mime_type) {}
    QByteArray data;
    QString mime_type;
    QString error;
  };

  virtual bool IsMediaFile(const QString &filename) const = 0;

  virtual bool ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const = 0;
  virtual bool SaveFile(const spb::tagreader::SaveFileRequest &request) const = 0;

  virtual QByteArray LoadEmbeddedArt(const QString &filename) const = 0;
  virtual bool SaveEmbeddedArt(const spb::tagreader::SaveEmbeddedArtRequest &request) const = 0;

  virtual bool SaveSongPlaycountToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const = 0;
  virtual bool SaveSongRatingToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const = 0;

  static float ConvertPOPMRating(const int POPM_rating);
  static int ConvertToPOPMRating(const float rating);

  static Cover LoadCoverFromRequest(const spb::tagreader::SaveFileRequest &request);
  static Cover LoadCoverFromRequest(const spb::tagreader::SaveEmbeddedArtRequest &request);

 private:
  static Cover LoadCoverFromRequest(const QString &song_filename, const QString &cover_filename, QByteArray cover_data, QString cover_mime_type);

  Q_DISABLE_COPY(TagReaderBase)
};

#endif  // TAGREADERBASE_H
