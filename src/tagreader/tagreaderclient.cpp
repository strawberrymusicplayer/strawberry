/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QImage>
#include <QScopeGuard>

#include "core/logging.h"
#include "core/song.h"

#include "tagreaderclient.h"
#include "tagreadertaglib.h"
#include "tagreaderresult.h"
#include "tagreaderrequest.h"
#include "tagreaderismediafilerequest.h"
#include "tagreaderreadfilerequest.h"
#include "tagreaderreadstreamrequest.h"
#include "tagreaderwritefilerequest.h"
#include "tagreaderloadcoverdatarequest.h"
#include "tagreaderloadcoverimagerequest.h"
#include "tagreadersavecoverrequest.h"
#include "tagreadersaveplaycountrequest.h"
#include "tagreadersaveratingrequest.h"
#include "tagreaderreply.h"
#include "tagreaderreadfilereply.h"
#include "tagreaderreadstreamreply.h"
#include "tagreaderloadcoverdatareply.h"
#include "tagreaderloadcoverimagereply.h"

using std::dynamic_pointer_cast;
using namespace Qt::Literals::StringLiterals;

TagReaderClient *TagReaderClient::sInstance = nullptr;

TagReaderClient::TagReaderClient(QObject *parent)
    : QObject(parent),
      original_thread_(thread()),
      abort_(false),
      processing_(false) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  if (!sInstance) {
    sInstance = this;
  }

}

void TagReaderClient::ExitAsync() {

  Q_ASSERT(QThread::currentThread() != thread());

  abort_ = true;
  QMetaObject::invokeMethod(this, &TagReaderClient::Exit, Qt::QueuedConnection);

}

void TagReaderClient::Exit() {

  Q_ASSERT(QThread::currentThread() == thread());

  moveToThread(original_thread_);
  Q_EMIT ExitFinished();

}

bool TagReaderClient::HaveRequests() const {

  Q_ASSERT(QThread::currentThread() == thread());

  {
    QMutexLocker l(&mutex_requests_);
    return !requests_.isEmpty();
  }

}

void TagReaderClient::EnqueueRequest(TagReaderRequestPtr request) {

  Q_ASSERT(QThread::currentThread() != thread());

  {
    QMutexLocker l(&mutex_requests_);
    requests_.enqueue(request);
  }

  if (!processing_.value()) {
    ProcessRequestsAsync();
  }

}

TagReaderRequestPtr TagReaderClient::DequeueRequest() {

  Q_ASSERT(QThread::currentThread() == thread());

  {
    QMutexLocker l(&mutex_requests_);
    if (requests_.isEmpty()) return TagReaderRequestPtr();
    return requests_.dequeue();
  }

}

void TagReaderClient::ProcessRequestsAsync() {

  Q_ASSERT(QThread::currentThread() != thread());

  QMetaObject::invokeMethod(this, &TagReaderClient::ProcessRequests, Qt::QueuedConnection);

}

void TagReaderClient::ProcessRequests() {

  Q_ASSERT(QThread::currentThread() == thread());

  processing_ = true;

  const QScopeGuard scopeguard_processing = qScopeGuard([this]() {
    processing_ = false;
  });

  while (HaveRequests()) {
    if (abort_.value()) return;
    ProcessRequest(DequeueRequest());
  }

}

