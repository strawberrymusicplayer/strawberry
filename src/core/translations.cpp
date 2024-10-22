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

#include "config.h"

#include <utility>

#include <QCoreApplication>
#include <QTranslator>
#include <QString>

#include "translations.h"
#include "core/logging.h"

using namespace Qt::Literals::StringLiterals;

Translations::Translations() = default;

Translations::~Translations() {

  for (QTranslator *t : std::as_const(translations_)) {
    QCoreApplication::removeTranslator(t);
    delete t;
  }

}

void Translations::LoadTranslation(const QString &prefix, const QString &path, const QString &language) {

  const QString basefilename = prefix + u'_' + language;
  QTranslator *t = new QTranslator;
  if (t->load(basefilename, path)) {
    qLog(Debug) << "Tranlations loaded from" << basefilename;
    QCoreApplication::installTranslator(t);
    translations_ << t;
  }
  else {
    delete t;
  }

}
