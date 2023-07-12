/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOCHANNEL_H
#define RADIOCHANNEL_H

#include <QMetaType>
#include <QList>
#include <QString>
#include <QUrl>

#include "core/song.h"

struct RadioChannel {
  explicit RadioChannel(const Song::Source _source = Song::Source::Unknown, const QString &_name = QString(), const QUrl &_url = QUrl(), const QUrl &_thumbnail_url = QUrl()) : source(_source), name(_name), url(_url), thumbnail_url(_thumbnail_url) {}

  Song::Source source;
  QString name;
  QUrl url;
  QUrl thumbnail_url;

  Song ToSong() const;
};
using RadioChannelList = QList<RadioChannel>;

Q_DECLARE_METATYPE(RadioChannel)
Q_DECLARE_METATYPE(RadioChannelList)

#endif  // RADIOCHANNEL_H
