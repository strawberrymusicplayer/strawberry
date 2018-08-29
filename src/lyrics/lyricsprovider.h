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

#ifndef LYRICSPROVIDER_H
#define LYRICSPROVIDER_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QList>
#include <QString>

struct LyricsSearchResult;

class LyricsProvider : public QObject {
  Q_OBJECT

public:
  explicit LyricsProvider(const QString &name, QObject *parent);

  QString name() const { return name_; }

  virtual bool StartSearch(const QString &artist, const QString &album, const QString &title, quint64 id) = 0;
  virtual void CancelSearch(quint64 id) {}

signals:
  void SearchFinished(quint64 id, const QList<LyricsSearchResult>& results);

private:
  QString name_;

};

#endif // LYRICSPROVIDER_H
