/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QWidget>
#include <QMimeData>
#include <QTreeView>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QtEvents>

#include "autoexpandingtreeview.h"
#include "core/mimedata.h"

namespace {
constexpr int kRowsToShow = 50;
}

AutoExpandingTreeView::AutoExpandingTreeView(QWidget *parent)
    : QTreeView(parent),
      auto_open_(false),
      expand_on_reset_(false),
      add_on_double_click_(true),
      ignore_next_click_(false) {

  setExpandsOnDoubleClick(true);
  setAnimated(true);

  QObject::connect(this, &AutoExpandingTreeView::expanded, this, &AutoExpandingTreeView::ItemExpanded);
  QObject::connect(this, &AutoExpandingTreeView::clicked, this, &AutoExpandingTreeView::ItemClicked);
  QObject::connect(this, &AutoExpandingTreeView::doubleClicked, this, &AutoExpandingTreeView::ItemDoubleClicked);

}

void AutoExpandingTreeView::reset() {
  QTreeView::reset();

  // Expand nodes in the tree until we have about 50 rows visible in the view
  if (auto_open_ && expand_on_reset_) {
    RecursivelyExpandSlot(rootIndex());
  }
}

void AutoExpandingTreeView::RecursivelyExpandSlot(const QModelIndex &idx) {
  int rows = model()->rowCount(idx);
  RecursivelyExpand(idx, &rows);
}

bool AutoExpandingTreeView::RecursivelyExpand(const QModelIndex &idx, int *count) {

  if (!CanRecursivelyExpand(idx)) {
    return true;
  }

  if (model()->canFetchMore(idx)) {
    model()->fetchMore(idx);
  }

  int children = model()->rowCount(idx);
  if (*count + children > kRowsToShow) {
    return false;
  }

  expand(idx);
  *count += children;

  for (int i = 0; i < children; ++i) {
    if (!RecursivelyExpand(model()->index(i, 0, idx), count)) {
      return false;
    }
  }

  return true;

}

void AutoExpandingTreeView::ItemExpanded(const QModelIndex &idx) {
  if (model()->rowCount(idx) == 1 && auto_open_)
    expand(model()->index(0, 0, idx));
}

void AutoExpandingTreeView::ItemClicked(const QModelIndex &idx) {

  if (ignore_next_click_) {
    ignore_next_click_ = false;
    return;
  }

  setExpanded(idx, !isExpanded(idx));

}

void AutoExpandingTreeView::ItemDoubleClicked(const QModelIndex &idx) {

  ignore_next_click_ = true;

  if (add_on_double_click_) {
    QMimeData *q_mimedata = model()->mimeData(QModelIndexList() << idx);
    if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
      mimedata->from_doubleclick_ = true;
    }
    Q_EMIT AddToPlaylistSignal(q_mimedata);
  }

}

void AutoExpandingTreeView::mousePressEvent(QMouseEvent *event) {

  if (event->modifiers() != Qt::NoModifier) {
    ignore_next_click_ = true;
  }

  QTreeView::mousePressEvent(event);

  //enqueue to playlist with middleClick
  if (event->button() == Qt::MiddleButton) {
    QMimeData *q_mimedata = model()->mimeData(selectedIndexes());
    if (MimeData *mimedata = qobject_cast<MimeData*>(q_mimedata)) {
      mimedata->enqueue_now_ = true;
    }
    Q_EMIT AddToPlaylistSignal(q_mimedata);
  }

}

void AutoExpandingTreeView::mouseDoubleClickEvent(QMouseEvent *event) {

  State p_state = state();
  QModelIndex idx = indexAt(event->pos());

  QTreeView::mouseDoubleClickEvent(event);

  // If the p_state was the "AnimatingState", then the base class's
  // "mouseDoubleClickEvent" method just did nothing, hence the "doubleClicked" signal is not emitted. So let's do it ourselves.
  if (idx.isValid() && p_state == AnimatingState) {
    Q_EMIT doubleClicked(idx);
  }

}

void AutoExpandingTreeView::keyPressEvent(QKeyEvent *e) {

  switch (e->key()) {
    case Qt::Key_Backspace:
    case Qt::Key_Escape:{
      Q_EMIT FocusOnFilterSignal(e);
      e->accept();
      break;
    }
    case Qt::Key_Left:{
      // Set focus on the root of the current branch
      const QModelIndex idx = currentIndex();
      if (idx.isValid() && idx.parent() != rootIndex() && (!isExpanded(idx) || model()->rowCount(idx) == 0)) {
        setCurrentIndex(idx.parent());
        setFocus();
        e->accept();
      }
      break;
    }
    default:
      break;
  }

  QTreeView::keyPressEvent(e);

}

void AutoExpandingTreeView::UpAndFocus() {
  setCurrentIndex(moveCursor(QAbstractItemView::MoveUp, Qt::NoModifier));
  setFocus();
}

void AutoExpandingTreeView::DownAndFocus() {
  setCurrentIndex(moveCursor(QAbstractItemView::MoveDown, Qt::NoModifier));
  setFocus();
}
