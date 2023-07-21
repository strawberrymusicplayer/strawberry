/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef SMARTPLAYLISTSEARCH_H
#define SMARTPLAYLISTSEARCH_H

#include "config.h"

#include <QList>
#include <QString>
#include <QDataStream>

#include "playlistgenerator.h"
#include "smartplaylistsearchterm.h"

class SmartPlaylistSearch {

 public:
  using TermList = QList<SmartPlaylistSearchTerm>;

  // These values are persisted, so add to the end of the enum only
  enum class SearchType {
    And = 0,
    Or,
    All
  };

  // These values are persisted, so add to the end of the enum only
  enum class SortType {
    Random = 0,
    FieldAsc,
    FieldDesc
  };

  explicit SmartPlaylistSearch();
  explicit SmartPlaylistSearch(const SearchType type, const TermList &terms, const SortType sort_type, const SmartPlaylistSearchTerm::Field sort_field, const int limit = PlaylistGenerator::kDefaultLimit);

  bool is_valid() const;
  bool operator==(const SmartPlaylistSearch &other) const;
  bool operator!=(const SmartPlaylistSearch &other) const { return !(*this == other); }

  SearchType search_type_;
  TermList terms_;
  SortType sort_type_;
  SmartPlaylistSearchTerm::Field sort_field_;
  int limit_;

  // Not persisted, used to alter the behaviour of the query
  QList<int> id_not_in_;
  int first_item_;

  void Reset();
  QString ToSql(const QString &songs_table) const;
};

QDataStream &operator<<(QDataStream &s, const SmartPlaylistSearch &search);
QDataStream &operator>>(QDataStream &s, SmartPlaylistSearch &search);

#endif  // SMARTPLAYLISTSEARCH_H
