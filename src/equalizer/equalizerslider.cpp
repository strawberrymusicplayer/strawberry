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

#include <cmath>

#include <QtGlobal>
#include <QWidget>
#include <QString>
#include <QLabel>
#include <QFont>
#include <QFontMetrics>
#include <QPalette>

#include "widgets/stickyslider.h"
#include "equalizerslider.h"
#include "ui_equalizerslider.h"

EqualizerSlider::EqualizerSlider(const QString &label, QWidget *parent, const bool preamp)
    : QWidget(parent),
      ui_(new Ui_EqualizerSlider),
      preamp_(preamp) {

  ui_->setupUi(this);
  ui_->band->setText(label);

  QFontMetrics fm = ui_->gain->fontMetrics();
  ui_->gain->setMinimumWidth(fm.horizontalAdvance(tr("%1 dB").arg(-99.99)));
  ui_->gain->setText(tr("%1 dB").arg(0));  // Gain [dB]

  ui_->slider->setValue(0);
#ifdef Q_OS_ANDROID
  while (QLayoutItem *item = ui_->horizontalLayout->takeAt(0)) delete item;
  ui_->slider->setOrientation(Qt::Horizontal);
  ui_->slider->setMinimumHeight(48);
  ui_->slider->setAccessibleName(label);
  ui_->verticalLayout->removeWidget(ui_->band);
  ui_->verticalLayout->removeWidget(ui_->gain);
  ui_->horizontalLayout->addWidget(ui_->band);
  ui_->horizontalLayout->addWidget(ui_->gain);
  ui_->verticalLayout->addWidget(ui_->slider);
  ui_->band->setWordWrap(true);
  ui_->gain->setWordWrap(true);
  ui_->gain->setMinimumWidth(0);
  ui_->band->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  ui_->gain->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QFont font = ui_->gain->font();
  font.setBold(true);
  ui_->gain->setFont(font);
  ui_->gain->setForegroundRole(QPalette::Link);
  setMinimumHeight(0);
#endif

  QObject::connect(ui_->slider, &QSlider::valueChanged, this, &EqualizerSlider::OnValueChanged);

}

EqualizerSlider::~EqualizerSlider() {
  delete ui_;
}

void EqualizerSlider::OnValueChanged(const int value) {

  // Converting % to dB as per GstEnginePipeline::UpdateEqualizer():
  float gain = (static_cast<int>(value) < 0) ? static_cast<float>(value) * static_cast<float>(0.24) : static_cast<float>(value) * static_cast<float>(0.12);

  if (preamp_) {
    if (value == -100) {
      ui_->gain->setText(tr("Muted"));
    }
    else {
      const double gain_db = 20.0 * std::log10((value + 100) * 0.01);
      ui_->gain->setText(tr("%1 dB").arg(std::round(gain_db * 10.0) / 10.0));
    }
  }
  else {
    ui_->gain->setText(tr("%1 dB").arg(gain));
  }
  Q_EMIT ValueChanged(value);

}

int EqualizerSlider::value() const {
  return ui_->slider->value();
}

void EqualizerSlider::set_value(const int value) {
  ui_->slider->setValue(value);
}
