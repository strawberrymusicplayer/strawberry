/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <algorithm>
#include <cmath>

#include <QtGlobal>
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QScrollBar>
#include <QMouseEvent>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QPalette>
#include <QColor>

#include "lyricswidget.h"

using namespace Qt::Literals::StringLiterals;

LyricLineLabel::LyricLineLabel(const int index, const qint64 timestamp_ms, const QString &text, QWidget *parent)
    : QLabel(text, parent),
      index_(index),
      timestamp_ms_(timestamp_ms) {

  setWordWrap(true);
  setCursor(Qt::PointingHandCursor);
  SetActive(false);

}

void LyricLineLabel::SetActive(const bool active) {

  const QColor c = palette().color(QPalette::Text);
  if (active) {
    setStyleSheet(QStringLiteral("QLabel { color: rgba(%1, %2, %3, 1.0); font-weight: bold; font-size: 115%; background: transparent; border: none; padding: 4px 6px; }")
                      .arg(c.red()).arg(c.green()).arg(c.blue()));
  }
  else {
    setStyleSheet(QStringLiteral("QLabel { color: rgba(%1, %2, %3, 0.4); font-weight: normal; font-size: 95%; background: transparent; border: none; padding: 4px 6px; }")
                      .arg(c.red()).arg(c.green()).arg(c.blue()));
  }

}

void LyricLineLabel::mouseReleaseEvent(QMouseEvent *e) {

  if (e->button() == Qt::LeftButton) {
    Q_EMIT Clicked(timestamp_ms_);
  }
  QLabel::mouseReleaseEvent(e);

}

LyricsWidget::LyricsWidget(QWidget *parent)
    : QWidget(parent),
      scroll_area_(new QScrollArea(this)),
      content_widget_(new QWidget(scroll_area_)),
      content_layout_(new QVBoxLayout(content_widget_)),
      button_sync_(new QPushButton(tr("↓ Sync"), this)),
      active_index_(-1),
      user_scrolled_away_(false),
      is_programmatic_scroll_(false),
      scroll_animation_(new QVariantAnimation(this)) {

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(scroll_area_);

  scroll_area_->setWidgetResizable(true);
  scroll_area_->setWidget(content_widget_);
  scroll_area_->setFrameShape(QFrame::NoFrame);
  scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll_area_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  content_layout_->setContentsMargins(10, 10, 10, 10);
  content_layout_->setSpacing(4);

  setMinimumHeight(280);

  button_sync_->setCursor(Qt::PointingHandCursor);
  button_sync_->setStyleSheet(u"QPushButton { background-color: rgba(60, 60, 60, 0.85); color: #ffffff; border: none; border-radius: 12px; padding: 6px 14px; font-weight: bold; font-size: 90%; } QPushButton:hover { background-color: rgba(80, 80, 80, 0.95); }"_s);
  button_sync_->hide();

  QObject::connect(scroll_area_->verticalScrollBar(), &QScrollBar::valueChanged, this, &LyricsWidget::ScrollBarValueChanged);
  QObject::connect(button_sync_, &QPushButton::clicked, this, &LyricsWidget::SyncButtonClicked);
  QObject::connect(scroll_animation_, &QVariantAnimation::valueChanged, this, &LyricsWidget::AnimationValueChanged);
  QObject::connect(scroll_animation_, &QVariantAnimation::finished, this, &LyricsWidget::AnimationFinished);

}

void LyricsWidget::Clear() {

  scroll_animation_->stop();
  lines_.clear();
  label_items_.clear();
  active_index_ = -1;
  user_scrolled_away_ = false;
  button_sync_->hide();

  QLayoutItem *child;
  while ((child = content_layout_->takeAt(0)) != nullptr) {
    delete child->widget();
    delete child;
  }

}

