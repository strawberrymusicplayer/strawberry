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

#include "config.h"

#include <QPushButton>

#include "visualizationselector.h"
#include "projectmpresetmodel.h"
#include "projectmvisualization.h"
#include "ui_visualizationselector.h"

VisualizationSelector::VisualizationSelector(QWidget *parent)
    : QDialog(parent),
      ui_(new Ui_VisualizationSelector),
      projectm_visualization_(nullptr),
      select_all_(nullptr),
      select_none_(nullptr) {

  ui_->setupUi(this);

  select_all_ = ui_->buttonBox->addButton(tr("Select All"), QDialogButtonBox::ActionRole);
  select_none_ = ui_->buttonBox->addButton(tr("Select None"), QDialogButtonBox::ActionRole);
  QObject::connect(select_all_, &QPushButton::clicked, this, &VisualizationSelector::SelectAll);
  QObject::connect(select_none_, &QPushButton::clicked, this, &VisualizationSelector::SelectNone);
  select_all_->setEnabled(false);
  select_none_->setEnabled(false);

  QObject::connect(ui_->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualizationSelector::ModeChanged);

}

VisualizationSelector::~VisualizationSelector() { delete ui_; }

void VisualizationSelector::showEvent(QShowEvent *e) {

  Q_UNUSED(e);

  if (!ui_->list->model()) {
    ui_->delay->setValue(projectm_visualization_->duration());
    ui_->list->setModel(projectm_visualization_->preset_model());
    QObject::connect(ui_->list->selectionModel(), &QItemSelectionModel::currentChanged, projectm_visualization_->preset_model(), &ProjectMPresetModel::SetImmediatePreset);
    QObject::connect(ui_->delay, QOverload<int>::of(&QSpinBox::valueChanged), projectm_visualization_, &ProjectMVisualization::SetDuration);

    ui_->mode->setCurrentIndex(static_cast<int>(projectm_visualization_->mode()));
  }

  projectm_visualization_->Lock(true);

}

void VisualizationSelector::hideEvent(QHideEvent *e) {
  Q_UNUSED(e);
  projectm_visualization_->Lock(false);
}

void VisualizationSelector::ModeChanged(const int mode) {

  const bool enabled = mode == 1;
  ui_->list->setEnabled(enabled);
  select_all_->setEnabled(enabled);
  select_none_->setEnabled(enabled);

  projectm_visualization_->SetMode(static_cast<ProjectMVisualization::Mode>(mode));

}

void VisualizationSelector::SelectAll() { projectm_visualization_->preset_model()->SelectAll(); }

void VisualizationSelector::SelectNone() { projectm_visualization_->preset_model()->SelectNone(); }
