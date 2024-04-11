/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/song.h"

#include "radiochannel.h"

Song RadioChannel::ToSong() const {

  Song song(source);
  song.set_valid(true);
  song.set_filetype(Song::FileType::Stream);
  song.set_title(Song::DescriptionForSource(source) + QLatin1Char(' ') + name);
  song.set_url(url);

  return song;

}
