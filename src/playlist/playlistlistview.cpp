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

#include "config.h"

#include <QAbstractItemModel>
#include <QWidget>
#include <QFlags>
#include <QFont>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QPaintEvent>

#include "playlistlistview.h"

PlaylistListView::PlaylistListView(QWidget *parent)
    : AutoExpandingTreeView(parent) {}

void PlaylistListView::paintEvent(QPaintEvent *event) {

  if (model()->rowCount() <= 0) {
    QPainter p(viewport());
    QRect rect(viewport()->rect());

    p.setPen(palette().color(QPalette::Disabled, QPalette::Text));

    QFont bold_font;
    bold_font.setBold(true);
    p.setFont(bold_font);

    p.drawText(rect, Qt::AlignHCenter | Qt::TextWordWrap,
               tr("\n\n"
                  "You can favorite playlists by clicking the star icon next "
                  "to a playlist name\n\n"
                  "Favorited playlists will be saved here"));
  }
  else {
    AutoExpandingTreeView::paintEvent(event);
  }

}

bool PlaylistListView::ItemsSelected() const {
  return selectionModel()->selectedRows().count() > 0;
}

void PlaylistListView::selectionChanged(const QItemSelection&, const QItemSelection&) {
  emit ItemsSelectedChanged(selectionModel()->selectedRows().count() > 0);
}
