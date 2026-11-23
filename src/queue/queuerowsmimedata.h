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

#ifndef QUEUEROWSMIMEDATA_H
#define QUEUEROWSMIMEDATA_H

#include "config.h"

#include <QObject>
#include <QList>
#include <QMimeData>
#include <QModelIndexList>
#include <QPersistentModelIndex>
#include <QPointer>

class Queue;

// The rows dragged from a queue, they are kept as persistent indexes since the queue can change during the drag.
class QueueRowsMimeData : public QMimeData {
  Q_OBJECT

 public:
  explicit QueueRowsMimeData(Queue *queue, const QModelIndexList &indexes, QObject *parent = nullptr);

  // The queue the rows were dragged from, null if it was deleted during the drag.
  Queue *queue() const;

  // The rows of the dragged items now, rows which were removed during the drag are left out.
  QList<int> rows() const;

 private:
  QPointer<Queue> queue_;
  QList<QPersistentModelIndex> indexes_;
};

#endif  // QUEUEROWSMIMEDATA_H
