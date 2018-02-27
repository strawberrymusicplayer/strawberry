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

#include "equalizerslider.h"
#include "ui_equalizerslider.h"

EqualizerSlider::EqualizerSlider(const QString &label, QWidget *parent)
  : QWidget(parent),
    ui_(new Ui_EqualizerSlider)
{
  ui_->setupUi(this);
  ui_->label->setText(label);

  connect(ui_->slider, SIGNAL(valueChanged(int)), SIGNAL(ValueChanged(int)));
}

EqualizerSlider::~EqualizerSlider() {
  delete ui_;
}

int EqualizerSlider::value() const {
  return ui_->slider->value();
}

void EqualizerSlider::set_value(int value) {
  ui_->slider->setValue(value);
}

