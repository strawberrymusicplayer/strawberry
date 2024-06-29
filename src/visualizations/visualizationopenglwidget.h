/*
 * Strawberry Music Player
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

#ifndef VISUALIZATIONOPENGLWIDGET_H
#define VISUALIZATIONOPENGLWIDGET_H

#include "config.h"

#include <QOpenGLWidget>

class ProjectMVisualization;

class VisualizationOpenGLWidget : public QOpenGLWidget {
  Q_OBJECT

 public:
  explicit VisualizationOpenGLWidget(ProjectMVisualization *projectm_visualization, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

 protected:
  void initializeGL() override;
  void paintGL() override;
  void resizeGL(const int width, const int height) override;

 private:
  void Setup(const int width, const int height);

 private:
  ProjectMVisualization *projectm_visualization_;
};

#endif  // VISUALIZATIONOPENGLWIDGET_H
