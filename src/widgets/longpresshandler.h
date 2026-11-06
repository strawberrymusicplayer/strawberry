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

#ifndef LONGPRESSHANDLER_H
#define LONGPRESSHANDLER_H

#include <QObject>
#include <QPoint>
#include <QTimer>

class LongPressHandler : public QObject {
  Q_OBJECT

 public:
  explicit LongPressHandler(const int interval, const int drag_distance, QObject *parent = nullptr);

  void Press(const QPoint &position);
  bool Move(const QPoint &position);
  bool Release();
  void Cancel();

 Q_SIGNALS:
  void LongPressed(const QPoint position);

 private:
  QTimer timer_;
  const int drag_distance_;
  QPoint position_;
  bool triggered_ = false;
};

#endif  // LONGPRESSHANDLER_H
