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

#ifndef SMARTPLAYLISTSEARCHTERMWIDGETOVERLAY_H
#define SMARTPLAYLISTSEARCHTERMWIDGETOVERLAY_H

#include <QWidget>
#include <QString>
#include <QPixmap>

class QPaintEvent;
class QMouseEvent;
class QKeyEvent;

class SmartPlaylistSearchTermWidget;

class SmartPlaylistSearchTermWidgetOverlay : public QWidget {
  Q_OBJECT

 public:
  explicit SmartPlaylistSearchTermWidgetOverlay(SmartPlaylistSearchTermWidget *parent);

  static const int kSpacing;
  static const int kIconSize;

  void Grab();
  void SetOpacity(const float opacity);
  float opacity() const;

 protected:
  void paintEvent(QPaintEvent *e) override;
  void mouseReleaseEvent(QMouseEvent *e) override;
  void keyReleaseEvent(QKeyEvent *e) override;

 private:
  SmartPlaylistSearchTermWidget *parent_;

  float opacity_;
  QString text_;
  QPixmap pixmap_;
  QPixmap icon_;
};

#endif  // SMARTPLAYLISTSEARCHTERMWIDGETOVERLAY_H
