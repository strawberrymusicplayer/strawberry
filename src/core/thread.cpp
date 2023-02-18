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

#include "config.h"

#include <QThread>

#include "thread.h"
#include "utilities/threadutils.h"

void Thread::run() {

#ifndef Q_OS_WIN32
  if (io_priority_ != Utilities::IoPriority::IOPRIO_CLASS_NONE) {
    Utilities::SetThreadIOPriority(io_priority_);
  }
#endif

  QThread::run();

}
