/*
 * Strawberry Music Player
 * Copyright 2013, 2017-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef ICONLOADER_H
#define ICONLOADER_H

#include <QList>
#include <QString>
#include <QIcon>

class IconLoader {
 public:
  static void Init();
  static QIcon Load(const QString &name, const int size = 0);
 private:
  IconLoader() {}
  static bool system_icons_;
  static bool custom_icons_;
};

#endif  // ICONLOADER_H
