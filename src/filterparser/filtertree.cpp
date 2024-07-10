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

FilterTree::FilterTree() = default;
FilterTree::~FilterTree() = default;

QVariant FilterTree::DataFromColumn(const QString &column, const Song &metadata) {

  if (column == QLatin1String("albumartist")) return metadata.effective_albumartist();
  if (column == QLatin1String("artist"))      return metadata.artist();
  if (column == QLatin1String("album"))       return metadata.album();
  if (column == QLatin1String("title"))       return metadata.title();
  if (column == QLatin1String("composer"))    return metadata.composer();
  if (column == QLatin1String("performer"))   return metadata.performer();
  if (column == QLatin1String("grouping"))    return metadata.grouping();
  if (column == QLatin1String("genre"))       return metadata.genre();
  if (column == QLatin1String("comment"))     return metadata.comment();
  if (column == QLatin1String("track"))       return metadata.track();
  if (column == QLatin1String("year"))        return metadata.year();
  if (column == QLatin1String("length"))      return metadata.length_nanosec();
  if (column == QLatin1String("samplerate"))  return metadata.samplerate();
  if (column == QLatin1String("bitdepth"))    return metadata.bitdepth();
  if (column == QLatin1String("bitrate"))     return metadata.bitrate();
  if (column == QLatin1String("rating"))      return metadata.rating();
  if (column == QLatin1String("playcount"))   return metadata.playcount();
  if (column == QLatin1String("skipcount"))   return metadata.skipcount();

  return QVariant();

}
