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
#include <QSlider>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsvorbis.h"
#include "ui_transcoderoptionsvorbis.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/vorbisenc";
}

TranscoderOptionsVorbis::TranscoderOptionsVorbis(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsVorbis) {
  ui_->setupUi(this);
}

TranscoderOptionsVorbis::~TranscoderOptionsVorbis() {
  delete ui_;
}

void TranscoderOptionsVorbis::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  int bitrate = s.value("bitrate", -1).toInt();
  bitrate = bitrate == -1 ? 0 : bitrate / 1000;

  int min_bitrate = s.value("min-bitrate", -1).toInt();
  min_bitrate = min_bitrate == -1 ? 0 : min_bitrate / 1000;

  int max_bitrate = s.value("max-bitrate", -1).toInt();
  max_bitrate = max_bitrate == -1 ? 0 : max_bitrate / 1000;

  ui_->quality_slider->setValue(static_cast<int>(s.value("quality", 1.0).toDouble() * 10));
  ui_->managed->setChecked(s.value("managed", false).toBool());
  ui_->max_bitrate_slider->setValue(max_bitrate);
  ui_->min_bitrate_slider->setValue(min_bitrate);
  ui_->bitrate_slider->setValue(bitrate);

  s.endGroup();

}

void TranscoderOptionsVorbis::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);

  int bitrate = ui_->bitrate_slider->value();
  bitrate = bitrate == 0 ? -1 : bitrate * 1000;

  int min_bitrate = ui_->min_bitrate_slider->value();
  min_bitrate = min_bitrate == 0 ? -1 : min_bitrate * 1000;

  int max_bitrate = ui_->max_bitrate_slider->value();
  max_bitrate = max_bitrate == 0 ? -1 : max_bitrate * 1000;

  s.setValue("quality", static_cast<double>(ui_->quality_slider->value()) / 10);
  s.setValue("managed", ui_->managed->isChecked());
  s.setValue("bitrate", bitrate);
  s.setValue("min-bitrate", min_bitrate);
  s.setValue("max-bitrate", max_bitrate);

  s.endGroup();

}
