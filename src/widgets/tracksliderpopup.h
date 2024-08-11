/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2011, David Sansome <me@davidsansome.com>
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

#ifndef TRACKSLIDERPOPUP_H
#define TRACKSLIDERPOPUP_H

#include <QtGlobal>
#include <QObject>
#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QFont>
#include <QFontMetrics>
#include <QPoint>

class QMouseEvent;
class QPaintEvent;

class TrackSliderPopup : public QWidget {
  Q_OBJECT

 public:
  explicit TrackSliderPopup(QWidget *parent);

 public Q_SLOTS:
  void SetText(const QString &text);
  void SetSmallText(const QString &small_text);
  void SetPopupPosition(const QPoint pos);

 protected:
  void paintEvent(QPaintEvent*) override;

 private:
  void UpdatePixmap();
  void UpdatePosition();
  void SendMouseEventToParent(QMouseEvent *e);

 private:
  QString text_;
  QString small_text_;
  QPoint pos_;

  QFont font_;
  QFont small_font_;
  QFontMetrics font_metrics_;
  QFontMetrics small_font_metrics_;
  QPixmap pixmap_;
  QPixmap background_cache_;
};

#endif  // TRACKSLIDERPOPUP_H
