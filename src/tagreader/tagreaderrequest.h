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

#ifndef TAGREADERREQUEST_H
#define TAGREADERREQUEST_H

#include <QString>

#include "core/shared_ptr.h"
#include "tagreaderreply.h"

class TagReaderRequest {
 public:
  explicit TagReaderRequest(const QString &_filename);
  virtual ~TagReaderRequest();
  QString filename;
  TagReaderReplyPtr reply;
};

using TagReaderRequestPtr = SharedPtr<TagReaderRequest>;

#endif  // TAGREADERREQUEST_H
