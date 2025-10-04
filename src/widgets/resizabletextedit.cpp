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
#include <QTextOption>
#include <QResizeEvent>

#include "resizabletextedit.h"

ResizableTextEdit::ResizableTextEdit(QWidget *parent)
    : QTextEdit(parent) {

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

}

QSize ResizableTextEdit::sizeHint() const {

  qreal doc_width = document()->textWidth();
  int current_width = doc_width > 0 ? qRound(doc_width) : width();

  if (current_width <= 0) {
    current_width = 200; // Fallback width
  }

  QSize doc_size = document()->size().toSize();
  QSize result = QSize(current_width, std::max(doc_size.height(), 10));

  return result;

}

void ResizableTextEdit::resizeEvent(QResizeEvent *e) {

  // Don't update document width here - it's controlled externally
  // from ContextView::resizeEvent()

  qreal old_doc_height = document()->size().height();

  updateGeometry();
  QTextEdit::resizeEvent(e);

  qreal new_doc_height = document()->size().height();

  // Force parent to update if height changed
  if (new_doc_height != old_doc_height && new_doc_height > 0) {
    if (parentWidget()) {
      parentWidget()->updateGeometry();
    }
  }

}

void ResizableTextEdit::SetText(const QString &text) {

  text_ = text;  // Store the text for sizeHint calculations

  QTextEdit::setText(text);

  // Only set document width if it's not already set (i.e., controlled externally)
  qreal current_doc_width = document()->textWidth();
  if (current_doc_width <= 0) {
    qreal doc_width = width() > 0 ? width() : 200;
    document()->setTextWidth(doc_width);
  }

  updateGeometry();

}
