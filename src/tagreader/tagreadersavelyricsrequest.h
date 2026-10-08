/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERSAVELYRICSREQUEST_H
#define TAGREADERSAVELYRICSREQUEST_H

#include <QString>

#include "includes/shared_ptr.h"
#include "tagreaderrequest.h"

using std::make_shared;

class TagReaderSaveLyricsRequest : public TagReaderRequest {
 public:
  explicit TagReaderSaveLyricsRequest(const QString &_filename);
  static SharedPtr<TagReaderSaveLyricsRequest> Create(const QString &filename) { return make_shared<TagReaderSaveLyricsRequest>(filename); }
  QString lyrics;
  bool synced;  // Whether the lyrics are synchronized lyrics in LRC format.
};

using TagReaderSaveLyricsRequestPtr = SharedPtr<TagReaderSaveLyricsRequest>;

#endif  // TAGREADERSAVELYRICSREQUEST_H
