/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CREDENTIALSREPLY_H
#define CREDENTIALSREPLY_H

#include <QObject>
#include <QString>
#include <QSharedPointer>

#include "credentialsresult.h"

// Reply for an asynchronous credentials manager request.
// Finished is emitted in the thread the reply was created in, after the request is finished.
class CredentialsReply : public QObject {
  Q_OBJECT

 public:
  explicit CredentialsReply(const QString &service, QObject *parent = nullptr);

  // Creates a reply which is deleted with deleteLater() in its own thread when the last reference is released.
  static QSharedPointer<CredentialsReply> Create(const QString &service);

  QString service() const { return service_; }
  bool finished() const { return finished_; }
  CredentialsResult result() const { return result_; }
  bool success() const { return result_.success(); }
  QString password() const { return result_.password; }
  QString error() const { return result_.error; }

  // Can be called from any thread, the result is set and Finished is emitted in the thread of this reply.
  void Finish(const CredentialsResult &credentials_result);

 Q_SIGNALS:
  void Finished();

 private:
  const QString service_;
  bool finished_;
  CredentialsResult result_;
};

using CredentialsReplyPtr = QSharedPointer<CredentialsReply>;

#endif  // CREDENTIALSREPLY_H
