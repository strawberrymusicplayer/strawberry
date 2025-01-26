/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

UrlHandler::UrlHandler(QObject *parent) : QObject(parent) {}

UrlHandler::LoadResult::LoadResult(const QUrl &media_url,
                                   const Type type,
                                   const QUrl &stream_url,
                                   const Song::FileType filetype,
                                   const int samplerate,
                                   const int bit_depth,
                                   const qint64 length_nanosec,
                                   const QString &error)
    : media_url_(media_url),
      type_(type),
      stream_url_(stream_url),
      filetype_(filetype),
      samplerate_(samplerate),
      bit_depth_(bit_depth),
      length_nanosec_(length_nanosec),
      error_(error) {}

UrlHandler::LoadResult::LoadResult(const QUrl &media_url, const Type type, const QString &error)
    : media_url_(media_url),
      type_(type),
      filetype_(Song::FileType::Stream),
      samplerate_(-1),
      bit_depth_(-1),
      length_nanosec_(-1),
      error_(error) {}

