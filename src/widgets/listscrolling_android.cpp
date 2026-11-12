// SPDX-License-Identifier: GPL-3.0-or-later

#include "listscrolling_android.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QHeaderView>
#include <QScroller>
#include <QScrollerProperties>
#include <QVariant>

AndroidListScrolling::AndroidListScrolling(QObject *parent) : QObject(parent) {
  parent->installEventFilter(this);
}

bool AndroidListScrolling::eventFilter(QObject *object, QEvent *event) {
  if (event->type() != QEvent::Show) return false;
  auto *view = qobject_cast<QAbstractItemView*>(object);
  if (!view || qobject_cast<QHeaderView*>(view)) return false;
  for (QWidget *ancestor = view->parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
    if (qobject_cast<QComboBox*>(ancestor)) return false;
  }
  QWidget *viewport = view->viewport();
  if (QScroller::hasScroller(viewport)) return false;
  view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
  // A finger drag scrolls rather than starting desktop drag-and-drop or reordering.
  view->setDragEnabled(false);
  auto *scroller = QScroller::scroller(viewport);
  QScrollerProperties properties = scroller->scrollerProperties();
  properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
  properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  scroller->setScrollerProperties(properties);
  QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture);
  return false;
}
