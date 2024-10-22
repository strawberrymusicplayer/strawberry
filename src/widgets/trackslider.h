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

#ifndef TRACKSLIDER_H
#define TRACKSLIDER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QSize>

class QLabel;
class QEvent;

#ifdef HAVE_MOODBAR
class MoodbarProxyStyle;
#endif
class Ui_TrackSlider;

class TrackSlider : public QWidget {
  Q_OBJECT

 public:
  explicit TrackSlider(QWidget *parent = nullptr);
  ~TrackSlider() override;

  void Init();

  // QWidget
  QSize sizeHint() const override;

  // QObject
  bool event(QEvent*) override;

#ifdef HAVE_MOODBAR
  MoodbarProxyStyle *moodbar_proxy_style() const { return moodbar_proxy_style_; }
#endif

 public Q_SLOTS:
  void SetValue(const int elapsed, const int total);
  void SetStopped();
  void SetCanSeek(const bool can_seek);
  void Seek(const int gap);

 Q_SIGNALS:
  void ValueChanged(const int value);
  void ValueChangedSeconds(const quint64 value);

  void SeekForward();
  void SeekBackward();
  void Next();
  void Previous();

 private Q_SLOTS:
  void ValueMaybeChanged(const int value);
  void ToggleTimeDisplay();

 private:
  void UpdateTimes(const int elapsed);
  void UpdateLabelWidth();
  static void UpdateLabelWidth(QLabel *label, const QString &text);

 private:
  Ui_TrackSlider *ui_;

#ifdef HAVE_MOODBAR
  MoodbarProxyStyle *moodbar_proxy_style_;
#endif

  bool setting_value_;
  bool show_remaining_time_;
  int slider_maximum_value_;  // We cache it to avoid unnecessary updates
};

#endif  // TRACKSLIDER_H
