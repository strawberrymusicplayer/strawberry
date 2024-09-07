/*
 * Strawberry Music Player
 * Copyright 2020-2022, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QRegularExpression>

#include "musixmatchprovider.h"

const char *MusixmatchProvider::kApiUrl = "https://api.musixmatch.com/ws/1.1";
const char *MusixmatchProvider::kApiKey = "Y2FhMDRlN2Y4OWE5OTIxYmZlOGMzOWQzOGI3ZGU4MjE=";

QString MusixmatchProvider::StringFixup(QString text) {

  static const QRegularExpression regex_illegal_characters(QStringLiteral("[^\\w0-9\\- ]"), QRegularExpression::UseUnicodePropertiesOption);
  static const QRegularExpression regex_duplicate_whitespaces(QStringLiteral(" {2,}"));
  static const QRegularExpression regex_duplicate_dashes(QStringLiteral("(-)\\1+"));

  return text.replace(u'/', u'-')
             .replace(u'\'', u'-')
             .remove(regex_illegal_characters)
             .replace(regex_duplicate_whitespaces, QStringLiteral(" "))
             .simplified()
             .replace(u' ', u'-')
             .replace(regex_duplicate_dashes, QStringLiteral("-"))
             .toLower();

}
