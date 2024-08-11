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

#ifndef SCROBBLERCACHE_H
#define SCROBBLERCACHE_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QString>

#include "scrobblercacheitem.h"

class QTimer;
class Song;

class ScrobblerCache : public QObject {
  Q_OBJECT

 public:
  explicit ScrobblerCache(const QString &filename, QObject *parent);
  ~ScrobblerCache() override;

  void ReadCache();

  ScrobblerCacheItemPtr Add(const Song &song, const quint64 timestamp);
  void Remove(ScrobblerCacheItemPtr cache_item);
  int Count() const { return scrobbler_cache_.size(); };
  ScrobblerCacheItemPtrList List() const { return scrobbler_cache_; }
  void ClearSent(ScrobblerCacheItemPtrList cache_items);
  void SetError(ScrobblerCacheItemPtrList cache_items);
  void Flush(ScrobblerCacheItemPtrList cache_items);

 public Q_SLOTS:
  void WriteCache();

 private:
  QTimer *timer_flush_;
  QString filename_;
  bool loaded_;
  QList<ScrobblerCacheItemPtr> scrobbler_cache_;
};

#endif  // SCROBBLERCACHE_H
