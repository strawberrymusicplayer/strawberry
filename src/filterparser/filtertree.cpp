/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>

#include "filtertree.h"

#include "core/song.h"

using namespace Qt::Literals::StringLiterals;

FilterTree::FilterTree() = default;
FilterTree::~FilterTree() = default;

QVariant FilterTree::DataFromColumn(const QString &column, const Song &metadata) {

  if (column == "albumartist"_L1) return metadata.effective_albumartist();
  if (column == "artist"_L1)      return metadata.artist();
  if (column == "album"_L1)       return metadata.album();
  if (column == "title"_L1)       return metadata.PrettyTitle();
  if (column == "composer"_L1)    return metadata.composer();
  if (column == "performer"_L1)   return metadata.performer();
  if (column == "grouping"_L1)    return metadata.grouping();
  if (column == "genre"_L1)       return metadata.genre();
  if (column == "comment"_L1)     return metadata.comment();
  if (column == "track"_L1)       return metadata.track();
  if (column == "year"_L1)        return metadata.year();
  if (column == "length"_L1)      return metadata.length_nanosec();
  if (column == "samplerate"_L1)  return metadata.samplerate();
  if (column == "bitdepth"_L1)    return metadata.bitdepth();
  if (column == "bitrate"_L1)     return metadata.bitrate();
  if (column == "rating"_L1)      return metadata.rating();
  if (column == "playcount"_L1)   return metadata.playcount();
  if (column == "skipcount"_L1)   return metadata.skipcount();
  if (column == "filename"_L1)    return metadata.basefilename();
  if (column == "url"_L1)         return metadata.effective_url().toString();

  return QVariant();

}
