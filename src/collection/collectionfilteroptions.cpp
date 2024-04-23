/*
 * Strawberry Music Player
 * Copyright 2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QDateTime>

#include "core/song.h"

#include "collectionfilteroptions.h"

CollectionFilterOptions::CollectionFilterOptions() : filter_mode_(FilterMode::All), max_age_(-1) {}

bool CollectionFilterOptions::Matches(const Song &song) const {

  if (max_age_ != -1) {
    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - max_age_;
    if (song.ctime() <= cutoff) return false;
  }

  if (!filter_text_.isNull()) {
    return song.albumartist().contains(filter_text_, Qt::CaseInsensitive) || song.artist().contains(filter_text_, Qt::CaseInsensitive) || song.album().contains(filter_text_, Qt::CaseInsensitive) || song.title().contains(filter_text_, Qt::CaseInsensitive);
  }

  return true;

}
