/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SMARTPLAYLISTSVIEW_H
#define SMARTPLAYLISTSVIEW_H

#include "config.h"

#include <QListView>
#include <QItemSelection>
#include <QModelIndex>
#include <QPoint>

class SmartPlaylistsView : public QListView {
  Q_OBJECT

 public:
  explicit SmartPlaylistsView(QWidget *parent = nullptr);
  ~SmartPlaylistsView();

 protected:
  void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

 Q_SIGNALS:
  void ItemsSelectedChanged();
  void RightClicked(const QPoint global_pos, const QModelIndex idx);

};

#endif  // SMARTPLAYLISTSVIEW_H
