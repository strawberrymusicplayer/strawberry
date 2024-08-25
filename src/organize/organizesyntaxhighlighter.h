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

#ifndef ORGANISESYNTAXHIGHLIGHTER_H
#define ORGANISESYNTAXHIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QString>
#include <QRgb>

class QTextDocument;
class QTextEdit;

class OrganizeSyntaxHighlighter : public QSyntaxHighlighter {
  Q_OBJECT

 public:
  static const QRgb kValidTagColorLight;
  static const QRgb kInvalidTagColorLight;
  static const QRgb kBlockColorLight;
  static const QRgb kValidTagColorDark;
  static const QRgb kInvalidTagColorDark;
  static const QRgb kBlockColorDark;

  explicit OrganizeSyntaxHighlighter(QObject *parent = nullptr);
  explicit OrganizeSyntaxHighlighter(QTextEdit *parent);
  explicit OrganizeSyntaxHighlighter(QTextDocument *parent);
  void highlightBlock(const QString &text) override;
};

#endif  // ORGANISESYNTAXHIGHLIGHTER_H
