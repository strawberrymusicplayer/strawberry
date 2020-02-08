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
    ui_(new Ui_EqualizerSlider)
{
  ui_->setupUi(this);
  ui_->band->setText(label);

  QFontMetrics fm = ui_->gain->fontMetrics();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 11, 0))
  int longestLabelWidth = fm.horizontalAdvance(tr("%1 dB").arg(-99.99));
#else
  int longestLabelWidth = fm.width(tr("%1 dB").arg(-99.99));
#endif
  ui_->gain->setMinimumWidth(longestLabelWidth);
  ui_->gain->setText(tr("%1 dB").arg(0));  // Gain [dB]

  ui_->slider->setValue(0);

  connect(ui_->slider, SIGNAL(valueChanged(int)), this, SLOT(OnValueChanged(int)));

}

EqualizerSlider::~EqualizerSlider() {
  delete ui_;
}

void EqualizerSlider::OnValueChanged(int value) {

  // Converting % to dB as per GstEnginePipeline::UpdateEqualizer():
  float gain = (value < 0) ? value * 0.24 : value * 0.12;

  ui_->gain->setText(tr("%1 dB").arg(gain));  // Gain [dB]
  emit ValueChanged(value);

}

int EqualizerSlider::value() const {
  return ui_->slider->value();
}

void EqualizerSlider::set_value(int value) {
  ui_->slider->setValue(value);
}

