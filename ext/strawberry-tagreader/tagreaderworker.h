/* This file is part of Strawberry.
   Copyright 2011, David Sansome <me@davidsansome.com>
   Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>

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

#include <memory>

#include <QObject>
#include <QList>

#include "core/messagehandler.h"

#include "tagreadermessages.pb.h"

class QIODevice;
class TagReaderBase;

using std::shared_ptr;

class TagReaderWorker : public AbstractMessageHandler<spb::tagreader::Message> {
  Q_OBJECT

 public:
  explicit TagReaderWorker(QIODevice *socket, QObject *parent = nullptr);

 protected:
  void MessageArrived(const spb::tagreader::Message &message) override;
  void DeviceClosed() override;

 private:
  void HandleMessage(const spb::tagreader::Message &message, spb::tagreader::Message &reply);

  QList<shared_ptr<TagReaderBase>> tagreaders_;
};

#endif  // TAGREADERWORKER_H
