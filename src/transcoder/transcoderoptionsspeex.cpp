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
#include <QString>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsspeex.h"
#include "ui_transcoderoptionsspeex.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/speexenc";
constexpr char kQuality[] = "quality";
constexpr char kBitrate[] = "bitrate";
constexpr char kMode[] = "mode";
constexpr char kVbr[] = "vbr";
constexpr char kAbr[] = "abr";
constexpr char kVad[] = "vad";
constexpr char kDtx[] = "dtx";
constexpr char kComplexity[] = "complexity";
constexpr char kNframes[] = "nframes";
}  // namespace

TranscoderOptionsSpeex::TranscoderOptionsSpeex(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsSpeex) {
  ui_->setupUi(this);
}

TranscoderOptionsSpeex::~TranscoderOptionsSpeex() {
  delete ui_;
}

void TranscoderOptionsSpeex::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  ui_->quality_slider->setValue(s.value(kQuality, 10).toInt());
  ui_->bitrate_slider->setValue(s.value(kBitrate, 0).toInt() / 1000);
  ui_->mode->setCurrentIndex(s.value(kMode, 0).toInt());
  ui_->vbr->setChecked(s.value(kVbr, false).toBool());
  ui_->abr_slider->setValue(s.value(kAbr, 0).toInt() / 1000);
  ui_->vad->setChecked(s.value(kVad, false).toBool());
  ui_->dtx->setChecked(s.value(kDtx, false).toBool());
  ui_->complexity->setValue(s.value(kComplexity, 3).toInt());
  ui_->nframes->setValue(s.value(kNframes, 1).toInt());

  s.endGroup();

}

void TranscoderOptionsSpeex::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  s.setValue(kQuality, ui_->quality_slider->value());
  s.setValue(kBitrate, ui_->bitrate_slider->value() * 1000);
  s.setValue(kMode, ui_->mode->currentIndex());
  s.setValue(kVbr, ui_->vbr->isChecked());
  s.setValue(kAbr, ui_->abr_slider->value() * 1000);
  s.setValue(kVad, ui_->vad->isChecked());
  s.setValue(kDtx, ui_->dtx->isChecked());
  s.setValue(kComplexity, ui_->complexity->value());
  s.setValue(kNframes, ui_->nframes->value());

  s.endGroup();

}
