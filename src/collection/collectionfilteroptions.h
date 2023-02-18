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

#ifndef COLLECTIONFILTEROPTIONS_H
#define COLLECTIONFILTEROPTIONS_H

#include <QString>

#include "core/song.h"

class CollectionFilterOptions {
 public:

  explicit CollectionFilterOptions();

  // Filter mode:
  // - use the all songs table
  // - use the duplicated songs view; by duplicated we mean those songs for which the (artist, album, title) tuple is found more than once in the songs table
  // - use the untagged songs view; by untagged we mean those for which at least one of the (artist, album, title) tags is empty
  // Please note that additional filtering based on FTS table (the filter attribute) won't work in Duplicates and Untagged modes.
  enum class FilterMode {
    All,
    Duplicates,
    Untagged
  };

  FilterMode filter_mode() const { return filter_mode_; }
  int max_age() const { return max_age_; }
  QString filter_text() const { return filter_text_; }

  void set_filter_mode(const FilterMode filter_mode) {
    filter_mode_ = filter_mode;
    filter_text_.clear();
  }
  void set_max_age(const int max_age) { max_age_ = max_age; }
  void set_filter_text(const QString &filter_text) {
    filter_mode_ = FilterMode::All;
    filter_text_ = filter_text;
  }

  bool Matches(const Song &song) const;

 private:
  FilterMode filter_mode_;
  int max_age_;
  QString filter_text_;
};

#endif  // COLLECTIONFILTEROPTIONS_H
