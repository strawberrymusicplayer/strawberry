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
#include "constants/transcodersettings.h"

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

  if (s.value(TranscoderSettings::LameMP3Settings::kTarget, 1).toInt() == 0) {
    ui_->target_quality->setChecked(true);
  }
  else {
    ui_->target_bitrate->setChecked(true);
  }

  ui_->quality_spinbox->setValue(s.value(TranscoderSettings::LameMP3Settings::kQuality, 10.0F).toFloat());
  ui_->bitrate_slider->setValue(s.value(TranscoderSettings::LameMP3Settings::kBitrate, 320).toInt());
  ui_->cbr->setChecked(s.value(TranscoderSettings::LameMP3Settings::kCbr, false).toBool());
  ui_->encoding_engine_quality->setCurrentIndex(s.value(TranscoderSettings::LameMP3Settings::kEncodingEngineQuality, 2).toInt());
  ui_->mono->setChecked(s.value(TranscoderSettings::LameMP3Settings::kMono, false).toBool());

  s.endGroup();

}

void TranscoderOptionsMP3::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  s.setValue(TranscoderSettings::LameMP3Settings::kTarget, ui_->target_quality->isChecked() ? 0 : 1);
  s.setValue(TranscoderSettings::LameMP3Settings::kQuality, ui_->quality_spinbox->value());
  s.setValue(TranscoderSettings::LameMP3Settings::kBitrate, ui_->bitrate_slider->value());
  s.setValue(TranscoderSettings::LameMP3Settings::kCbr, ui_->cbr->isChecked());
  s.setValue(TranscoderSettings::LameMP3Settings::kEncodingEngineQuality, ui_->encoding_engine_quality->currentIndex());
  s.setValue(TranscoderSettings::LameMP3Settings::kMono, ui_->mono->isChecked());

  s.endGroup();

}

void TranscoderOptionsMP3::QualitySliderChanged(const int value) {
  ui_->quality_spinbox->setValue(static_cast<float>(value) / 100.0);
}

void TranscoderOptionsMP3::QualitySpinboxChanged(const double value) {
  ui_->quality_slider->setValue(static_cast<int>(value * 100.0));
}
