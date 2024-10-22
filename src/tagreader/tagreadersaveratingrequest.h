/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERSAVERATINGREQUEST_H
#define TAGREADERSAVERATINGREQUEST_H

#include <QString>

#include "includes/shared_ptr.h"
#include "tagreaderrequest.h"

using std::make_shared;

class TagReaderSaveRatingRequest : public TagReaderRequest {
 public:
  explicit TagReaderSaveRatingRequest(const QString &_filename);
  static SharedPtr<TagReaderSaveRatingRequest> Create(const QString &filename) { return make_shared<TagReaderSaveRatingRequest>(filename); }
  float rating;
};

using TagReaderSaveRatingRequestPtr = SharedPtr<TagReaderSaveRatingRequest>;

#endif  // TAGREADERSAVERATINGREQUEST_H
