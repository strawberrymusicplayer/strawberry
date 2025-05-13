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

#include "config.h"

#include <QWidget>
#include <QFont>
#include <QMimeData>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QPaintEvent>

#include "widgets/autoexpandingtreeview.h"
#include "playlistlistview.h"
#include "playlist.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kDragHoverTimeout = 500;
}

PlaylistListView::PlaylistListView(QWidget *parent) : AutoExpandingTreeView(parent) {}

void PlaylistListView::paintEvent(QPaintEvent *event) {

  if (model()->rowCount() <= 0) {
    QPainter p(viewport());
    QRect rect(viewport()->rect());

    p.setPen(palette().color(QPalette::Disabled, QPalette::Text));

    QFont bold_font;
    bold_font.setBold(true);
    p.setFont(bold_font);

    p.drawText(rect,
               Qt::AlignHCenter | Qt::TextWordWrap,
               "\n\n"_L1 +
               tr("You can favorite playlists by clicking the star icon next to a playlist name") +
               "\n\n"_L1 +
               tr("Favorited playlists will be saved here"));
  }
  else {
    AutoExpandingTreeView::paintEvent(event);
  }

}

bool PlaylistListView::ItemsSelected() const {
  return selectionModel()->selectedRows().count() > 0;
}

void PlaylistListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {

  Q_UNUSED(selected)
  Q_UNUSED(deselected)

  Q_EMIT ItemsSelectedChanged(selectionModel()->selectedRows().count() > 0);

}

void PlaylistListView::dragEnterEvent(QDragEnterEvent *e) {

  if (e->mimeData()->hasFormat(QLatin1String(Playlist::kRowsMimetype))) {
    e->acceptProposedAction();
  }
  else {
    AutoExpandingTreeView::dragEnterEvent(e);
  }

}

void PlaylistListView::dragMoveEvent(QDragMoveEvent *e) {

  QModelIndex drag_hover_tab_ = indexAt(e->position().toPoint());

  if (e->mimeData()->hasFormat(QLatin1String(Playlist::kRowsMimetype))) {
    if (drag_hover_tab_ != currentIndex()) {
      e->setDropAction(Qt::CopyAction);
      e->accept(visualRect(drag_hover_tab_));
      setCurrentIndex(drag_hover_tab_);
      if (drag_hover_timer_.isActive()) {
        drag_hover_timer_.stop();
      }
      drag_hover_timer_.start(kDragHoverTimeout, this);
    }
  }
  else {
    AutoExpandingTreeView::dragMoveEvent(e);
  }

}

void PlaylistListView::dragLeaveEvent(QDragLeaveEvent *e) {

  if (drag_hover_timer_.isActive()) {
    drag_hover_timer_.stop();
  }
  AutoExpandingTreeView::dragLeaveEvent(e);

}

void PlaylistListView::timerEvent(QTimerEvent *e) {

  QTreeView::timerEvent(e);
  if (e->timerId() == drag_hover_timer_.timerId()) {
    drag_hover_timer_.stop();
    if (currentIndex().isValid()) {
      Q_EMIT doubleClicked(currentIndex());
    }
  }

}

void PlaylistListView::dropEvent(QDropEvent *e) {

  if (e->mimeData()->hasFormat(QLatin1String(Playlist::kRowsMimetype))) {
    if (drag_hover_timer_.isActive()) {
      drag_hover_timer_.stop();
    }
    if (currentIndex().isValid()) {
      Q_EMIT ItemMimeDataDroppedSignal(currentIndex(), e->mimeData());
    }
  }
  else  {
    AutoExpandingTreeView::dropEvent(e);
  }

}