void TagReaderClient::ProcessRequest(TagReaderRequestPtr request) {

  Q_ASSERT(QThread::currentThread() == thread());

  TagReaderReplyPtr reply = request->reply;

  TagReaderResult result;

  if (TagReaderIsMediaFileRequestPtr is_media_file_request = dynamic_pointer_cast<TagReaderIsMediaFileRequest>(request)) {
    result = tagreader_.IsMediaFile(is_media_file_request->filename);
    if (result.error_code == TagReaderResult::ErrorCode::FileOpenError || result.error_code == TagReaderResult::ErrorCode::Unsupported) {
      result = gmereader_.IsMediaFile(is_media_file_request->filename);
    }
  }
  else if (TagReaderReadFileRequestPtr read_file_request = dynamic_pointer_cast<TagReaderReadFileRequest>(request)) {
    Song song;
    result = ReadFileBlocking(read_file_request->filename, &song);
    if (result.error_code == TagReaderResult::ErrorCode::FileOpenError || result.error_code == TagReaderResult::ErrorCode::Unsupported) {
      result = gmereader_.ReadFile(read_file_request->filename, &song);
    }
    if (result.success()) {
      if (TagReaderReadFileReplyPtr read_file_reply = qSharedPointerDynamicCast<TagReaderReadFileReply>(reply)) {
        read_file_reply->set_song(song);
      }
    }
  }
#ifdef HAVE_STREAMTAGREADER
  else if (TagReaderReadStreamRequestPtr read_stream_request = dynamic_pointer_cast<TagReaderReadStreamRequest>(request)) {
    Song song;
    result = ReadStreamBlocking(read_stream_request->url, read_stream_request->filename, read_stream_request->size, read_stream_request->mtime, read_stream_request->token_type, read_stream_request->access_token, &song);
    if (result.success()) {
      if (TagReaderReadStreamReplyPtr read_stream_reply = qSharedPointerDynamicCast<TagReaderReadStreamReply>(reply)) {
        read_stream_reply->set_song(song);
      }
    }
  }
#endif  // HAVE_STREAMTAGREADER
  else if (TagReaderWriteFileRequestPtr write_file_request = dynamic_pointer_cast<TagReaderWriteFileRequest>(request)) {
    result = WriteFileBlocking(write_file_request->filename, write_file_request->song, write_file_request->save_tags_options, write_file_request->save_tag_cover_data);
  }
  else if (TagReaderLoadCoverDataRequestPtr load_cover_data_request = dynamic_pointer_cast<TagReaderLoadCoverDataRequest>(request)) {
    QByteArray cover_data;
    result = LoadCoverDataBlocking(load_cover_data_request->filename, cover_data);
    if (result.success()) {
      if (TagReaderLoadCoverDataReplyPtr load_cover_data_reply = qSharedPointerDynamicCast<TagReaderLoadCoverDataReply>(reply)) {
        load_cover_data_reply->set_data(cover_data);
      }
    }
  }
  else if (TagReaderLoadCoverImageRequestPtr load_cover_image_request = dynamic_pointer_cast<TagReaderLoadCoverImageRequest>(request)) {
    QImage cover_image;
    result = LoadCoverImageBlocking(load_cover_image_request->filename, cover_image);
    if (result.success()) {
      if (TagReaderLoadCoverImageReplyPtr load_cover_image_reply = qSharedPointerDynamicCast<TagReaderLoadCoverImageReply>(reply)) {
        load_cover_image_reply->set_image(cover_image);
      }
    }
  }
  else if (TagReaderSaveCoverRequestPtr save_cover_request = dynamic_pointer_cast<TagReaderSaveCoverRequest>(request)) {
    result = SaveCoverBlocking(save_cover_request->filename, save_cover_request->save_tag_cover_data);
  }
  else if (TagReaderSavePlaycountRequestPtr save_playcount_request = dynamic_pointer_cast<TagReaderSavePlaycountRequest>(request)) {
    result = SaveSongPlaycountBlocking(save_playcount_request->filename, save_playcount_request->playcount);
  }
  else if (TagReaderSaveRatingRequestPtr save_rating_request = dynamic_pointer_cast<TagReaderSaveRatingRequest>(request)) {
    result = SaveSongRatingBlocking(save_rating_request->filename, save_rating_request->rating);
  }

  reply->set_result(result);

  reply->Finish();

}

bool TagReaderClient::IsMediaFileBlocking(const QString &filename) const {

  Q_ASSERT(QThread::currentThread() != thread());

  return tagreader_.IsMediaFile(filename).success() || gmereader_.IsMediaFile(filename).success();

}

TagReaderReplyPtr TagReaderClient::IsMediaFileAsync(const QString &filename) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(filename);

  TagReaderIsMediaFileRequestPtr request = TagReaderIsMediaFileRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;

  EnqueueRequest(request);

  return reply;

}

TagReaderResult TagReaderClient::ReadFileBlocking(const QString &filename, Song *song) {

  const TagReaderResult result = tagreader_.ReadFile(filename, song);
  if (result.error_code == TagReaderResult::ErrorCode::FileOpenError || result.error_code == TagReaderResult::ErrorCode::Unsupported) {
    return gmereader_.ReadFile(filename, song);
  }

  return result;

}

TagReaderReadFileReplyPtr TagReaderClient::ReadFileAsync(const QString &filename) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReadFileReplyPtr reply = TagReaderReply::Create<TagReaderReadFileReply>(filename);

  TagReaderReadFileRequestPtr request = TagReaderReadFileRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;

  EnqueueRequest(request);

  return reply;

}

#ifdef HAVE_STREAMTAGREADER
TagReaderResult TagReaderClient::ReadStreamBlocking(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token, Song *song) {

  return tagreader_.ReadStream(url, filename, size, mtime, token_type, access_token, song);

}

TagReaderReadStreamReplyPtr TagReaderClient::ReadStreamAsync(const QUrl &url, const QString &filename, const quint64 size, const quint64 mtime, const QString &token_type, const QString &access_token) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReadStreamReplyPtr reply = TagReaderReply::Create<TagReaderReadStreamReply>(url, filename);

  TagReaderReadStreamRequestPtr request = TagReaderReadStreamRequest::Create(url, filename);
  request->reply = reply;
  request->size = size;
  request->mtime = mtime;
  request->token_type = token_type;
  request->access_token = access_token;

  EnqueueRequest(request);

  return reply;

}
#endif  // HAVE_STREAMTAGREADER

