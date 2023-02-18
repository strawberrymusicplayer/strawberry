/* This file is part of Strawberry.
   Copyright 2015, David Sansome <me@davidsansome.com>

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

#ifndef THREAD_H
#define THREAD_H

#include "config.h"

#include <QThread>

#include "utilities/threadutils.h"

class QObject;

// Improve QThread by adding a SetIoPriority function
class Thread : public QThread {
  Q_OBJECT

 public:
  explicit Thread(QObject *parent = nullptr) : QThread(parent), io_priority_(Utilities::IoPriority::IOPRIO_CLASS_NONE) {}

  void SetIoPriority(Utilities::IoPriority priority) {
    io_priority_ = priority;
  }
  void run() override;

 private:
  Utilities::IoPriority io_priority_;
};

#endif  // THREAD_H
