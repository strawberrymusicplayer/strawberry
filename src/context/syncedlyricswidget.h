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

#ifndef SYNCEDLYRICSWIDGET_H
#define SYNCEDLYRICSWIDGET_H

#include <QListWidget>
#include <QFont>
#include <QTimer>
#include <QPropertyAnimation>

#include "lyrics/lyricline.h"

class SyncedLyricsWidget : public QListWidget {
  Q_OBJECT

 public:
  explicit SyncedLyricsWidget(QWidget *parent = nullptr);

  void SetLyrics(const SyncedLyrics &lyrics);
  void SetPositionMsec(const qint64 position_msec);
  void Clear();
  void SetFonts(const QFont &base_font);

 Q_SIGNALS:
  void SeekRequested(const qint64 msec);

 protected:
  void wheelEvent(QWheelEvent *e) override;
 private Q_SLOTS:
  void ItemClicked(QListWidgetItem *item);
  void UserScrollTimeout();

 private:
  void UpdateCurrentLine(const int new_index);
  void StyleItem(QListWidgetItem *item, bool is_current, int distance);
  void SmoothScrollToItem(const int index);

  SyncedLyrics lyrics_;
  int current_line_index_;
  bool is_user_scrolling_;
  QTimer *user_scroll_timer_;
  QPropertyAnimation *scroll_animation_;
  QFont font_current_;
  QFont font_inactive_;
};

#endif  // SYNCEDLYRICSWIDGET_H
