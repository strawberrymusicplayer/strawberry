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

#include <utility>

#include "longpresshandler.h"

LongPressHandler::LongPressHandler(const int interval, const int drag_distance, QObject *parent)
    : QObject(parent),
      drag_distance_(drag_distance) {

  timer_.setInterval(interval);
  timer_.setSingleShot(true);
  timer_.setTimerType(Qt::PreciseTimer);
  QObject::connect(&timer_, &QTimer::timeout, this, [this]() {
    triggered_ = true;
    Q_EMIT LongPressed(position_);
  });

}

void LongPressHandler::Press(const QPoint &position) {

  position_ = position;
  triggered_ = false;
  timer_.start();

}

bool LongPressHandler::Move(const QPoint &position) {

  if ((position - position_).manhattanLength() > drag_distance_) Cancel();
  return triggered_;

}

bool LongPressHandler::Release() {

  Cancel();
  return std::exchange(triggered_, false);

}

void LongPressHandler::Cancel() {
  timer_.stop();
}
