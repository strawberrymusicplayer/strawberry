/* This file is part of Strawberry.
   Copyright 2011, David Sansome <me@davidsansome.com>
   Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>

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

#ifndef TAGREADERWORKER_H
#define TAGREADERWORKER_H

#include "config.h"

#include <QObject>

#include "core/messagehandler.h"
#if defined(USE_TAGLIB)
#  include "tagreadertaglib.h"
#elif defined(USE_TAGPARSER)
#  include "tagreadertagparser.h"
#endif
#include "tagreadermessages.pb.h"

class QIODevice;

class TagReaderWorker : public AbstractMessageHandler<spb::tagreader::Message> {
  Q_OBJECT

 public:
  explicit TagReaderWorker(QIODevice *socket, QObject *parent = nullptr);

 protected:
  void MessageArrived(const spb::tagreader::Message &message) override;
  void DeviceClosed() override;

 private:
#if defined(USE_TAGLIB)
  TagReaderTagLib tag_reader_;
#elif defined(USE_TAGPARSER)
  TagReaderTagParser tag_reader_;
#endif

};

#endif  // TAGREADERWORKER_H
