// SPDX-License-Identifier: GPL-3.0-or-later

#include "scrollarea_android.h"

#include <QAbstractScrollArea>
#include <QScroller>
#include <QScrollerProperties>
#include <QWidget>

namespace AndroidScrollArea {

void EnableForContents(QAbstractScrollArea *area) {
  QWidget *viewport = area->viewport();
  if (QScroller::hasScroller(viewport)) return;

  QScroller *scroller = QScroller::scroller(viewport);
  QScrollerProperties properties = scroller->scrollerProperties();
  properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
  properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  scroller->setScrollerProperties(properties);
  QScroller::grabGesture(viewport, QScroller::LeftMouseButtonGesture);
}

}  // namespace AndroidScrollArea
