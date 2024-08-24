/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2011, Andrea Decorte <adecorte@gmail.com>
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

#include <QtGlobal>
#include <QWidget>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>

#include "renametablineedit.h"

RenameTabLineEdit::RenameTabLineEdit(QWidget *parent) : QLineEdit(parent) {}

void RenameTabLineEdit::keyPressEvent(QKeyEvent *e) {

  if (e->key() == Qt::Key_Escape) {
    e->accept();
    Q_EMIT EditingCanceled();
  }
  else {
    QLineEdit::keyPressEvent(e);
  }

}

void RenameTabLineEdit::focusOutEvent(QFocusEvent *e) {

  Q_UNUSED(e);

  //if the user hasn't explicitly accepted, discard the value
  Q_EMIT EditingCanceled();
  //we don't call the default event since it will trigger editingFished()

}
