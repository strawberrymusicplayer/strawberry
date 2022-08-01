/* This file is part of Strawberry.
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

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

#define QStringFromStdString(x) QString::fromUtf8((x).data(), (x).size())
#define DataCommaSizeFromQString(x) (x).toUtf8().constData(), (x).toUtf8().length()

/*
 * This class holds all useful methods to read and write tags from/to files.
 * You should not use it directly in the main process but rather use a TagReaderWorker process (using TagReaderClient)
 */
class TagReaderBase {
 public:
  explicit TagReaderBase();
  ~TagReaderBase();

  virtual bool IsMediaFile(const QString &filename) const = 0;

  virtual bool ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const = 0;
  virtual bool SaveFile(const QString &filename, const spb::tagreader::SongMetadata &song) const = 0;

  virtual QByteArray LoadEmbeddedArt(const QString &filename) const = 0;
  virtual bool SaveEmbeddedArt(const QString &filename, const QByteArray &data) = 0;

  virtual bool SaveSongPlaycountToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const = 0;
  virtual bool SaveSongRatingToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const = 0;

  static void Decode(const QString &tag, std::string *output);

  static float ConvertPOPMRating(const int POPM_rating);
  static int ConvertToPOPMRating(const float rating);

 protected:
  static const std::string kEmbeddedCover;

  Q_DISABLE_COPY(TagReaderBase)
};

#endif  // TAGREADERBASE_H
