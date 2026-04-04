/*
 * Strawberry Music Player
 * Copyright 2026, guitaripod <guitaripod@users.noreply.github.com>
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

#include "syncedlyricswidget.h"

#include <QColor>
#include <QFont>
#include <QListWidgetItem>
#include <QPalette>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
constexpr int kUserScrollResumeMs = 3000;
constexpr int kScrollAnimationDurationMs = 400;
constexpr qreal kCurrentFontSizeMultiplier = 1.4;
}  // namespace

SyncedLyricsWidget::SyncedLyricsWidget(QWidget *parent)
    : QListWidget(parent),
      current_line_index_(-1),
      is_user_scrolling_(false),
      user_scroll_timer_(new QTimer(this)),
      scroll_animation_(new QPropertyAnimation(this)) {

  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  setSelectionMode(QAbstractItemView::NoSelection);
  setFocusPolicy(Qt::NoFocus);
  setWordWrap(true);
  setSpacing(8);

  scroll_animation_->setTargetObject(verticalScrollBar());
  scroll_animation_->setPropertyName("value");
  scroll_animation_->setDuration(kScrollAnimationDurationMs);
  scroll_animation_->setEasingCurve(QEasingCurve::OutCubic);

  user_scroll_timer_->setSingleShot(true);
  user_scroll_timer_->setInterval(kUserScrollResumeMs);

  QObject::connect(user_scroll_timer_, &QTimer::timeout, this, &SyncedLyricsWidget::UserScrollTimeout);
  QObject::connect(this, &QListWidget::itemClicked, this, &SyncedLyricsWidget::ItemClicked);

}

void SyncedLyricsWidget::SetFonts(const QFont &base_font) {

  font_inactive_ = base_font;
  font_inactive_.setWeight(QFont::Medium);

  font_current_ = base_font;
  font_current_.setPointSizeF(base_font.pointSizeF() * kCurrentFontSizeMultiplier);
  font_current_.setWeight(QFont::Bold);

}

void SyncedLyricsWidget::SetLyrics(const SyncedLyrics &lyrics) {

  clear();
  lyrics_ = lyrics;
  current_line_index_ = -1;
  is_user_scrolling_ = false;
  scroll_animation_->stop();

  const int spacer_height = qMax(200, height() / 2);

  QListWidgetItem *top_spacer = new QListWidgetItem();
  top_spacer->setFlags(Qt::NoItemFlags);
  top_spacer->setSizeHint(QSize(0, spacer_height));
  addItem(top_spacer);

  for (int i = 0; i < lyrics_.size(); ++i) {
    QListWidgetItem *item = new QListWidgetItem(lyrics_[i].text);
    item->setData(Qt::UserRole, lyrics_[i].time_msec);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
    StyleItem(item, false, count());
    addItem(item);
  }

  QListWidgetItem *bottom_spacer = new QListWidgetItem();
  bottom_spacer->setFlags(Qt::NoItemFlags);
  bottom_spacer->setSizeHint(QSize(0, spacer_height));
  addItem(bottom_spacer);

  scrollToTop();

}

void SyncedLyricsWidget::SetPositionMsec(const qint64 position_msec) {

  if (lyrics_.isEmpty()) return;

  int new_index = -1;
  for (int i = 0; i < lyrics_.size(); ++i) {
    if (lyrics_[i].time_msec <= position_msec) {
      new_index = i;
    }
    else {
      break;
    }
  }

  if (new_index != current_line_index_) {
    UpdateCurrentLine(new_index);
  }

}

void SyncedLyricsWidget::Clear() {

  clear();
  lyrics_.clear();
  current_line_index_ = -1;
  is_user_scrolling_ = false;
  scroll_animation_->stop();

}

void SyncedLyricsWidget::UpdateCurrentLine(const int new_index) {

  current_line_index_ = new_index;

  const int item_offset = 1;

  for (int i = 0; i < lyrics_.size(); ++i) {
    const int widget_index = i + item_offset;
    if (i == current_line_index_) {
      StyleItem(item(widget_index), true, 0);
    }
    else {
      const int distance = current_line_index_ >= 0 ? qAbs(i - current_line_index_) : lyrics_.size();
      StyleItem(item(widget_index), false, distance);
    }
  }

  if (current_line_index_ >= 0 && current_line_index_ < lyrics_.size()) {
    if (!is_user_scrolling_) {
      SmoothScrollToItem(current_line_index_ + item_offset);
    }
  }

}

void SyncedLyricsWidget::SmoothScrollToItem(const int index) {

  QListWidgetItem *target = item(index);
  if (!target) return;

  const QRect item_rect = visualItemRect(target);
  const int viewport_center = viewport()->height() / 2;
  const int item_center = item_rect.top() + item_rect.height() / 2;
  const int target_value = verticalScrollBar()->value() + item_center - viewport_center;
  const int clamped = qBound(verticalScrollBar()->minimum(), target_value, verticalScrollBar()->maximum());

  if (scroll_animation_->state() == QAbstractAnimation::Running) {
    scroll_animation_->stop();
  }

  scroll_animation_->setStartValue(verticalScrollBar()->value());
  scroll_animation_->setEndValue(clamped);
  scroll_animation_->start();

}

void SyncedLyricsWidget::StyleItem(QListWidgetItem *item, bool is_current, int distance) {

  if (is_current) {
    item->setFont(font_current_);
    item->setForeground(palette().color(QPalette::Text));
  }
  else {
    item->setFont(font_inactive_);
    QColor color = palette().color(QPalette::Text);
    const int alpha = qMax(60, 160 - distance * 25);
    color.setAlpha(alpha);
    item->setForeground(color);
  }

}

void SyncedLyricsWidget::ItemClicked(QListWidgetItem *item) {

  const qint64 time_msec = item->data(Qt::UserRole).toLongLong();
  Q_EMIT SeekRequested(time_msec);

}

void SyncedLyricsWidget::wheelEvent(QWheelEvent *e) {

  is_user_scrolling_ = true;
  user_scroll_timer_->start();
  scroll_animation_->stop();
  QListWidget::wheelEvent(e);

}

void SyncedLyricsWidget::UserScrollTimeout() {
  is_user_scrolling_ = false;
}
