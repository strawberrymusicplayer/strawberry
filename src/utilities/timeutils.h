/*
 * Strawberry Music Player
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TIMEUTILS_H
#define TIMEUTILS_H

#include <QString>
#include <QDate>
#include <QSize>
#include <QLocale>

namespace Utilities {

QString PrettyTime(int seconds);
QString PrettyTimeDelta(const int seconds);
QString PrettyTimeNanosec(const qint64 nanoseconds);
QString WordyTime(const quint64 seconds);
QString WordyTimeNanosec(const quint64 nanoseconds);
QString Ago(const qint64 seconds_since_epoch, const QLocale &locale);
QString PrettyFutureDate(const QDate date);
QDateTime ParseRFC822DateTime(const QString &text);

}  // namespace Utilities

#endif  // TIMEUTILS_H
