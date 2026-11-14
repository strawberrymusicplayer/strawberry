// SPDX-License-Identifier: GPL-3.0-or-later

#include "scrollarea_android.h"

#include <QEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QScrollEvent>
#include <QScroller>
#include <QScrollerProperties>
#include <QVariant>
#include <QWidget>

AndroidScrollArea::AndroidScrollArea(QScrollArea *area) : QObject(area), area_(area) {
  AddGestureTarget(area->viewport());
}

void AndroidScrollArea::AddGestureTarget(QWidget *target) {
  if (QScroller::hasScroller(target)) return;

  target->installEventFilter(this);
  QScroller *scroller = QScroller::scroller(target);
  scrollers_.append(scroller);
  QObject::connect(scroller, &QObject::destroyed, this, [this, scroller] { scrollers_.removeAll(scroller); });
  QScrollerProperties properties = scroller->scrollerProperties();
  properties.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.0);
  properties.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  properties.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  scroller->setScrollerProperties(properties);
  QObject::connect(scroller, &QScroller::stateChanged, this, [this, scroller](QScroller::State state) {
    if (state != QScroller::Pressed) return;
    for (QScroller *other : scrollers_) {
      if (other != scroller) other->stop();
    }
  });
  QScroller::grabGesture(target, QScroller::LeftMouseButtonGesture);
}

bool AndroidScrollArea::eventFilter(QObject *object, QEvent *event) {
  Q_UNUSED(object)
  if (event->type() == QEvent::ScrollPrepare) {
    auto *prepare = static_cast<QScrollPrepareEvent*>(event);
    QScrollBar *horizontal = area_->horizontalScrollBar();
    QScrollBar *vertical = area_->verticalScrollBar();
    prepare->setViewportSize(area_->viewport()->size());
    prepare->setContentPosRange(QRectF(horizontal->minimum(), vertical->minimum(),
                                       horizontal->maximum() - horizontal->minimum(), vertical->maximum() - vertical->minimum()));
    prepare->setContentPos(QPointF(horizontal->value(), vertical->value()));
    prepare->accept();
    return true;
  }
  if (event->type() == QEvent::Scroll) {
    auto *scroll = static_cast<QScrollEvent*>(event);
    area_->horizontalScrollBar()->setValue(qRound(scroll->contentPos().x()));
    area_->verticalScrollBar()->setValue(qRound(scroll->contentPos().y()));
    event->accept();
    return true;
  }
  return false;
}
