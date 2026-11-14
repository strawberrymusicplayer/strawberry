// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCROLLAREA_ANDROID_H
#define SCROLLAREA_ANDROID_H

#include <QObject>
#include <QList>

class QEvent;
class QScrollArea;
class QScroller;
class QWidget;

class AndroidScrollArea : public QObject {
 public:
  explicit AndroidScrollArea(QScrollArea *area);
  void AddGestureTarget(QWidget *target);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 private:
  QScrollArea *area_;
  QList<QScroller*> scrollers_;
};

#endif
