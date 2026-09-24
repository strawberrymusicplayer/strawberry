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

#ifndef LYRICSWIDGET_H
#define LYRICSWIDGET_H

#include "config.h"

#include <QWidget>
#include <QScrollArea>
#include <QList>
#include <QString>
#include <QLabel>
#include <QVariantAnimation>
#include "contextview.h"

class QVBoxLayout;
class QPushButton;
class QMouseEvent;

class LyricLineLabel : public QLabel {
  Q_OBJECT

 public:
  LyricLineLabel(int index, qint64 timestamp_ms, const QString &text, QWidget *parent = nullptr);

  int index() const { return index_; }
  qint64 timestamp_ms() const { return timestamp_ms_; }
  void SetActive(bool active);

 Q_SIGNALS:
  void Clicked(qint64 timestamp_ms);

 protected:
  void mouseReleaseEvent(QMouseEvent *event) override;

 private:
  int index_;
  qint64 timestamp_ms_;
};

class LyricsWidget : public QWidget {
  Q_OBJECT

 public:
  explicit LyricsWidget(QWidget *parent = nullptr);

  void SetPlainLyrics(const QString &text);
  void SetSyncedLyrics(const QList<ContextView::LrcLine> &lines);
  void SetActiveIndex(int index);
  void Clear();
  bool is_synced() const { return !lines_.isEmpty(); }

 Q_SIGNALS:
  void SeekRequested(qint64 timestamp_ms);

 protected:
  void resizeEvent(QResizeEvent *event) override;

 private Q_SLOTS:
  void ScrollBarValueChanged(int value);
  void SyncButtonClicked();
  void AnimationValueChanged(const QVariant &value);
  void AnimationFinished();

 private:
  void ScrollToActiveLine(bool force = false);

 private:
  QScrollArea *scroll_area_;
  QWidget *content_widget_;
  QVBoxLayout *content_layout_;
  QPushButton *button_sync_;

  QList<ContextView::LrcLine> lines_;
  QList<LyricLineLabel*> label_items_;
  int active_index_;
  bool user_scrolled_away_;
  bool is_programmatic_scroll_;

  QVariantAnimation *scroll_animation_;
};

#endif  // LYRICSWIDGET_H
