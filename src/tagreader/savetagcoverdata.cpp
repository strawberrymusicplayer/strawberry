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

#include <QByteArray>
#include <QString>

#include "savetagcoverdata.h"

SaveTagCoverData::SaveTagCoverData(const QString &_cover_filename, const QByteArray &_cover_data, const QString &_cover_mimetype)
    : cover_filename(_cover_filename),
      cover_data(_cover_data),
      cover_mimetype(_cover_mimetype) {}

SaveTagCoverData::SaveTagCoverData(const QByteArray &_cover_data, const QString &_cover_mimetype)
    : cover_data(_cover_data),
      cover_mimetype(_cover_mimetype) {}
