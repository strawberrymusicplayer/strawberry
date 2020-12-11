/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "widgets/autoexpandingtreeview.h"

class QPaintEvent;

class PlaylistListView : public AutoExpandingTreeView {
  Q_OBJECT

 public:
  explicit PlaylistListView(QWidget *parent = nullptr);

  bool ItemsSelected() const;

 signals:
  void ItemsSelectedChanged(bool);

 protected:
  // QWidget
  void paintEvent(QPaintEvent *event) override;
  void selectionChanged(const QItemSelection&, const QItemSelection&) override;
};

#endif  // PLAYLISTVIEW_H
