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

#include "config.h"

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QLabel>
#include <QFontMetrics>

#include "widgets/stickyslider.h"
#include "equalizerslider.h"
#include "ui_equalizerslider.h"

EqualizerSlider::EqualizerSlider(const QString &label, QWidget *parent)
    : QWidget(parent),
      ui_(new Ui_EqualizerSlider) {

  ui_->setupUi(this);
  ui_->band->setText(label);

  QFontMetrics fm = ui_->gain->fontMetrics();
  ui_->gain->setMinimumWidth(fm.horizontalAdvance(tr("%1 dB").arg(-99.99)));
  ui_->gain->setText(tr("%1 dB").arg(0));  // Gain [dB]

  ui_->slider->setValue(0);

  QObject::connect(ui_->slider, &QSlider::valueChanged, this, &EqualizerSlider::OnValueChanged);

}

EqualizerSlider::~EqualizerSlider() {
  delete ui_;
}

void EqualizerSlider::OnValueChanged(const int value) {

  // Converting % to dB as per GstEnginePipeline::UpdateEqualizer():
  float gain = (static_cast<int>(value) < 0) ? static_cast<float>(value) * static_cast<float>(0.24) : static_cast<float>(value) * static_cast<float>(0.12);

  ui_->gain->setText(tr("%1 dB").arg(gain));  // Gain [dB]
  Q_EMIT ValueChanged(value);

}

int EqualizerSlider::value() const {
  return ui_->slider->value();
}

void EqualizerSlider::set_value(const int value) {
  ui_->slider->setValue(value);
}

