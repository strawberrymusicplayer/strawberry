/* This file is part of Strawberry.
   Copyright 2011, David Sansome <me@davidsansome.com>

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

#include "messagereply.h"

#include <QObject>
#include <QtDebug>

#include "core/logging.h"

_MessageReplyBase::_MessageReplyBase(QObject *parent)
    : QObject(parent), finished_(false), success_(false) {}

bool _MessageReplyBase::WaitForFinished() {

  qLog(Debug) << "Waiting on ID" << id();
  semaphore_.acquire();
  qLog(Debug) << "Acquired ID" << id();
  return success_;

}

void _MessageReplyBase::Abort() {

  Q_ASSERT(!finished_);
  finished_ = true;
  success_ = false;

  emit Finished(success_);
  qLog(Debug) << "Releasing ID" << id() << "(aborted)";
  semaphore_.release();

}
