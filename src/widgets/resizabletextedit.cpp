/*
 * Strawberry Music Player
 * Copyright 2022, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QResizeEvent>

#include "resizabletextedit.h"

ResizableTextEdit::ResizableTextEdit(QWidget *parent)
    : QTextEdit(parent) {

  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

}

QSize ResizableTextEdit::sizeHint() const {

  return QSize(std::max(QTextEdit::sizeHint().width(), 10), std::max(document()->size().toSize().height(), 10));

}

void ResizableTextEdit::resizeEvent(QResizeEvent *e) {

  updateGeometry();
  QTextEdit::resizeEvent(e);

}

void ResizableTextEdit::SetText(const QString &text) {

  text_ = text;

  QTextEdit::setText(text);

  updateGeometry();

}
