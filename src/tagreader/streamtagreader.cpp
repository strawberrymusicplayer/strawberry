/*
 * Strawberry Music Player
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QEventLoop>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslError>

#include "core/logging.h"
#include "core/networkaccessmanager.h"

#include "streamtagreader.h"

namespace {
constexpr TagLibLengthType kTagLibPrefixCacheBytes = 64UL * 1024UL;
constexpr TagLibLengthType kTagLibSuffixCacheBytes = 8UL * 1024UL;
}  // namespace

StreamTagReader::StreamTagReader(const QUrl &url,
                                 const QString &filename,
                                 const quint64 length,
                                 const QString &token_type,
                                 const QString &access_token)
    : url_(url),
      filename_(filename),
      encoded_filename_(filename_.toUtf8()),
      length_(static_cast<TagLibLengthType>(length)),
      token_type_(token_type),
      access_token_(access_token),
      network_(new NetworkAccessManager),
      cursor_(0),
      cache_(length),
      num_requests_(0) {

  network_->setAutoDeleteReplies(true);

}

TagLib::FileName StreamTagReader::name() const { return encoded_filename_.data(); }

TagLib::ByteVector StreamTagReader::readBlock(const TagLibLengthType length) {

  const uint start = static_cast<uint>(cursor_);
  const uint end = static_cast<uint>(std::min(cursor_ + length - 1, length_ - 1));

  if (end < start) {
    return TagLib::ByteVector();
  }

  if (CheckCache(start, end)) {
    const TagLib::ByteVector cached = GetCache(start, end);
    cursor_ += static_cast<TagLibLengthType>(cached.size());
    return cached;
  }

  QNetworkRequest network_request(url_);
  if (!token_type_.isEmpty() && !access_token_.isEmpty()) {
    network_request.setRawHeader("Authorization", token_type_.toUtf8() + " " + access_token_.toUtf8());
  }
  network_request.setRawHeader("Range", QStringLiteral("bytes=%1-%2").arg(start).arg(end).toUtf8());
  network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  QNetworkReply *reply = network_->get(network_request);
  ++num_requests_;

  QEventLoop event_loop;
  QObject::connect(reply, &QNetworkReply::finished, &event_loop, &QEventLoop::quit);
  event_loop.exec();

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "Unable to get tags from stream for" << url_ << "got error:" << reply->errorString();
    return TagLib::ByteVector();
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http_status_code >= 400) {
      qLog(Error) << "Unable to get tags from stream for" << url_ << "received HTTP code" << http_status_code;
      return TagLib::ByteVector();
    }
  }

  const QByteArray data = reply->readAll();
  const TagLib::ByteVector bytes(data.data(), static_cast<uint>(data.size()));
  cursor_ += static_cast<TagLibLengthType>(data.size());

  FillCache(start, bytes);

  return bytes;

}

void StreamTagReader::writeBlock(const TagLib::ByteVector &data) {
  Q_UNUSED(data);
}

void StreamTagReader::insert(const TagLib::ByteVector &data, const TagLibUOffsetType start, const TagLibLengthType replace) {
  Q_UNUSED(data)
  Q_UNUSED(start)
  Q_UNUSED(replace)
}

void StreamTagReader::removeBlock(const TagLibUOffsetType start, const TagLibLengthType length) {
  Q_UNUSED(start)
  Q_UNUSED(length)
}

bool StreamTagReader::readOnly() const { return true; }

bool StreamTagReader::isOpen() const { return true; }

void StreamTagReader::seek(const TagLibOffsetType offset, const TagLib::IOStream::Position position) {

  switch (position) {
    case TagLib::IOStream::Beginning:
      cursor_ = static_cast<TagLibLengthType>(offset);
      break;

    case TagLib::IOStream::Current:
      cursor_ = std::min(cursor_ + static_cast<TagLibLengthType>(offset), length_);
      break;

    case TagLib::IOStream::End:
      // This should really not have qAbs(), but OGG reading needs it.
      cursor_ = std::max(static_cast<TagLibLengthType>(0), length_ - qAbs(static_cast<TagLibLengthType>(offset)));
      break;
  }

}

void StreamTagReader::clear() { cursor_ = 0; }

TagLibOffsetType StreamTagReader::tell() const { return static_cast<TagLibOffsetType>(cursor_); }

TagLibOffsetType StreamTagReader::length() { return static_cast<TagLibOffsetType>(length_); }

void StreamTagReader::truncate(const TagLibOffsetType length) {
  Q_UNUSED(length)
}

bool StreamTagReader::CheckCache(const uint start, const uint end) {

  for (uint i = start; i <= end; ++i) {
    if (!cache_.test(i)) {
      return false;
    }
  }

  return true;

}

void StreamTagReader::FillCache(const uint start, const TagLib::ByteVector &data) {

  for (uint i = 0; i < data.size(); ++i) {
    cache_.set(start + i, data[static_cast<int>(i)]);
  }

}

TagLib::ByteVector StreamTagReader::GetCache(const uint start, const uint end) {

  const uint size = end - start + 1U;
  TagLib::ByteVector data(size);
  for (uint i = 0; i < size; ++i) {
    data[static_cast<int>(i)] = cache_.get(start + i);
  }

  return data;

}

void StreamTagReader::PreCache() {

  // For reading the tags of an MP3, TagLib tends to request:
  // 1. The first 1024 bytes
  // 2. Somewhere between the first 2KB and first 60KB
  // 3. The last KB or two.
  // 4. Somewhere in the first 64KB again
  //
  // OGG Vorbis may read the last 4KB.
  //
  // So, if we precache the first 64KB and the last 8KB we should be sorted :-)
  // Ideally, we would use bytes=0-655364,-8096 but Google Drive does not seem
  // to support multipart byte ranges yet so we have to make do with two requests.

  seek(0, TagLib::IOStream::Beginning);
  readBlock(kTagLibPrefixCacheBytes);
  seek(kTagLibSuffixCacheBytes, TagLib::IOStream::End);
  readBlock(kTagLibSuffixCacheBytes);
  clear();

}
