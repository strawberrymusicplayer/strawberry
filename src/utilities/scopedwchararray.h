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

#ifndef SCOPEDWCHARARRAY_H
#define SCOPEDWCHARARRAY_H

#include <QObject>
#include <QString>

#include "includes/scoped_ptr.h"

class ScopedWCharArray {
 public:
  explicit ScopedWCharArray(const QString &str);

  QString ToString() const { return QString::fromWCharArray(data_.get()); }

  wchar_t *get() const { return data_.get(); }
  explicit operator wchar_t *() const { return get(); }

  qint64 characters() const { return chars_; }
  qint64 bytes() const { return (chars_ + 1) * static_cast<qint64>(sizeof(wchar_t)); }

 private:
  Q_DISABLE_COPY(ScopedWCharArray)

  qint64 chars_;
  ScopedPtr<wchar_t[]> data_;
};

#endif  // SCOPEDWCHARARRAY_H
