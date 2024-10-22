/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TAGREADERLOADCOVERDATAREPLY_H
#define TAGREADERLOADCOVERDATAREPLY_H

#include <QByteArray>
#include <QString>
#include <QSharedPointer>

#include "tagreaderreply.h"
#include "tagreaderresult.h"

class TagReaderLoadCoverDataReply : public TagReaderReply {
  Q_OBJECT

 public:
  explicit TagReaderLoadCoverDataReply(const QString &_filename, QObject *parent = nullptr);

  void Finish() override;

  QByteArray data() const { return data_; }
  void set_data(const QByteArray &data) { data_ = data; }

 Q_SIGNALS:
  void Finished(const QString &filename, const QByteArray &data, const TagReaderResult &result);

 private Q_SLOTS:
  void EmitFinished() override;

 private:
  QByteArray data_;
};

using TagReaderLoadCoverDataReplyPtr = QSharedPointer<TagReaderLoadCoverDataReply>;

#endif  // TAGREADERLOADCOVERDATAREPLY_H
