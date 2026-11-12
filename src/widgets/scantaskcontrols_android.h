// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SCANTASKCONTROLS_ANDROID_H
#define SCANTASKCONTROLS_ANDROID_H

#include <QWidget>
#include <QCoreApplication>
#include "includes/shared_ptr.h"

class QLabel;
class QPushButton;
class QTimer;
class TaskManager;

class ScanTaskControls : public QWidget {
  Q_DECLARE_TR_FUNCTIONS(ScanTaskControls)

 public:
  explicit ScanTaskControls(SharedPtr<TaskManager> task_manager, QWidget *parent = nullptr);

 private:
  void Refresh();
  SharedPtr<TaskManager> task_manager_;
  QLabel *status_;
  QPushButton *stop_;
  QTimer *timer_;
};

#endif
