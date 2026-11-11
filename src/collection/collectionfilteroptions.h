/*
 * Strawberry Music Player
 * Copyright 2023-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QString>

#include "core/song.h"

class CollectionFilterOptions {
 public:
  explicit CollectionFilterOptions();

  // Filter mode:
  // - all songs
  // - duplicated songs; by duplicated we mean those songs for which the (artist, album, title) tuple is found more than once in the collection, CollectionFilter finds these since it depends on the other songs
  // - untagged songs; by untagged we mean those for which at least one of the (artist, album, title) tags is empty
  enum class FilterMode {
    All,
    Duplicates,
    Untagged
  };

  FilterMode filter_mode() const { return filter_mode_; }
  int max_age() const { return max_age_; }
  float min_rating() const { return min_rating_; }
  QString filter_text() const { return filter_text_; }

  void set_filter_mode(const FilterMode filter_mode) {
    filter_mode_ = filter_mode;
    filter_text_.clear();
  }
  // The songs are compared to a cutoff time, so the same songs match each time they're checked.
  // Setting the age sets the cutoff from the current time, CollectionFilter moves it forward when songs become older than the age.
  void set_max_age(const int max_age);
  qint64 max_age_cutoff() const { return max_age_cutoff_; }
  void set_max_age_cutoff(const qint64 max_age_cutoff) { max_age_cutoff_ = max_age_cutoff; }
  void set_min_rating(const float min_rating) { min_rating_ = min_rating; }
  void set_filter_text(const QString &filter_text) {
    filter_mode_ = FilterMode::All;
    filter_text_ = filter_text;
  }

  bool Matches(const Song &song) const;

 private:
  FilterMode filter_mode_;
  int max_age_;
  qint64 max_age_cutoff_;
  float min_rating_;
  QString filter_text_;
};

#endif  // COLLECTIONFILTEROPTIONS_H
