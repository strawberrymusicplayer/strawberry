/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "redirectfollower.h"

RedirectFollower::RedirectFollower(QNetworkReply *first_reply, int max_redirects) : QObject(nullptr), current_reply_(first_reply), redirects_remaining_(max_redirects) {
  ConnectReply(first_reply);
}

void RedirectFollower::ConnectReply(QNetworkReply *reply) {

  connect(reply, SIGNAL(readyRead()), SLOT(ReadyRead()));
  connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SIGNAL(error(QNetworkReply::NetworkError)));
  connect(reply, SIGNAL(downloadProgress(qint64,qint64)), SIGNAL(downloadProgress(qint64,qint64)));
  connect(reply, SIGNAL(uploadProgress(qint64,qint64)), SIGNAL(uploadProgress(qint64,qint64)));
  connect(reply, SIGNAL(finished()), SLOT(ReplyFinished()));

}

void RedirectFollower::ReadyRead() {

  // Don't re-emit this signal for redirect replies.
  if (current_reply_->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
    return;
  }

  emit readyRead();

}

void RedirectFollower::ReplyFinished() {

  current_reply_->deleteLater();

  if (current_reply_->attribute(QNetworkRequest::RedirectionTargetAttribute).isValid()) {
    if (redirects_remaining_-- == 0) {
      emit finished();
      return;
    }

    const QUrl next_url = current_reply_->url().resolved(current_reply_->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl());

    QNetworkRequest req(current_reply_->request());
    req.setUrl(next_url);

    current_reply_ = current_reply_->manager()->get(req);
    ConnectReply(current_reply_);
    return;
  }

  emit finished();

}

