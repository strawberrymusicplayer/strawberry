/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QString>
#include <QRegularExpression>

#include "organizeformat.h"
#include "organizeformatvalidator.h"

OrganizeFormatValidator::OrganizeFormatValidator(QObject *parent) : QValidator(parent) {}

QValidator::State OrganizeFormatValidator::validate(QString &input, int &_pos) const {

  Q_UNUSED(_pos)

  // Make sure all the blocks match up
  int block_level = 0;
  for (int i = 0; i < input.length(); ++i) {
    if (input[i] == u'{') {
      ++block_level;
    }
    else if (input[i] == u'}') {
      --block_level;
    }

    if (block_level < 0 || block_level > 1) {
      return QValidator::Invalid;
    }
  }

  if (block_level != 0) return QValidator::Invalid;

  // Make sure the tags are valid
  static const QRegularExpression tag_regexp(QString::fromLatin1(OrganizeFormat::kTagPattern));
  QRegularExpressionMatch re_match;
  qint64 pos = 0;
  for (re_match = tag_regexp.match(input, pos); re_match.hasMatch(); re_match = tag_regexp.match(input, pos)) {
    pos = re_match.capturedStart();
    if (!OrganizeFormat::kKnownTags.contains(re_match.captured(1))) {
      return QValidator::Invalid;
    }

    pos += re_match.capturedLength();
  }

  return QValidator::Acceptable;

}
