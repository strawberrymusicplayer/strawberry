/*
 * Strawberry Music Player
 * Copyright 2022-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QTextEdit>
#include <QTextCharFormat>
#include <QResizeEvent>

#include "resizabletextedit.h"

ResizableTextEdit::ResizableTextEdit(QWidget *parent)
    : QTextEdit(parent) {

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWordWrapMode(QTextOption::WordWrap);
  setLineWrapMode(QTextEdit::WidgetWidth);

}

QSize ResizableTextEdit::sizeHint() const {

  return QSize(std::max(QTextEdit::sizeHint().width(), 10), std::max(document()->size().toSize().height(), 10));

}

void ResizableTextEdit::resizeEvent(QResizeEvent *e) {

  QTextEdit::resizeEvent(e);
  updateGeometry();

}

void ResizableTextEdit::SetText(const QString &text) {

  text_ = text;

  QTextEdit::setText(text);

  updateGeometry();

}

void ResizableTextEdit::ResetCharFormat() {

  // Setting plain text gives all the text the format at the cursor, so when the cursor is in text formatted differently, like a bold line the user clicked in, all the new text would get that format.
  setCurrentCharFormat(QTextCharFormat());

}

void ResizableTextEdit::Clear() {

  text_.clear();
  ResetCharFormat();
  QTextEdit::clear();
  updateGeometry();

}

void ResizableTextEdit::SetPlainText(const QString &text) {

  text_ = text;
  ResetCharFormat();
  QTextEdit::setPlainText(text);
  updateGeometry();

}
