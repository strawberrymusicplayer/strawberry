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

#ifndef STREAMTAGREADER_H
#define STREAMTAGREADER_H

#include <taglib/tiostream.h>
#include <google/sparsetable>

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "includes/scoped_ptr.h"
#include "core/networkaccessmanager.h"

#if TAGLIB_MAJOR_VERSION >= 2
using TagLibLengthType = size_t;
using TagLibUOffsetType = TagLib::offset_t;
using TagLibOffsetType = TagLib::offset_t;
#else
using TagLibLengthType = ulong;
using TagLibUOffsetType = ulong;
using TagLibOffsetType = long;
#endif

class StreamTagReader : public TagLib::IOStream {

 public:
  explicit StreamTagReader(const QUrl &url,
                           const QString &filename,
                           const quint64 length,
                           const QString &token_type,
                           const QString &access_token);

  virtual TagLib::FileName name() const override;
  virtual TagLib::ByteVector readBlock(const TagLibLengthType length) override;
  virtual void writeBlock(const TagLib::ByteVector &data) override;
  virtual void insert(const TagLib::ByteVector &data, const TagLibUOffsetType start, const TagLibLengthType replace) override;
  virtual void removeBlock(const TagLibUOffsetType start, const TagLibLengthType length) override;
  virtual bool readOnly() const override;
  virtual bool isOpen() const override;
  virtual void seek(const TagLibOffsetType offset, const TagLib::IOStream::Position position) override;
  virtual void clear() override;
  virtual TagLibOffsetType tell() const override;
  virtual TagLibOffsetType length() override;
  virtual void truncate(const TagLibOffsetType length) override;

  google::sparsetable<char>::size_type cached_bytes() const {
    return cache_.num_nonempty();
  }

  int num_requests() const { return num_requests_; }

  void PreCache();

 private:
  bool CheckCache(const uint start, const uint end);
  void FillCache(const uint start, const TagLib::ByteVector &data);
  TagLib::ByteVector GetCache(const uint start, const uint end);

 private:
  const QUrl url_;
  const QString filename_;
  const QByteArray encoded_filename_;
  const TagLibLengthType length_;
  const QString token_type_;
  const QString access_token_;

  ScopedPtr<NetworkAccessManager> network_;

  TagLibLengthType cursor_;
  google::sparsetable<char> cache_;
  int num_requests_;
};

#endif  // STREAMTAGREADER_H
