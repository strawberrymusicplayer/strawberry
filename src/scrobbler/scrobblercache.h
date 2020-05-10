/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SCROBBLERCACHE_H
#define SCROBBLERCACHE_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QHash>
#include <QString>

#include "scrobblercacheitem.h"

class Song;

class ScrobblerCache : public QObject {
  Q_OBJECT

 public:
  explicit ScrobblerCache(const QString &filename, QObject *parent);
  ~ScrobblerCache();

  void ReadCache();

  ScrobblerCacheItemPtr Add(const Song &song, const quint64 &timestamp);
  ScrobblerCacheItemPtr Get(const quint64 hash);
  void Remove(const quint64 hash);
  void Remove(ScrobblerCacheItemPtr item);
  int Count() const { return scrobbler_cache_.size(); };
  QList<ScrobblerCacheItemPtr> List() const { return scrobbler_cache_.values(); }
  void ClearSent(const QList<quint64> &list);
  void Flush(const QList<quint64> &list);

 public slots:
  void WriteCache();

 private:
  QString filename_;
  bool loaded_;
  QHash<quint64, ScrobblerCacheItemPtr> scrobbler_cache_;

};

#endif  // SCROBBLERCACHE_H
