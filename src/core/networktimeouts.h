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

#ifndef NETWORKTIMEOUTS_H
#define NETWORKTIMEOUTS_H

#include "config.h"

#include <QObject>
#include <QHash>

class QNetworkReply;
class QTimerEvent;

class NetworkTimeouts : public QObject {
  Q_OBJECT

 public:
  explicit NetworkTimeouts(const int timeout_msec, QObject *parent = nullptr);

  void AddReply(QNetworkReply *reply);
  void SetTimeout(const int msec) { timeout_msec_ = msec; }

 protected:
  void timerEvent(QTimerEvent *e) override;

 private Q_SLOTS:
  void ReplyFinished();

 private:
  int timeout_msec_;
  QHash<QNetworkReply*, int> timers_;

};

#endif  // NETWORKTIMEOUTS_H
