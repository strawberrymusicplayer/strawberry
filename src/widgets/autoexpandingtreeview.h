/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef AUTOEXPANDINGTREEVIEW_H
#define AUTOEXPANDINGTREEVIEW_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QTreeView>

class QMimeData;
class QWidget;
class QModelIndex;
class QKeyEvent;
class QMouseEvent;

class AutoExpandingTreeView : public QTreeView {
  Q_OBJECT

 public:
  explicit AutoExpandingTreeView(QWidget *parent = nullptr);

  static const int kRowsToShow;

  void SetAutoOpen(bool v) { auto_open_ = v; }
  void SetExpandOnReset(bool v) { expand_on_reset_ = v; }
  void SetAddOnDoubleClick(bool v) { add_on_double_click_ = v; }

public slots:
  void RecursivelyExpand(const QModelIndex &idx);
  void UpAndFocus();
  void DownAndFocus();

signals:
  void AddToPlaylistSignal(QMimeData *data);
  void FocusOnFilterSignal(QKeyEvent *event);

protected:
  // QAbstractItemView
  void reset();

  // QWidget
  void mousePressEvent(QMouseEvent *event);
  void mouseDoubleClickEvent(QMouseEvent *event);
  void keyPressEvent(QKeyEvent *event);

  virtual bool CanRecursivelyExpand(const QModelIndex &idx) const { Q_UNUSED(idx); return true; }

private slots:
  void ItemExpanded(const QModelIndex &idx);
  void ItemClicked(const QModelIndex &idx);
  void ItemDoubleClicked(const QModelIndex &idx);

private:
  bool RecursivelyExpand(const QModelIndex &idx, int *count);

private:
  bool auto_open_;
  bool expand_on_reset_;
  bool add_on_double_click_;
  bool ignore_next_click_;
};

#endif  // AUTOEXPANDINGTREEVIEW_H

