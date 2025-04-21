/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QWidget>
#include <QList>
#include <QString>
#include <QStringList>
#include <QPainter>
#include <QSize>
#include <QRect>
#include <QSizePolicy>
#include <QPaintEvent>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"
#include "multiloadingindicator.h"
#include "widgets/busyindicator.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kVerticalPadding = 4;
constexpr int kHorizontalPadding = 6;
constexpr int kSpacing = 6;
}

MultiLoadingIndicator::MultiLoadingIndicator(QWidget *parent)
    : QWidget(parent),
      task_manager_(nullptr),
      spinner_(new BusyIndicator(this)),
      task_count_(-1) {

  spinner_->move(kHorizontalPadding, kVerticalPadding);
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

}

QSize MultiLoadingIndicator::sizeHint() const {

  const int width = kHorizontalPadding * 2 + spinner_->sizeHint().width() + kSpacing + fontMetrics().horizontalAdvance(text_);
  const int height = kVerticalPadding * 2 + qMax(spinner_->sizeHint().height(), fontMetrics().height());

  return QSize(width, height);

}

void MultiLoadingIndicator::SetTaskManager(SharedPtr<TaskManager> task_manager) {

  task_manager_ = task_manager;
  QObject::connect(&*task_manager_, &TaskManager::TasksChanged, this, &MultiLoadingIndicator::UpdateText);

}

void MultiLoadingIndicator::UpdateText() {

  const QList<TaskManager::Task> tasks = task_manager_->GetTasks();

  QStringList strings;
  strings.reserve(tasks.count());
  for (const TaskManager::Task &task : tasks) {
    QString task_text = task.name;
    task_text[0] = task_text[0].toLower();

    if (task.progress_max > 0) {
      int percentage = static_cast<int>(static_cast<float>(task.progress) / static_cast<float>(task.progress_max) * 100.0F);
      task_text += QStringLiteral(" %1%").arg(percentage);
    }

    strings << task_text;
  }

  text_ = strings.join(", "_L1);
  if (!text_.isEmpty()) {
    text_[0] = text_[0].toUpper();
    text_ += "..."_L1;
  }

  if (task_count_ != tasks.count()) {
    task_count_ = tasks.count();
    Q_EMIT TaskCountChange(static_cast<int>(tasks.count()));
  }

  update();
  updateGeometry();

}

void MultiLoadingIndicator::paintEvent(QPaintEvent *e) {

  Q_UNUSED(e)

  QPainter p(this);

  const QRect text_rect(
      kHorizontalPadding + spinner_->sizeHint().width() + kSpacing, kVerticalPadding,
      width() - kHorizontalPadding * 2 - spinner_->sizeHint().width() - kSpacing,
      height() - kVerticalPadding * 2);
  p.drawText(text_rect, Qt::TextSingleLine | Qt::AlignLeft, fontMetrics().elidedText(text_, Qt::ElideRight, text_rect.width()));  // NOLINT(bugprone-suspicious-enum-usage)

}
