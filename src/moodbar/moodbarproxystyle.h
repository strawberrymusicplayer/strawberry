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
#include <QPoint>
#include <QStyle>

#include "constants/moodbarsettings.h"
#include "moodbarrenderer.h"

class QAction;
class QActionGroup;
class QMenu;
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
  void MoodbarShow(const bool show);
  void StyleChanged();

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
  void ShowContextMenu(const QPoint pos);

  static QPixmap MoodbarPixmap(const ColorVector &colors, const QSize size, const QPalette &palette, const QStyleOptionSlider *opt);

 private Q_SLOTS:
  void FaderValueChanged(const qreal value);
  void SetStyle(QAction *action);

 Q_SIGNALS:
  void SettingsChanged();

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

  QMenu *context_menu_;
  QAction *show_moodbar_action_;
  QActionGroup *style_action_group_;
};

#endif  // MOODBARPROXYSTYLE_H
