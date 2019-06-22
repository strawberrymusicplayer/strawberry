/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QUrl>

#include "song.h"
#include "urlhandler.h"

UrlHandler::LoadResult::LoadResult(const QUrl &original_url, const Type type, const QUrl &media_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 length_nanosec, const QString error) :
  original_url_(original_url),
  type_(type),
  media_url_(media_url),
  filetype_(filetype),
  samplerate_(samplerate),
  bit_depth_(bit_depth),
  length_nanosec_(length_nanosec),
  error_(error)
  {}

UrlHandler::UrlHandler(QObject *parent) : QObject(parent) {}
