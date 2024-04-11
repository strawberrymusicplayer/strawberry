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

#include <QWidget>
#include <QVariant>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsmp3.h"
#include "ui_transcoderoptionsmp3.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/lamemp3enc";
}

TranscoderOptionsMP3::TranscoderOptionsMP3(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsMP3) {

  ui_->setupUi(this);

  QObject::connect(ui_->quality_slider, &QSlider::valueChanged, this, &TranscoderOptionsMP3::QualitySliderChanged);
  QObject::connect(ui_->quality_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &TranscoderOptionsMP3::QualitySpinboxChanged);

}

TranscoderOptionsMP3::~TranscoderOptionsMP3() {
  delete ui_;
}

void TranscoderOptionsMP3::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  if (s.value("target", 1).toInt() == 0) {
    ui_->target_quality->setChecked(true);
  }
  else {
    ui_->target_bitrate->setChecked(true);
  }

  ui_->quality_spinbox->setValue(s.value("quality", 10.0F).toFloat());
  ui_->bitrate_slider->setValue(s.value("bitrate", 320).toInt());
  ui_->cbr->setChecked(s.value("cbr", false).toBool());
  ui_->encoding_engine_quality->setCurrentIndex(s.value("encoding-engine-quality", 2).toInt());
  ui_->mono->setChecked(s.value("mono", false).toBool());

  s.endGroup();

}

void TranscoderOptionsMP3::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  s.setValue("target", ui_->target_quality->isChecked() ? 0 : 1);
  s.setValue("quality", ui_->quality_spinbox->value());
  s.setValue("bitrate", ui_->bitrate_slider->value());
  s.setValue("cbr", ui_->cbr->isChecked());
  s.setValue("encoding-engine-quality", ui_->encoding_engine_quality->currentIndex());
  s.setValue("mono", ui_->mono->isChecked());

  s.endGroup();

}

void TranscoderOptionsMP3::QualitySliderChanged(const int value) {
  ui_->quality_spinbox->setValue(static_cast<float>(value) / 100.0);
}

void TranscoderOptionsMP3::QualitySpinboxChanged(const double value) {
  ui_->quality_slider->setValue(static_cast<int>(value * 100.0));
}
