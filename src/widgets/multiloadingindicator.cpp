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

#include <QtGlobal>
#include <QWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QPainter>
#include <QSize>
#include <QRect>
#include <QSizePolicy>
#include <QStringList>
#include <QFontMetrics>
#include <QPaintEvent>

#include "core/taskmanager.h"
#include "multiloadingindicator.h"
#include "widgets/busyindicator.h"

const int MultiLoadingIndicator::kVerticalPadding = 4;
const int MultiLoadingIndicator::kHorizontalPadding = 6;
const int MultiLoadingIndicator::kSpacing = 6;

MultiLoadingIndicator::MultiLoadingIndicator(QWidget *parent)
  : QWidget(parent),
    task_manager_(nullptr),
    spinner_(new BusyIndicator(this))
{
  spinner_->move(kHorizontalPadding, kVerticalPadding);
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
}

QSize MultiLoadingIndicator::sizeHint() const {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
  const int width = kHorizontalPadding * 2 + spinner_->sizeHint().width() + kSpacing + fontMetrics().horizontalAdvance(text_);
#else
  const int width = kHorizontalPadding * 2 + spinner_->sizeHint().width() + kSpacing + fontMetrics().width(text_);
#endif
  const int height = kVerticalPadding * 2 + qMax(spinner_->sizeHint().height(), fontMetrics().height());

  return QSize(width, height);
}

void MultiLoadingIndicator::SetTaskManager(TaskManager* task_manager) {
  task_manager_ = task_manager;
  connect(task_manager_, SIGNAL(TasksChanged()), SLOT(UpdateText()));
}

void MultiLoadingIndicator::UpdateText() {

  QList<TaskManager::Task> tasks = task_manager_->GetTasks();

  QStringList strings;
  for (const TaskManager::Task& task : tasks) {
    QString task_text(task.name);
    task_text[0] = task_text[0].toLower();

    if (task.progress_max) {
      int percentage = float(task.progress) / task.progress_max * 100;
      task_text += QString(" %1%").arg(percentage);
    }

    strings << task_text;
  }

  text_ = strings.join(", ");
  if (!text_.isEmpty()) {
    text_[0] = text_[0].toUpper();
    text_ += "...";
  }

  emit TaskCountChange(tasks.count());
  update();
  updateGeometry();

}

void MultiLoadingIndicator::paintEvent(QPaintEvent*) {

  QPainter p(this);

  const QRect text_rect(
      kHorizontalPadding + spinner_->sizeHint().width() + kSpacing, kVerticalPadding,
      width() - kHorizontalPadding * 2 - spinner_->sizeHint().width() - kSpacing,
      height() - kVerticalPadding * 2);
  p.drawText(text_rect, Qt::TextSingleLine | Qt::AlignLeft, fontMetrics().elidedText(text_, Qt::ElideRight, text_rect.width()));

}

