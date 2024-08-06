/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef VISUALIZATIONOVERLAY_H
#define VISUALIZATIONOVERLAY_H

#include "config.h"

#include <QWidget>
#include <QString>
#include <QBasicTimer>

class Ui_VisualizationOverlay;

class QGraphicsProxyWidget;
class QTimeLine;
class QAction;

class VisualizationOverlay : public QWidget {
  Q_OBJECT

 public:
  explicit VisualizationOverlay(QWidget *parent = nullptr);
  ~VisualizationOverlay();

  QGraphicsProxyWidget *title(QGraphicsProxyWidget *proxy) const;

  void SetActions(QAction *previous, QAction *play_pause, QAction *stop, QAction *next);
  void SetSongTitle(const QString &title);

 public slots:
  void SetVisible(const bool visible);

 signals:
  void OpacityChanged(const qreal value);
  void ShowPopupMenu(const QPoint &pos);

 protected:
  // QWidget
  void timerEvent(QTimerEvent *e);

 private slots:
  void ShowSettingsMenu();

 private:
  Ui_VisualizationOverlay *ui_;

  QTimeLine *fade_timeline_;
  QBasicTimer fade_out_timeout_;
  bool visible_;
};

#endif  // VISUALIZATIONOVERLAY_H
