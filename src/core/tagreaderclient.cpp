/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"

#include <string>

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QString>
#include <QImage>

#include "core/logging.h"
#include "core/workerpool.h"

#include "song.h"
#include "tagreaderclient.h"

using namespace Qt::StringLiterals;

namespace {
constexpr char kWorkerExecutableName[] = "strawberry-tagreader";
}

TagReaderClient *TagReaderClient::sInstance = nullptr;

TagReaderClient::TagReaderClient(QObject *parent) : QObject(parent), worker_pool_(new WorkerPool<HandlerType>(this)) {

  setObjectName(QLatin1String(metaObject()->className()));

  sInstance = this;
  original_thread_ = thread();

  worker_pool_->SetExecutableName(QLatin1String(kWorkerExecutableName));
  QObject::connect(worker_pool_, &WorkerPool<HandlerType>::WorkerFailedToStart, this, &TagReaderClient::WorkerFailedToStart);

}

void TagReaderClient::Start() { worker_pool_->Start(); }

void TagReaderClient::ExitAsync() {
  QMetaObject::invokeMethod(this, &TagReaderClient::Exit, Qt::QueuedConnection);
}

void TagReaderClient::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());
  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

void TagReaderClient::WorkerFailedToStart() {
  qLog(Error) << "The" << kWorkerExecutableName << "executable was not found in the current directory or on the PATH.  Strawberry will not be able to read music file tags without it.";
}

TagReaderReply *TagReaderClient::IsMediaFile(const QString &filename) {

  spb::tagreader::Message message;
  message.mutable_is_media_file_request()->set_filename(filename.toStdString());
  return worker_pool_->SendMessageWithReply(&message);

}

TagReaderReply *TagReaderClient::ReadFile(const QString &filename) {

  spb::tagreader::Message message;
  message.mutable_read_file_request()->set_filename(filename.toStdString());
  return worker_pool_->SendMessageWithReply(&message);

}

TagReaderReply *TagReaderClient::WriteFile(const QString &filename, const Song &metadata, const SaveTypes save_types, const SaveCoverOptions &save_cover_options) {

  spb::tagreader::Message message;
  spb::tagreader::WriteFileRequest *request = message.mutable_write_file_request();

  request->set_filename(filename.toStdString());

  request->set_save_tags(save_types.testFlag(SaveType::Tags));
  request->set_save_playcount(save_types.testFlag(SaveType::PlayCount));
  request->set_save_rating(save_types.testFlag(SaveType::Rating));
  request->set_save_cover(save_types.testFlag(SaveType::Cover));

  if (!save_cover_options.cover_filename.isEmpty()) {
    request->set_cover_filename(save_cover_options.cover_filename.toStdString());
  }
  if (!save_cover_options.cover_data.isEmpty()) {
    request->set_cover_data(save_cover_options.cover_data.toStdString());
  }
  if (!save_cover_options.mime_type.isEmpty()) {
    request->set_cover_mime_type(save_cover_options.mime_type.toStdString());
  }

  metadata.ToProtobuf(request->mutable_metadata());

  ReplyType *reply = worker_pool_->SendMessageWithReply(&message);

  return reply;

}

TagReaderReply *TagReaderClient::LoadEmbeddedArt(const QString &filename) {

  spb::tagreader::Message message;
  spb::tagreader::LoadEmbeddedArtRequest *request = message.mutable_load_embedded_art_request();

  request->set_filename(filename.toStdString());

  return worker_pool_->SendMessageWithReply(&message);

}

TagReaderReply *TagReaderClient::SaveEmbeddedArt(const QString &filename, const SaveCoverOptions &save_cover_options) {

  spb::tagreader::Message message;
  spb::tagreader::SaveEmbeddedArtRequest *request = message.mutable_save_embedded_art_request();

  request->set_filename(filename.toStdString());

  if (!save_cover_options.cover_filename.isEmpty()) {
    request->set_cover_filename(save_cover_options.cover_filename.toStdString());
  }
  if (!save_cover_options.cover_data.isEmpty()) {
    request->set_cover_data(save_cover_options.cover_data.toStdString());
  }
  if (!save_cover_options.mime_type.isEmpty()) {
    request->set_cover_mime_type(save_cover_options.mime_type.toStdString());
  }

  return worker_pool_->SendMessageWithReply(&message);

}

TagReaderReply *TagReaderClient::SaveSongPlaycount(const QString &filename, const uint playcount) {

  spb::tagreader::Message message;
  spb::tagreader::SaveSongPlaycountToFileRequest *request = message.mutable_save_song_playcount_to_file_request();

  request->set_filename(filename.toStdString());
  request->set_playcount(playcount);

  return worker_pool_->SendMessageWithReply(&message);

}

void TagReaderClient::SaveSongsPlaycount(const SongList &songs) {

  for (const Song &song : songs) {
    TagReaderReply *reply = SaveSongPlaycount(song.url().toLocalFile(), song.playcount());
    QObject::connect(reply, &TagReaderReply::Finished, reply, &TagReaderReply::deleteLater);
  }

}

TagReaderReply *TagReaderClient::SaveSongRating(const QString &filename, const float rating) {

  spb::tagreader::Message message;
  spb::tagreader::SaveSongRatingToFileRequest *request = message.mutable_save_song_rating_to_file_request();

  request->set_filename(filename.toStdString());
  request->set_rating(rating);

  return worker_pool_->SendMessageWithReply(&message);

}

