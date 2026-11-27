// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LISTSCROLLING_ANDROID_H
#define LISTSCROLLING_ANDROID_H

#include <QObject>

class AndroidListScrolling : public QObject {
 public:
  explicit AndroidListScrolling(QObject *parent);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;
};

#endif
