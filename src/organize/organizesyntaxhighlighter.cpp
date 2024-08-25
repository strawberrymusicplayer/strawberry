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

#include <QApplication>
#include <QString>
#include <QRegularExpression>
#include <QColor>
#include <QPalette>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextCharFormat>

#include "organizeformat.h"
#include "organizesyntaxhighlighter.h"

const QRgb OrganizeSyntaxHighlighter::kValidTagColorLight = qRgb(64, 64, 255);
const QRgb OrganizeSyntaxHighlighter::kInvalidTagColorLight = qRgb(255, 64, 64);
const QRgb OrganizeSyntaxHighlighter::kBlockColorLight = qRgb(230, 230, 230);

const QRgb OrganizeSyntaxHighlighter::kValidTagColorDark = qRgb(128, 128, 255);
const QRgb OrganizeSyntaxHighlighter::kInvalidTagColorDark = qRgb(255, 128, 128);
const QRgb OrganizeSyntaxHighlighter::kBlockColorDark = qRgb(64, 64, 64);

OrganizeSyntaxHighlighter::OrganizeSyntaxHighlighter(QObject *parent) : QSyntaxHighlighter(parent) {}

OrganizeSyntaxHighlighter::OrganizeSyntaxHighlighter(QTextEdit *parent) : QSyntaxHighlighter(parent) {}

OrganizeSyntaxHighlighter::OrganizeSyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {}

void OrganizeSyntaxHighlighter::highlightBlock(const QString &text) {

  const bool light = QApplication::palette().color(QPalette::Base).value() > 128;
  const QRgb block_color = light ? kBlockColorLight : kBlockColorDark;
  const QRgb valid_tag_color = light ? kValidTagColorLight : kValidTagColorDark;
  const QRgb invalid_tag_color = light ? kInvalidTagColorLight : kInvalidTagColorDark;

  QTextCharFormat block_format;
  block_format.setBackground(QColor(block_color));

  // Reset formatting
  setFormat(0, static_cast<int>(text.length()), QTextCharFormat());

  // Blocks
  static const QRegularExpression block_regexp(QString::fromLatin1(OrganizeFormat::kBlockPattern));
  QRegularExpressionMatch re_match;
  qint64 pos = 0;
  for (re_match = block_regexp.match(text, pos); re_match.hasMatch(); re_match = block_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    setFormat(static_cast<int>(pos), static_cast<int>(re_match.capturedLength()), block_format);
    pos += re_match.capturedLength();
  }

  // Tags
  static const QRegularExpression tag_regexp(QString::fromLatin1(OrganizeFormat::kTagPattern));
  pos = 0;
  for (re_match = tag_regexp.match(text, pos); re_match.hasMatch(); re_match = tag_regexp.match(text, pos)) {
    pos = re_match.capturedStart();
    QTextCharFormat f = format(static_cast<int>(pos));
    f.setForeground(QColor(OrganizeFormat::kKnownTags.contains(re_match.captured(1)) ? valid_tag_color : invalid_tag_color));

    setFormat(static_cast<int>(pos), static_cast<int>(re_match.capturedLength()), f);
    pos += re_match.capturedLength();
  }

}
