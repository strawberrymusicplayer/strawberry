/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
#  include "constants/seekbarsettings.h"
#endif

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QEvent;
class QContextMenuEvent;
class QPoint;

#ifdef HAVE_MOODBAR
class MoodbarProxyStyle;
#endif
#ifdef HAVE_WAVEFORM
class WaveformProxyStyle;
#endif
class Ui_TrackSlider;

class TrackSlider : public QWidget {
  Q_OBJECT

 public:
  explicit TrackSlider(QWidget *parent = nullptr);
  ~TrackSlider() override;

  void Init();

  using SeekbarMode = SeekbarSettings::Mode;

  // QWidget
  QSize sizeHint() const override;

  // QObject
  bool event(QEvent *e) override;
  bool eventFilter(QObject *object, QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *e) override;

#ifdef HAVE_MOODBAR
  MoodbarProxyStyle *moodbar_proxy_style() const { return moodbar_proxy_style_; }
#endif

#ifdef HAVE_WAVEFORM
  WaveformProxyStyle *waveform_proxy_style() const { return waveform_proxy_style_; }
#endif

  void SetSeekbarMode(const SeekbarMode seekbar_mode);
  SeekbarMode seekbar_mode() const { return seekbar_mode_; }

  // Re-resolves the active seekbar mode from the proxy styles' (already refreshed) show state.
  // Call after the moodbar/waveform proxy styles reload their settings so a change made from either Preferences page — not just the context menu — keeps the two visualizations mutually exclusive and the active QStyle / seekbar_mode_ consistent.
  void ReloadSettings();

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
  static SeekbarMode LoadSeekbarMode();
  void ShowSeekbarContextMenu(const QPoint pos);

 private:
  Ui_TrackSlider *ui_;

#ifdef HAVE_MOODBAR
  MoodbarProxyStyle *moodbar_proxy_style_;
#endif

#ifdef HAVE_WAVEFORM
  WaveformProxyStyle *waveform_proxy_style_;
#endif

#if defined(HAVE_MOODBAR) || defined(HAVE_WAVEFORM)
  SeekbarMode seekbar_mode_;

  // Unified seekbar right-click menu, built in the constructor.
  QMenu *seekbar_menu_;
  QActionGroup *seekbar_mode_group_;
  QAction *normal_action_;
#endif
#ifdef HAVE_MOODBAR
  QAction *moodbar_action_;
  QMenu *moodbar_style_menu_;
  QActionGroup *moodbar_style_group_;
#endif
#ifdef HAVE_WAVEFORM
  QAction *waveform_action_;
#endif

  bool setting_value_;
  bool show_remaining_time_;
  int slider_maximum_value_;  // We cache it to avoid unnecessary updates
};

#endif  // TRACKSLIDER_H
