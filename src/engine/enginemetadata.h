/*
* Strawberry Music Player
* Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ENGINEMETADATA_H
#define ENGINEMETADATA_H

#include <QMetaType>
#include <QString>
#include <QUrl>

#include "core/song.h"

class EngineMetadata {
 public:
  EngineMetadata();
  enum class Type {
    Any,
    Current,
    Next
  };
  Type type;
  QUrl media_url;
  QUrl stream_url;
  QString title;
  QString artist;
  QString album;
  QString comment;
  QString genre;
  qint64 length;
  int year;
  int track;
  Song::FileType filetype;
  int samplerate;
  int bitdepth;
  int bitrate;
  QString lyrics;
};
Q_DECLARE_METATYPE(EngineMetadata)

#endif  // ENGINEMETADATA_H