TagReaderResult TagReaderClient::WriteFileBlocking(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) {

  return tagreader_.WriteFile(filename, song, save_tags_options, save_tag_cover_data);

}

TagReaderReplyPtr TagReaderClient::WriteFileAsync(const QString &filename, const Song &song, const SaveTagsOptions save_tags_options, const SaveTagCoverData &save_tag_cover_data) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(filename);

  TagReaderWriteFileRequestPtr request = TagReaderWriteFileRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;
  request->song = song;
  request->save_tags_options = save_tags_options;
  request->save_tag_cover_data = save_tag_cover_data;

  EnqueueRequest(request);

  return reply;

}

TagReaderResult TagReaderClient::LoadCoverDataBlocking(const QString &filename, QByteArray &data) {

  return tagreader_.LoadEmbeddedCover(filename, data);

}

TagReaderResult TagReaderClient::LoadCoverImageBlocking(const QString &filename, QImage &image) {

  QByteArray data;
  TagReaderResult result = LoadCoverDataBlocking(filename, data);
  if (result.error_code == TagReaderResult::ErrorCode::Success && !image.loadFromData(data)) {
    result.error_code = TagReaderResult::ErrorCode::Unsupported;
    result.error_text = QObject::tr("Failed to load image from data for %1").arg(filename);
  }

  return result;

}

TagReaderLoadCoverDataReplyPtr TagReaderClient::LoadCoverDataAsync(const QString &filename) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderLoadCoverDataReplyPtr reply = TagReaderReply::Create<TagReaderLoadCoverDataReply>(filename);

  TagReaderLoadCoverDataRequestPtr request = TagReaderLoadCoverDataRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;

  EnqueueRequest(request);

  return reply;

}

TagReaderLoadCoverImageReplyPtr TagReaderClient::LoadCoverImageAsync(const QString &filename) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderLoadCoverImageReplyPtr reply = TagReaderReply::Create<TagReaderLoadCoverImageReply>(filename);

  TagReaderLoadCoverImageRequestPtr request = TagReaderLoadCoverImageRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;

  EnqueueRequest(request);

  return reply;

}

TagReaderResult TagReaderClient::SaveCoverBlocking(const QString &filename, const SaveTagCoverData &save_tag_cover_data) {

  return tagreader_.SaveEmbeddedCover(filename, save_tag_cover_data);

}

TagReaderReplyPtr TagReaderClient::SaveCoverAsync(const QString &filename, const SaveTagCoverData &save_tag_cover_data) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(filename);

  TagReaderSaveCoverRequestPtr request = TagReaderSaveCoverRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;
  request->save_tag_cover_data = save_tag_cover_data;

  EnqueueRequest(request);

  return reply;

}

TagReaderReplyPtr TagReaderClient::SaveSongPlaycountAsync(const QString &filename, const uint playcount) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(filename);

  TagReaderSavePlaycountRequestPtr request = TagReaderSavePlaycountRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;
  request->playcount = playcount;

  EnqueueRequest(request);

  return reply;

}

TagReaderResult TagReaderClient::SaveSongPlaycountBlocking(const QString &filename, const uint playcount) {

  return tagreader_.SaveSongPlaycount(filename, playcount);

}

void TagReaderClient::SaveSongsPlaycountAsync(const SongList &songs) {

  Q_ASSERT(QThread::currentThread() != thread());

  for (const Song &song : songs) {
    SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
    TagReaderReplyPtr reply = SaveSongPlaycountAsync(song.url().toLocalFile(), song.playcount());
    *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [reply, connection]() {
      QObject::disconnect(*connection);
    }, Qt::QueuedConnection);
  }

}

TagReaderResult TagReaderClient::SaveSongRatingBlocking(const QString &filename, const float rating) {

  return tagreader_.SaveSongRating(filename, rating);

}

TagReaderReplyPtr TagReaderClient::SaveSongRatingAsync(const QString &filename, const float rating) {

  Q_ASSERT(QThread::currentThread() != thread());

  TagReaderReplyPtr reply = TagReaderReply::Create<TagReaderReply>(filename);

  TagReaderSaveRatingRequestPtr request = TagReaderSaveRatingRequest::Create(filename);
  request->reply = reply;
  request->filename = filename;
  request->rating = rating;

  EnqueueRequest(request);

  return reply;

}

void TagReaderClient::SaveSongsRatingAsync(const SongList &songs) {

  Q_ASSERT(QThread::currentThread() != thread());

  for (const Song &song : songs) {
    SharedPtr<QMetaObject::Connection> connection = make_shared<QMetaObject::Connection>();
    TagReaderReplyPtr reply = SaveSongRatingAsync(song.url().toLocalFile(), song.rating());
    *connection = QObject::connect(&*reply, &TagReaderReply::Finished, this, [reply, connection]() {
      QObject::disconnect(*connection);
    }, Qt::QueuedConnection);
  }

}
