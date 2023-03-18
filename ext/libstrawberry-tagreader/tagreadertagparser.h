/* This file is part of Strawberry.
   Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>

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

#ifndef TAGREADERTAGPARSER_H
#define TAGREADERTAGPARSER_H

#include "config.h"

#include <string>

#include <tagparser/tag.h>

#include <QByteArray>
#include <QString>

#include "tagreadermessages.pb.h"
#include "tagreaderbase.h"

/*
 * This class holds all useful methods to read and write tags from/to files.
 * You should not use it directly in the main process but rather use a TagReaderWorker process (using TagReaderClient)
 */
class TagReaderTagParser : public TagReaderBase {
 public:
  explicit TagReaderTagParser();
  ~TagReaderTagParser();

  bool IsMediaFile(const QString &filename) const override;

  bool ReadFile(const QString &filename, spb::tagreader::SongMetadata *song) const override;
  bool SaveFile(const spb::tagreader::SaveFileRequest &request) const override;

  QByteArray LoadEmbeddedArt(const QString &filename) const override;
  bool SaveEmbeddedArt(const spb::tagreader::SaveEmbeddedArtRequest &request) const override;

  bool SaveSongPlaycountToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const override;
  bool SaveSongRatingToFile(const QString &filename, const spb::tagreader::SongMetadata &song) const override;

 private:
  void SaveSongPlaycountToFile(TagParser::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SaveSongRatingToFile(TagParser::Tag *tag, const spb::tagreader::SongMetadata &song) const;
  void SaveEmbeddedArt(TagParser::Tag *tag, const QByteArray &data) const;

 public:
  Q_DISABLE_COPY(TagReaderTagParser)
};

#endif  // TAGREADERTAGPARSER_H
