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

#include "config.h"

#include <string>

#include <QCoreApplication>
#include <QObject>
#include <QIODevice>
#include <QByteArray>

#include "tagreaderworker.h"

TagReaderWorker::TagReaderWorker(QIODevice *socket, QObject *parent)
  : AbstractMessageHandler<pb::tagreader::Message>(socket, parent) {}

void TagReaderWorker::MessageArrived(const pb::tagreader::Message &message) {

  pb::tagreader::Message reply;

  if (message.has_read_file_request()) {
    tag_reader_.ReadFile(QStringFromStdString(message.read_file_request().filename()), reply.mutable_read_file_response()->mutable_metadata());
  }
  else if (message.has_save_file_request()) {
    reply.mutable_save_file_response()->set_success(tag_reader_.SaveFile(QStringFromStdString(message.save_file_request().filename()), message.save_file_request().metadata()));
  }

  else if (message.has_is_media_file_request()) {
    reply.mutable_is_media_file_response()->set_success(tag_reader_.IsMediaFile(QStringFromStdString(message.is_media_file_request().filename())));
  }
  else if (message.has_load_embedded_art_request()) {
    QByteArray data = tag_reader_.LoadEmbeddedArt(QStringFromStdString(message.load_embedded_art_request().filename()));
    reply.mutable_load_embedded_art_response()->set_data(data.constData(), data.size());
  }

  SendReply(message, &reply);

}


void TagReaderWorker::DeviceClosed() {
  AbstractMessageHandler<pb::tagreader::Message>::DeviceClosed();

  qApp->exit();
}
