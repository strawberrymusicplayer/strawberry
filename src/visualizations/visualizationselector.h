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

#ifndef VISUALIZATIONSELECTOR_H
#define VISUALIZATIONSELECTOR_H

#include "config.h"

#include <QDialog>

class QPushButton;
class QShowEvent;
class QHideEvent;

class ProjectMVisualization;
class Ui_VisualizationSelector;

class VisualizationSelector : public QDialog {
  Q_OBJECT

 public:
  explicit VisualizationSelector(QWidget *parent = nullptr);
  ~VisualizationSelector();

  void SetVisualization(ProjectMVisualization *projectm_visualization) { projectm_visualization_ = projectm_visualization; }

 protected:
  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

 private slots:
  void ModeChanged(const int mode);
  void SelectAll();
  void SelectNone();

 private:
  Ui_VisualizationSelector *ui_;
  ProjectMVisualization *projectm_visualization_;
  QPushButton *select_all_;
  QPushButton *select_none_;
};

#endif  // VISUALIZATIONSELECTOR_H
