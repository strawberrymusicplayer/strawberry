/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERREADSTREAMREQUEST_H
#define TAGREADERREADSTREAMREQUEST_H

#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "tagreaderrequest.h"

using std::make_shared;

class TagReaderReadStreamRequest : public TagReaderRequest {
 public:
  explicit TagReaderReadStreamRequest(const QUrl &_url, const QString &_filename);
  static SharedPtr<TagReaderReadStreamRequest> Create(const QUrl &_url, const QString &_filename) { return make_shared<TagReaderReadStreamRequest>(_url, _filename); }
  quint64 size;
  quint64 mtime;
  QString token_type;
  QString access_token;
};

using TagReaderReadStreamRequestPtr = SharedPtr<TagReaderReadStreamRequest>;

#endif  // TAGREADERREADSTREAMREQUEST_H
