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

#ifndef MPRIS_COMMON_H
#define MPRIS_COMMON_H

#include "config.h"

#include <QObject>
#include <QVariantMap>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QDBusObjectPath>

namespace mpris {

inline void AddMetadata(const QString &key, const QString &metadata, QVariantMap *map) {
  if (!metadata.isEmpty()) (*map)[key] = metadata;
}

inline void AddMetadataAsList(const QString &key, const QString &metadata, QVariantMap *map) {
  if (!metadata.isEmpty()) (*map)[key] = QStringList() << metadata;
}

inline void AddMetadata(const QString &key, int metadata, QVariantMap *map) {
  if (metadata > 0) (*map)[key] = metadata;
}

inline void AddMetadata(const QString &key, qint64 metadata, QVariantMap *map) {
  if (metadata > 0) (*map)[key] = metadata;
}

inline void AddMetadata(const QString &key, double metadata, QVariantMap *map) {
  if (metadata != 0.0) (*map)[key] = metadata;
}

inline void AddMetadata(const QString &key, const QDateTime &metadata, QVariantMap *map) {
  if (metadata.isValid()) (*map)[key] = metadata;
}

inline void AddMetadata(const QString &key, const QDBusObjectPath &metadata, QVariantMap *map) {
  if (metadata.path().length() > 0) (*map)[key] = QVariant::fromValue<QDBusObjectPath>(metadata);
}

inline QString AsMPRISDateTimeType(const qint64 time) {
  return time != -1 ? QDateTime::fromSecsSinceEpoch(time).toString(Qt::ISODate) : QLatin1String("");
}

}  // namespace mpris

#endif  // MPRIS_COMMON_H
