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

#include <QObject>
#include <QString>
#include <QSharedPointer>

#include "credentialsreply.h"
#include "credentialsresult.h"

CredentialsReply::CredentialsReply(const QString &service, QObject *parent)
    : QObject(parent),
      service_(service),
      finished_(false) {}

QSharedPointer<CredentialsReply> CredentialsReply::Create(const QString &service) {

  return QSharedPointer<CredentialsReply>(new CredentialsReply(service), &QObject::deleteLater);

}

void CredentialsReply::Finish(const CredentialsResult &credentials_result) {

  QMetaObject::invokeMethod(this, [this, credentials_result]() {
    result_ = credentials_result;
    finished_ = true;
    Q_EMIT Finished();
  }, Qt::QueuedConnection);

}
