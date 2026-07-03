/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MOODBARPROXYSTYLE_H
#define MOODBARPROXYSTYLE_H

#include <QtGlobal>
#include <QObject>
#include <QProxyStyle>
#include <QByteArray>
#include <QString>
#include <QPixmap>
#include <QPalette>
#include <QRect>
#include <QStyle>

#include "constants/moodbarsettings.h"
#include "moodbarrenderer.h"

class QPainter;
class QSlider;
class QStyleOptionComplex;
class QStyleOptionSlider;
class QTimeLine;
class QWidget;
class QEvent;

class MoodbarProxyStyle : public QProxyStyle {
  Q_OBJECT

 public:
  explicit MoodbarProxyStyle(QSlider *slider, QObject *parent = nullptr);

  void ReloadSettings();

  MoodbarSettings::Style moodbar_style() const { return moodbar_style_; }

  // Whether the moodbar would paint (enabled and shown).
  // Lets TrackSlider pick the active seekbar renderer and reflect the current mode.
  bool is_showing() const { return show_; }

  // Sets the active moodbar color scheme and persists the choice to QSettings.
  // Called by TrackSlider's seekbar context menu.
  void SetStyle(const MoodbarSettings::Style style);

  // QProxyStyle
  void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const override;
  QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt, SubControl sc, const QWidget *widget) const override;

  // QObject
  bool eventFilter(QObject *object, QEvent *event) override;

 public Q_SLOTS:
  // An empty byte array means there's no moodbar, so just show a normal slider.
  void SetMoodbarData(const QByteArray &data);

  // If the moodbar is disabled then a normal slider will always be shown.
  void SetShowMoodbar(const bool show);

 Q_SIGNALS:
  void StyleChanged();

  // Emitted by SetShowMoodbar when the show state changes, so the controller can start or stop generating the current song's moodbar for the seekbar.
  void MoodbarShow(const bool show);

 private:
  enum class State {
    MoodbarOn,
    MoodbarOff,
    FadingToOn,
    FadingToOff
  };

 private:
  void NextState();

  void Render(const ComplexControl control, const QStyleOptionSlider *option, QPainter *painter, const QWidget *widget);
  void EnsureMoodbarRendered(const QStyleOptionSlider *opt);
  void DrawArrow(const QStyleOptionSlider *option, QPainter *painter) const;

  static QPixmap MoodbarPixmap(const ColorVector &colors, const QSize size, const QPalette &palette, const QStyleOptionSlider *opt);

 private Q_SLOTS:
  void FaderValueChanged(const qreal value);

 private:
  QSlider *slider_;

  bool show_;
  QByteArray data_;
  MoodbarSettings::Style moodbar_style_;

  State state_;
  QTimeLine *fade_timeline_;

  QPixmap fade_source_;
  QPixmap fade_target_;

  bool moodbar_colors_dirty_;
  bool moodbar_pixmap_dirty_;
  ColorVector moodbar_colors_;
  QPixmap moodbar_pixmap_;
};

#endif  // MOODBARPROXYSTYLE_H
