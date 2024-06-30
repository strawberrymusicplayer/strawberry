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

#include "config.h"

#include <QPainter>

#include "core/logging.h"
#include "visualizationopenglwidget.h"
#include "projectmvisualization.h"

VisualizationOpenGLWidget::VisualizationOpenGLWidget(ProjectMVisualization *projectm_visualization, QWidget *parent, Qt::WindowFlags f)
  : QOpenGLWidget(parent, f),
    projectm_visualization_(projectm_visualization) {}

void VisualizationOpenGLWidget::initializeGL() {

  projectm_visualization_->Init();

  QOpenGLWidget::initializeGL();
  QOpenGLFunctions::initializeOpenGLFunctions();

}

void VisualizationOpenGLWidget::paintGL() {

  QPainter p(this);
  p.beginNativePainting();
  projectm_visualization_->RenderFrame(width(), height());
  p.endNativePainting();
  update();

  qLog(Debug) << __PRETTY_FUNCTION__ << glGetError();

}

void VisualizationOpenGLWidget::resizeGL(const int width, const int height) {

  Setup(width, height);
  projectm_visualization_->Resize(width, height);

}

void VisualizationOpenGLWidget::Setup(const int width, const int height) {

  glShadeModel(GL_SMOOTH);
  glClearColor(0, 0, 0, 0);
  glViewport(0, 0, width, height);
  glMatrixMode(GL_TEXTURE);
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDrawBuffer(GL_BACK);
  glReadBuffer(GL_BACK);
  glEnable(GL_BLEND);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_LINE_SMOOTH);
  glEnable(GL_POINT_SMOOTH);
  glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
  glLineStipple(2, 0xAAAA);

}
