/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCROBBLERCACHEITEM_H
#define SCROBBLERCACHEITEM_H

#include "config.h"

#include <QtGlobal>
#include <QMetaType>

#include "includes/shared_ptr.h"
#include "scrobblemetadata.h"

class ScrobblerCacheItem {

 public:
  explicit ScrobblerCacheItem(const ScrobbleMetadata &_metadata, const quint64 _timestamp);

  ScrobbleMetadata metadata;
  quint64 timestamp;
  bool sent;
  bool error;
};

using ScrobblerCacheItemPtr = SharedPtr<ScrobblerCacheItem>;
using ScrobblerCacheItemPtrList = QList<ScrobblerCacheItemPtr>;

Q_DECLARE_METATYPE(ScrobblerCacheItemPtr)
Q_DECLARE_METATYPE(ScrobblerCacheItemPtrList)

#endif  // SCROBBLERCACHEITEM_H
