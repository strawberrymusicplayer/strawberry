/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTLISTVIEW_H
#define PLAYLISTLISTVIEW_H

#include "config.h"

#include <QBasicTimer>
#include <QObject>
#include <QWidget>
#include <QString>

#include "widgets/autoexpandingtreeview.h"

class QPaintEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
class QTimerEvent;

class PlaylistListView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit PlaylistListView(QWidget *parent = nullptr);

  bool ItemsSelected() const;

 Q_SIGNALS:
  void ItemsSelectedChanged(const bool);
  void ItemMimeDataDroppedSignal(const QModelIndex &proxy_idx, const QMimeData *q_mimedata);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event) override;
  void selectionChanged(const QItemSelection&, const QItemSelection&) override;

  void dragEnterEvent(QDragEnterEvent *e) override;
  void dragMoveEvent(QDragMoveEvent *e) override;
  void dragLeaveEvent(QDragLeaveEvent *e) override;
  void dropEvent(QDropEvent *e) override;
  void timerEvent(QTimerEvent *e) override;

 private:
  QBasicTimer drag_hover_timer_;
};

#endif  // PLAYLISTLISTVIEW_H
