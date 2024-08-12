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

#include "config.h"

#include <utility>
#include <memory>
#include <string>

#include <QCoreApplication>
#include <QObject>
#include <QIODevice>
#include <QByteArray>

#include "tagreaderworker.h"

#ifdef HAVE_TAGLIB
#  include "tagreadertaglib.h"
#  include "tagreadergme.h"
#endif

#ifdef HAVE_TAGPARSER
#  include "tagreadertagparser.h"
#endif

using std::make_shared;
using std::shared_ptr;

TagReaderWorker::TagReaderWorker(QIODevice *socket, QObject *parent)
    : AbstractMessageHandler<spb::tagreader::Message>(socket, parent) {

#ifdef HAVE_TAGLIB
  tagreaders_ << make_shared<TagReaderTagLib>();
  tagreaders_ << make_shared<TagReaderGME>();
#endif

#ifdef HAVE_TAGPARSER
  tagreaders_ << make_shared<TagReaderTagParser>();
#endif

}

void TagReaderWorker::MessageArrived(const spb::tagreader::Message &message) {

  spb::tagreader::Message reply;

  HandleMessage(message, reply);
  SendReply(message, &reply);

}

void TagReaderWorker::DeviceClosed() {

  AbstractMessageHandler<spb::tagreader::Message>::DeviceClosed();

  QCoreApplication::exit();

}

void TagReaderWorker::HandleMessage(const spb::tagreader::Message &message, spb::tagreader::Message &reply) {

  for (shared_ptr<TagReaderBase> reader : std::as_const(tagreaders_)) {

    if (message.has_is_media_file_request()) {
      const QString filename = QString::fromStdString(message.is_media_file_request().filename());
      const bool success = reader->IsMediaFile(filename);
      reply.mutable_is_media_file_response()->set_success(success);
      if (success) {
        return;
      }
    }
    if (message.has_read_file_request()) {
      const QString filename = QString::fromStdString(message.read_file_request().filename());
      spb::tagreader::ReadFileResponse *response = reply.mutable_read_file_response();
      const TagReaderBase::Result result = reader->ReadFile(filename, response->mutable_metadata());
      response->set_success(result.success());
      if (result.success()) {
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }
    if (message.has_write_file_request()) {
      const QString filename = QString::fromStdString(message.write_file_request().filename());
      const TagReaderBase::Result result = reader->WriteFile(filename, message.write_file_request());
      spb::tagreader::WriteFileResponse *response = reply.mutable_write_file_response();
      response->set_success(result.success());
      if (result.success()) {
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }
    if (message.has_load_embedded_art_request()) {
      const QString filename = QString::fromStdString(message.load_embedded_art_request().filename());
      QByteArray data;
      const TagReaderBase::Result result = reader->LoadEmbeddedArt(filename, data);
      spb::tagreader::LoadEmbeddedArtResponse *response = reply.mutable_load_embedded_art_response();
      response->set_success(result.success());
      if (result.success()) {
        response->set_data(data.toStdString());
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }
    if (message.has_save_embedded_art_request()) {
      const QString filename = QString::fromStdString(message.save_embedded_art_request().filename());
      const TagReaderBase::Result result = reader->SaveEmbeddedArt(filename, message.save_embedded_art_request());
      spb::tagreader::SaveEmbeddedArtResponse *response = reply.mutable_save_embedded_art_response();
      response->set_success(result.success());
      if (result.success()) {
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }
    if (message.has_save_song_playcount_to_file_request()) {
      const QString filename = QString::fromStdString(message.save_song_playcount_to_file_request().filename());
      const TagReaderBase::Result result = reader->SaveSongPlaycountToFile(filename, message.save_song_playcount_to_file_request().playcount());
      spb::tagreader::SaveSongPlaycountToFileResponse *response = reply.mutable_save_song_playcount_to_file_response();
      response->set_success(result.success());
      if (result.success()) {
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }
    if (message.has_save_song_rating_to_file_request()) {
      const QString filename = QString::fromStdString(message.save_song_rating_to_file_request().filename());
      const TagReaderBase::Result result = reader->SaveSongRatingToFile(filename, message.save_song_rating_to_file_request().rating());
      spb::tagreader::SaveSongRatingToFileResponse *response = reply.mutable_save_song_rating_to_file_response();
      response->set_success(result.success());
      if (result.success()) {
        if (response->has_error()) {
          response->clear_error();
        }
        return;
      }
      else {
        if (!response->has_error()) {
          response->set_error(TagReaderBase::ErrorString(result).toStdString());
        }
      }
    }

  }

}
