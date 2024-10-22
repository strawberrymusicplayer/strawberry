/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TRANSLATIONS_H
#define TRANSLATIONS_H

#include "config.h"

#include <QList>
#include <QString>

class QTranslator;

class Translations {
 public:
  explicit Translations();
  ~Translations();
  void LoadTranslation(const QString &prefix, const QString &path, const QString &language);

 private:
  QList<QTranslator*> translations_;

};

#endif  // TRANSLATIONS_H
