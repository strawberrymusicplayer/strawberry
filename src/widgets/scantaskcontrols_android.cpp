// SPDX-License-Identifier: GPL-3.0-or-later

#include "scantaskcontrols_android.h"

#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QStringList>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include "core/taskmanager.h"
#include "utilities/timeutils.h"

ScanTaskControls::ScanTaskControls(SharedPtr<TaskManager> task_manager, QWidget *parent)
    : QWidget(parent), task_manager_(task_manager), status_(new QLabel(this)),
      progress_(new QProgressBar(this)), details_(new QWidget(this)),
      stop_(new QPushButton(tr("Stop"), this)), timer_(new QTimer(this)) {
  setObjectName(QStringLiteral("android_app_header"));
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(6, 4, 6, 4);
  auto *title = new QLabel(QCoreApplication::applicationName(), this);
  title->setObjectName(QStringLiteral("app_title"));
  title->setTextFormat(Qt::PlainText);
  title->setWordWrap(true);
  layout->addWidget(title);
  auto *center = new QVBoxLayout(details_);
  center->setContentsMargins(0, 0, 0, 0);
  status_->setWordWrap(true);
  status_->setAlignment(Qt::AlignCenter);
  status_->setMinimumWidth(0);
  status_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  status_->setTextFormat(Qt::PlainText);
  status_->setObjectName(QStringLiteral("scan_status"));
  progress_->setObjectName(QStringLiteral("scan_progress"));
  progress_->setTextVisible(false);
  progress_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  progress_->setMinimumWidth(0);
  progress_->setMaximumHeight(6);
  stop_->setObjectName(QStringLiteral("stop_scan"));
  stop_->setMinimumHeight(48);
  stop_->setAutoDefault(false);
  stop_->setAccessibleName(tr("Stop scan"));
  center->addWidget(status_);
  center->addWidget(progress_);
  layout->addWidget(details_, 1);
  layout->addWidget(stop_);
  setMinimumHeight(56);
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
  qint64 processed = 0;
  qint64 total = 0;
  bool counting = false;
  for (const TaskManager::Task &task : task_manager_->GetTasks()) {
    if (!task.cancellable) continue;
    ++scans;
    processed += task.progress;
    total += task.progress_max;
    counting = counting || task.progress_max == 0;
    QString detail = task.detail;
    if (task.cancellation_requested) detail = tr("Stopping scan after the current operation...");
    else {
      can_stop = true;
      if (task.progress_max > 0) {
        detail += tr(" - %1/%2").arg(task.progress).arg(task.progress_max);
        const int remaining = task.EstimatedSecondsRemaining();
        if (task.progress >= task.progress_max) detail += tr(" - finishing scan...");
        else detail += remaining < 0 ? tr(" - estimating time...") : tr(" - about %1 remaining").arg(Utilities::PrettyTime(remaining));
      }
    }
    QString name = task.name;
    if (task.progress_max > 0) name += tr(" %1%").arg(qBound(0, static_cast<int>(100.0 * task.progress / task.progress_max), 100));
    details << name + u'\n' + detail;
  }
  status_->setText(details.join(u'\n'));
  stop_->setEnabled(can_stop);
  stop_->setAccessibleName(scans > 1 ? tr("Stop scans") : tr("Stop scan"));
  stop_->setVisible(scans > 0);
  details_->setVisible(scans > 0);
  if (counting) progress_->setRange(0, 0);
  else {
    progress_->setRange(0, 100);
    progress_->setValue(total > 0 ? static_cast<int>(qBound(0.0, 100.0 * processed / total, 100.0)) : 0);
  }
  if (details.isEmpty()) timer_->stop();
  else if (!timer_->isActive()) timer_->start();
  updateGeometry();
}
