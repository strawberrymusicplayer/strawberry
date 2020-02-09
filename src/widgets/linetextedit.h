/*
   Strawberry Music Player
   This file was part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef LINETEXTEDIT_H
#define LINETEXTEDIT_H

#include "config.h"

#include <QObject>
#include <QSize>
#include <QString>
#include <QTextEdit>

class QWidget;
class QKeyEvent;

class LineTextEdit : public QTextEdit {
  Q_OBJECT

 public:
  LineTextEdit(QWidget *parent = nullptr);

  QSize sizeHint() const;
  QSize minimumSizeHint() const;

protected:
  void keyPressEvent(QKeyEvent *e);
};

#endif  // LINETEXTEDIT_H
