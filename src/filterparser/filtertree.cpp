/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include "filtercolumn.h"
#include "core/song.h"

using namespace Qt::Literals::StringLiterals;

FilterTree::FilterTree() = default;
FilterTree::~FilterTree() = default;

QVariant FilterTree::DataFromColumn(const FilterColumn filter_column, const Song &song) {

  switch (filter_column) {
    case FilterColumn::AlbumArtist:
      return song.effective_albumartist();
    case FilterColumn::AlbumArtistSort:
      return song.effective_albumartistsort();
    case FilterColumn::Artist:
      return song.artist();
    case FilterColumn::ArtistSort:
      return song.effective_artistsort();
    case FilterColumn::Album:
      return song.album();
    case FilterColumn::AlbumSort:
      return song.effective_albumsort();
    case FilterColumn::Title:
      return song.PrettyTitle();
    case FilterColumn::TitleSort:
      return song.effective_titlesort();
    case FilterColumn::Composer:
      return song.composer();
    case FilterColumn::ComposerSort:
      return song.effective_composersort();
    case FilterColumn::Performer:
      return song.performer();
    case FilterColumn::PerformerSort:
      return song.effective_performersort();
    case FilterColumn::Grouping:
      return song.grouping();
    case FilterColumn::Genre:
      return song.genre();
    case FilterColumn::Comment:
      return song.comment();
    case FilterColumn::Track:
      return song.track();
    case FilterColumn::Year:
      return song.year();
    case FilterColumn::Length:
      return song.length_nanosec();
    case FilterColumn::Samplerate:
      return song.samplerate();
    case FilterColumn::Bitdepth:
      return song.bitdepth();
    case FilterColumn::Bitrate:
      return song.bitrate();
    case FilterColumn::Rating:
      return song.rating();
    case FilterColumn::Playcount:
      return song.playcount();
    case FilterColumn::Skipcount:
      return song.skipcount();
    case FilterColumn::Filename:
      return song.basefilename();
    case FilterColumn::URL:
      return song.effective_url().toString();
    case FilterColumn::Unknown:
      break;
  }

  return QVariant();

}
