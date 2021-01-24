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

#ifndef WIDGETFADEHELPER_H
#define WIDGETFADEHELPER_H

#include <QWidget>
#include <QPixmap>

class QTimeLine;
class QPaintEvent;
class QEvent;

class WidgetFadeHelper : public QWidget {
  Q_OBJECT

 public:
  WidgetFadeHelper(QWidget *parent, const int msec = 500);

 public slots:
  void StartBlur();
  void StartFade();

 protected:
  void paintEvent(QPaintEvent*) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

 private slots:
  void FadeFinished();

 private:
  void CaptureParent();

 private:
  static const int kLoadingPadding;
  static const int kLoadingBorderRadius;

  QWidget* parent_;
  QTimeLine* blur_timeline_;
  QTimeLine* fade_timeline_;

  QPixmap original_pixmap_;
  QPixmap blurred_pixmap_;
};

#endif  // WIDGETFADEHELPER_H
