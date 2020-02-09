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

#include "config.h"

#include <QObject>
#include <QAbstractItemView>
#include <QScrollBar>
#include <QEvent>

#include "forcescrollperpixel.h"

ForceScrollPerPixel::ForceScrollPerPixel(QAbstractItemView *item_view, QObject *parent)
  : QObject(parent), item_view_(item_view) {
  item_view_->installEventFilter(this);
}

bool ForceScrollPerPixel::eventFilter(QObject *object, QEvent *event) {
  if (object == item_view_ && event->type() != QEvent::Destroy && event->type() != QEvent::WinIdChange) {
      //event->type() != QEvent::AccessibilityPrepare)
    item_view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    item_view_->verticalScrollBar()->setSingleStep(20);
  }

  return QObject::eventFilter(object, event);
}
