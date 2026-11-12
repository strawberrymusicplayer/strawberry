// SPDX-License-Identifier: GPL-3.0-or-later

#include "scantaskcontrols_android.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QStringList>
#include <QVBoxLayout>
#include "core/taskmanager.h"
#include "utilities/timeutils.h"

ScanTaskControls::ScanTaskControls(SharedPtr<TaskManager> task_manager, QWidget *parent)
    : QWidget(parent), task_manager_(task_manager), status_(new QLabel(this)),
      stop_(new QPushButton(tr("Stop scan"), this)), timer_(new QTimer(this)) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  status_->setWordWrap(true);
  status_->setTextFormat(Qt::PlainText);
  status_->setObjectName(QStringLiteral("scan_status"));
  stop_->setObjectName(QStringLiteral("stop_scan"));
  stop_->setMinimumHeight(48);
  stop_->setAutoDefault(false);
  layout->addWidget(status_);
  layout->addWidget(stop_);
  QObject::connect(stop_, &QPushButton::clicked, this, [this]() {
    for (const TaskManager::Task &task : task_manager_->GetTasks()) {
      if (task.cancellable) task_manager_->RequestTaskCancellation(task.id);
    }
  });
  QObject::connect(&*task_manager_, &TaskManager::TasksChanged, this, &ScanTaskControls::Refresh);
  timer_->setInterval(1000);
  QObject::connect(timer_, &QTimer::timeout, this, &ScanTaskControls::Refresh);
  Refresh();
}

void ScanTaskControls::Refresh() {
  QStringList details;
  bool can_stop = false;
  int scans = 0;
  for (const TaskManager::Task &task : task_manager_->GetTasks()) {
    if (!task.cancellable) continue;
    ++scans;
    QString detail = task.detail;
    if (task.cancellation_requested) detail = tr("Stopping scan after the current operation...");
    else {
      can_stop = true;
      if (task.progress_max > 0) {
        detail += tr(" - %1 / %2 entries").arg(task.progress).arg(task.progress_max);
        const int remaining = task.EstimatedSecondsRemaining();
        if (task.progress >= task.progress_max) detail += tr(" - finishing scan...");
        else detail += remaining < 0 ? tr(" - estimating time...") : tr(" - about %1 remaining").arg(Utilities::PrettyTime(remaining));
      }
    }
    details << detail;
  }
  status_->setText(details.join(u'\n'));
  stop_->setEnabled(can_stop);
  stop_->setText(scans > 1 ? tr("Stop scans") : tr("Stop scan"));
  setVisible(!details.isEmpty());
  if (details.isEmpty()) timer_->stop();
  else if (!timer_->isActive()) timer_->start();
  updateGeometry();
}
