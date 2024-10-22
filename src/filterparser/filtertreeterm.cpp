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

#include "filtertreeterm.h"
#include "filterparsersearchtermcomparator.h"

FilterTreeTerm::FilterTreeTerm(FilterParserSearchTermComparator *comparator) : cmp_(comparator) {}

bool FilterTreeTerm::accept(const Song &song) const {

  if (cmp_->Matches(song.PrettyTitle())) return true;
  if (cmp_->Matches(song.album())) return true;
  if (cmp_->Matches(song.artist())) return true;
  if (cmp_->Matches(song.albumartist())) return true;
  if (cmp_->Matches(song.composer())) return true;
  if (cmp_->Matches(song.performer())) return true;
  if (cmp_->Matches(song.grouping())) return true;
  if (cmp_->Matches(song.genre())) return true;
  if (cmp_->Matches(song.comment())) return true;

  return false;

}
