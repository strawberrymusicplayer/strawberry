/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
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

#ifndef FAVORITEWIDGET_H
#define FAVORITEWIDGET_H

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QRect>
#include <QSize>

class QMouseEvent;
class QPaintEvent;

class FavoriteWidget : public QWidget {
  Q_OBJECT

 public:
  explicit FavoriteWidget(const int tab_index, const bool favorite = false, QWidget *parent = nullptr);

  // Change the value if different from the current one and then update display and emit FavoriteStateChanged signal
  bool IsFavorite() const { return favorite_; }
  void SetFavorite(const bool favorite);

  QSize sizeHint() const override;

 Q_SIGNALS:
  void FavoriteStateChanged(const int tab_index, const bool favorite);

 protected:
  void paintEvent(QPaintEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;

 private:
  // The playlist's id this widget belongs to
  int tab_index_;
  bool favorite_;
  QIcon on_;
  QIcon off_;
  QRect rect_;
};

#endif  // FAVORITEWIDGET_H
