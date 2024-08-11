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

#ifndef RENAMETABLINEEDIT_H
#define RENAMETABLINEEDIT_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QLineEdit>

class QFocusEvent;
class QKeyEvent;

class RenameTabLineEdit : public QLineEdit {
  Q_OBJECT

 public:
  explicit RenameTabLineEdit(QWidget *parent = nullptr);

 Q_SIGNALS:
  void EditingCanceled();

 protected:
  void focusOutEvent(QFocusEvent *e) override;
  void keyPressEvent(QKeyEvent *e) override;
};

#endif  // RENAMETABLINEEDIT_H
