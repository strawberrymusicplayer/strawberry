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

#ifndef TAGREADERREPLY_H
#define TAGREADERREPLY_H

#include <QObject>
#include <QString>

#include "core/shared_ptr.h"
#include "tagreaderresult.h"

class TagReaderReply : public QObject {
  Q_OBJECT

 public:
  explicit TagReaderReply(const QString &filename, QObject *parent = nullptr);
  virtual ~TagReaderReply() override;

  template<typename T>
  static SharedPtr<T> Create(const QString &filename) {

    SharedPtr<T> reply;
    reply.reset(new T(filename), [](QObject *obj) { obj->deleteLater(); });
    return reply;

  }

  QString filename() const { return filename_; }

  TagReaderResult result() const { return result_; }
  void set_result(const TagReaderResult &result) { result_ = result; }

  bool finished() const { return finished_; }
  bool success() const { return result_.success(); }
  QString error() const { return result_.error_string(); }

  virtual void Finish();

 Q_SIGNALS:
  void Finished(const QString &filename, const TagReaderResult &result);

 protected:
  const QString filename_;
  bool finished_;
  TagReaderResult result_;
};

using TagReaderReplyPtr = SharedPtr<TagReaderReply>;

#endif  // TAGREADERREPLY_H