void LyricsWidget::SetPlainLyrics(const QString &text) {

  Clear();

  LyricLineLabel *label = new LyricLineLabel(0, 0, text, content_widget_);
  label->setCursor(Qt::ArrowCursor);
  label->SetActive(true);
  content_layout_->addWidget(label);
  content_layout_->addStretch();

}

void LyricsWidget::SetSyncedLyrics(const QList<ContextView::LrcLine> &lines) {

  Clear();

  lines_ = lines;
  for (int i = 0; i < lines_.size(); ++i) {
    LyricLineLabel *label = new LyricLineLabel(i, lines_[i].timestamp_ms, lines_[i].text.isEmpty() ? u"♪"_s : lines_[i].text, content_widget_);
    content_layout_->addWidget(label);
    label_items_.append(label);

    QObject::connect(label, &LyricLineLabel::Clicked, this, [this](const qint64 ts) {
      Q_EMIT SeekRequested(ts);
    });
  }
  content_layout_->addStretch();

}

void LyricsWidget::SetActiveIndex(const int index) {

  if (index == active_index_ || index < 0 || index >= label_items_.size()) return;

  if (active_index_ >= 0 && active_index_ < label_items_.size()) {
    label_items_[active_index_]->SetActive(false);
  }

  active_index_ = index;
  label_items_[active_index_]->SetActive(true);

  ScrollToActiveLine(false);

}

void LyricsWidget::ScrollToActiveLine(const bool force) {

  if (active_index_ < 0 || active_index_ >= label_items_.size()) return;
  if (user_scrolled_away_ && !force) return;

  LyricLineLabel *label = label_items_[active_index_];
  const int label_center = label->mapTo(content_widget_, label->rect().center()).y();
  const int viewport_half = scroll_area_->viewport()->height() / 2;
  int target_y = label_center - viewport_half;
  const int max_y = scroll_area_->verticalScrollBar()->maximum();
  target_y = std::clamp(target_y, 0, max_y);

  const int current_y = scroll_area_->verticalScrollBar()->value();
  if (std::abs(target_y - current_y) < 2) return;

  scroll_animation_->stop();
  scroll_animation_->setDuration(450);
  scroll_animation_->setEasingCurve(QEasingCurve::OutBack);
  scroll_animation_->setStartValue(current_y);
  scroll_animation_->setEndValue(target_y);

  is_programmatic_scroll_ = true;
  scroll_animation_->start();

}

void LyricsWidget::AnimationValueChanged(const QVariant &value) {

  scroll_area_->verticalScrollBar()->setValue(value.toInt());

}

void LyricsWidget::AnimationFinished() {

  is_programmatic_scroll_ = false;

}

void LyricsWidget::ScrollBarValueChanged(const int value) {

  if (is_programmatic_scroll_ || lines_.isEmpty() || active_index_ < 0 || active_index_ >= label_items_.size()) {
    return;
  }

  LyricLineLabel *label = label_items_[active_index_];
  const int label_center = label->mapTo(content_widget_, label->rect().center()).y();
  const int viewport_half = scroll_area_->viewport()->height() / 2;
  const int expected_y = std::clamp(label_center - viewport_half, 0, scroll_area_->verticalScrollBar()->maximum());

  if (std::abs(value - expected_y) > 40) {
    if (!user_scrolled_away_) {
      user_scrolled_away_ = true;
      button_sync_->show();
      button_sync_->raise();
    }
  }
  else if (user_scrolled_away_) {
    user_scrolled_away_ = false;
    button_sync_->hide();
  }

}

void LyricsWidget::SyncButtonClicked() {

  user_scrolled_away_ = false;
  button_sync_->hide();
  ScrollToActiveLine(true);

}

void LyricsWidget::resizeEvent(QResizeEvent *e) {

  QWidget::resizeEvent(e);

  button_sync_->adjustSize();
  const int x = width() - button_sync_->width() - 20;
  const int y = height() - button_sync_->height() - 20;
  button_sync_->move(x, y);

}
