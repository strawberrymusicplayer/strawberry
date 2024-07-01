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

#ifndef VISUALIZATIONCONTAINER_H
#define VISUALIZATIONCONTAINER_H

#include "config.h"

#include <QBasicTimer>
#include <QGraphicsView>

#include "core/song.h"

class GstEngine;
class ProjectMVisualization;
class VisualizationOverlay;
class VisualizationSelector;

class QMenu;
class QActionGroup;
class QEvent;
class QShowEvent;
class QHideEvent;
class QCloseEvent;
class QResizeEvent;
class QTimerEvent;
class QMouseEvent;
class QContextMenuEvent;
class QKeyEvent;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
class QEnterEvent;
#endif

class VisualizationContainer : public QGraphicsView {
  Q_OBJECT

 public:
  explicit VisualizationContainer(QWidget *parent = nullptr);

  static const char *kSettingsGroup;

  void SetEngine(GstEngine *engine);
  void SetActions(QAction *previous, QAction *play_pause, QAction *stop, QAction *next);

 public slots:
  void SongMetadataChanged(const Song &metadata);
  void Stopped();

 protected:
  // QWidget
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void closeEvent(QCloseEvent *e) override;
  void resizeEvent(QResizeEvent *e) override;
  void timerEvent(QTimerEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  void enterEvent(QEnterEvent *e) override;
#else
  void enterEvent(QEvent *e) override;
#endif
  void leaveEvent(QEvent *e) override;
  void mouseDoubleClickEvent(QMouseEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;

 private:
  void SizeChanged();
  void AddFramerateMenuItem(const QString &name, int value, int def, QActionGroup *group);
  void AddQualityMenuItem(const QString &name, int value, int def, QActionGroup *group);

 private slots:
  void ChangeOverlayOpacity(qreal value);
  void ShowPopupMenu(const QPoint &pos);
  void ToggleFullscreen();
  void SetFps(const int fps);
  void SetQuality(const int size);

 private:
  ProjectMVisualization *projectm_visualization_;
  VisualizationOverlay *overlay_;
  VisualizationSelector *selector_;
  QGraphicsProxyWidget *overlay_proxy_;
  GstEngine *engine_;
  QMenu *menu_;
  QBasicTimer update_timer_;
  int fps_;
  int size_;
};

#endif  // VISUALIZATIONCONTAINER_H