void TagReaderClient::SaveSongsRating(const SongList &songs) {

  for (const Song &song : songs) {
    TagReaderReply *reply = SaveSongRating(song.url().toLocalFile(), song.rating());
    QObject::connect(reply, &TagReaderReply::Finished, reply, &TagReaderReply::deleteLater);
  }

}

bool TagReaderClient::IsMediaFileBlocking(const QString &filename) {

  Q_ASSERT(QThread::currentThread() != thread());

  bool success = false;

  TagReaderReply *reply = IsMediaFile(filename);
  if (reply->WaitForFinished()) {
    const spb::tagreader::IsMediaFileResponse &response = reply->message().is_media_file_response();
    if (response.has_success()) {
      success = response.success();
    }
  }
  reply->deleteLater();

  return success;

}

TagReaderClient::Result TagReaderClient::ReadFileBlocking(const QString &filename, Song *song) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = ReadFile(filename);
  if (reply->WaitForFinished()) {
    const spb::tagreader::ReadFileResponse &response = reply->message().read_file_response();
    if (response.has_success()) {
      if (response.success()) {
        result.error_code = Result::ErrorCode::Success;
        if (response.has_metadata()) {
          song->InitFromProtobuf(response.metadata());
        }
      }
      else {
        result.error_code = Result::ErrorCode::Failure;
        if (response.has_error()) {
          result.error = QString::fromStdString(response.error());
        }
      }
    }
  }
  reply->deleteLater();

  return result;

}

TagReaderClient::Result TagReaderClient::WriteFileBlocking(const QString &filename, const Song &metadata, const SaveTypes save_types, const SaveCoverOptions &save_cover_options) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = WriteFile(filename, metadata, save_types, save_cover_options);
  if (reply->WaitForFinished()) {
    const spb::tagreader::WriteFileResponse &response = reply->message().write_file_response();
    if (response.has_success()) {
      result.error_code = response.success() ? Result::ErrorCode::Success : Result::ErrorCode::Failure;
      if (response.has_error()) {
        result.error = QString::fromStdString(response.error());
      }
    }
  }
  reply->deleteLater();

  return result;

}

TagReaderClient::Result TagReaderClient::LoadEmbeddedArtBlocking(const QString &filename, QByteArray &data) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = LoadEmbeddedArt(filename);
  if (reply->WaitForFinished()) {
    const spb::tagreader::LoadEmbeddedArtResponse &response = reply->message().load_embedded_art_response();
    if (response.has_success()) {
      if (response.success()) {
        result.error_code = Result::ErrorCode::Success;
        if (response.has_data()) {
          data = QByteArray(response.data().data(), static_cast<qint64>(response.data().size()));
        }
      }
      else {
        result.error_code = Result::ErrorCode::Failure;
        if (response.has_error()) {
          result.error = QString::fromStdString(response.error());
        }
      }
    }
  }
  reply->deleteLater();

  return result;

}

TagReaderClient::Result TagReaderClient::LoadEmbeddedArtAsImageBlocking(const QString &filename, QImage &image) {

  Q_ASSERT(QThread::currentThread() != thread());

  QByteArray data;
  Result result = LoadEmbeddedArtBlocking(filename, data);
  if (result.error_code == Result::ErrorCode::Success && !image.loadFromData(data)) {
    result.error_code = Result::ErrorCode::Failure;
    result.error = QObject::tr("Failed to load image from data for %1").arg(filename);
  }

  return result;

}

TagReaderClient::Result TagReaderClient::SaveEmbeddedArtBlocking(const QString &filename, const SaveCoverOptions &save_cover_options) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = SaveEmbeddedArt(filename, save_cover_options);
  if (reply->WaitForFinished()) {
    const spb::tagreader::SaveEmbeddedArtResponse &response = reply->message().save_embedded_art_response();
    if (response.has_success()) {
      result.error_code = response.success() ? Result::ErrorCode::Success : Result::ErrorCode::Failure;
      if (response.has_error()) {
        result.error = QString::fromStdString(response.error());
      }
    }
  }
  reply->deleteLater();

  return result;

}

TagReaderClient::Result TagReaderClient::SaveSongPlaycountBlocking(const QString &filename, const uint playcount) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = SaveSongPlaycount(filename, playcount);
  if (reply->WaitForFinished()) {
    const spb::tagreader::SaveSongPlaycountToFileResponse &response = reply->message().save_song_playcount_to_file_response();
    if (response.has_success()) {
      result.error_code = response.success() ? Result::ErrorCode::Success : Result::ErrorCode::Failure;
      if (response.has_error()) {
        result.error = QString::fromStdString(response.error());
      }
    }
  }
  reply->deleteLater();

  return result;

}

TagReaderClient::Result TagReaderClient::SaveSongRatingBlocking(const QString &filename, const float rating) {

  Q_ASSERT(QThread::currentThread() != thread());

  Result result(Result::ErrorCode::Failure);

  TagReaderReply *reply = SaveSongRating(filename, rating);
  if (reply->WaitForFinished()) {
    const spb::tagreader::SaveSongRatingToFileResponse &response = reply->message().save_song_rating_to_file_response();
    if (response.has_success()) {
      result.error_code = response.success() ? Result::ErrorCode::Success : Result::ErrorCode::Failure;
      if (response.has_error()) {
        result.error = QString::fromStdString(response.error());
      }
    }
  }
  reply->deleteLater();

  return result;

}
