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

#include "config.h"

#include <QListView>
#include <QContextMenuEvent>

#include "core/logging.h"
#include "core/mimedata.h"
#include "smartplaylistsmodel.h"
#include "smartplaylistsview.h"
#include "smartplaylistwizard.h"

SmartPlaylistsView::SmartPlaylistsView(QWidget *_parent) : QListView(_parent) {

  setAttribute(Qt::WA_MacShowFocusRect, false);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setSelectionMode(QAbstractItemView::SingleSelection);

}

SmartPlaylistsView::~SmartPlaylistsView() = default;

void SmartPlaylistsView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
  Q_UNUSED(selected)
  Q_UNUSED(deselected)
  Q_EMIT ItemsSelectedChanged();
}

void SmartPlaylistsView::contextMenuEvent(QContextMenuEvent *e) {

  Q_EMIT RightClicked(e->globalPos(), indexAt(e->pos()));
  e->accept();

}
