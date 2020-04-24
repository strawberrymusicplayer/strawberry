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
#include <QNetworkReply>
#include <QTimerEvent>

#include "networktimeouts.h"

NetworkTimeouts::NetworkTimeouts(const int timeout_msec, QObject *parent)
    : QObject(parent), timeout_msec_(timeout_msec) {}

void NetworkTimeouts::AddReply(QNetworkReply *reply) {

  if (timers_.contains(reply)) return;

  connect(reply, SIGNAL(destroyed()), SLOT(ReplyFinished()));
  connect(reply, SIGNAL(finished()), SLOT(ReplyFinished()));
  timers_[reply] = startTimer(timeout_msec_);

}

void NetworkTimeouts::ReplyFinished() {

  QNetworkReply *reply = reinterpret_cast<QNetworkReply*>(sender());
  if (timers_.contains(reply)) {
    killTimer(timers_.take(reply));
  }

}

void NetworkTimeouts::timerEvent(QTimerEvent *e) {

  QNetworkReply *reply = timers_.key(e->timerId());
  if (reply) {
    reply->abort();
  }

}
