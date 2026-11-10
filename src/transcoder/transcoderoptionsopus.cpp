/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, Martin Brodbeck <martin@brodbeck-online.de>
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
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsopus.h"
#include "ui_transcoderoptionsopus.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/opusenc";
constexpr char kBitrate[] = "bitrate";
constexpr char kBitrateType[] = "bitrate-type";
constexpr char kComplexity[] = "complexity";
constexpr char kAudioType[] = "audio-type";
constexpr char kBandwidth[] = "bandwidth";
constexpr char kFrameSize[] = "frame-size";
constexpr char kDtx[] = "dtx";

// The values are the values of the opusenc enum properties, the defaults are the opusenc defaults.
constexpr int kBitrateTypeCBR = 0;
constexpr int kBitrateTypeVBR = 1;
constexpr int kBitrateTypeConstrainedVBR = 2;

constexpr int kAudioTypeVoice = 2048;
constexpr int kAudioTypeGeneric = 2049;
constexpr int kAudioTypeRestrictedLowDelay = 2051;

constexpr int kBandwidthAuto = -1000;
constexpr int kBandwidthNarrowband = 1101;
constexpr int kBandwidthMediumband = 1102;
constexpr int kBandwidthWideband = 1103;
constexpr int kBandwidthSuperWideband = 1104;
constexpr int kBandwidthFullband = 1105;

constexpr int kDefaultFrameSize = 20;
}

TranscoderOptionsOpus::TranscoderOptionsOpus(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsOpus) {

  ui_->setupUi(this);

  ui_->bitrate_type->addItem(tr("Constant bitrate (CBR)"), kBitrateTypeCBR);
  ui_->bitrate_type->addItem(tr("Variable bitrate (VBR)"), kBitrateTypeVBR);
  ui_->bitrate_type->addItem(tr("Constrained variable bitrate"), kBitrateTypeConstrainedVBR);

  ui_->audio_type->addItem(tr("Music"), kAudioTypeGeneric);
  ui_->audio_type->addItem(tr("Voice"), kAudioTypeVoice);
  ui_->audio_type->addItem(tr("Low delay"), kAudioTypeRestrictedLowDelay);

  ui_->bandwidth->addItem(tr("Automatic"), kBandwidthAuto);
  ui_->bandwidth->addItem(tr("Full band (20 kHz)"), kBandwidthFullband);
  ui_->bandwidth->addItem(tr("Super wide band (12 kHz)"), kBandwidthSuperWideband);
  ui_->bandwidth->addItem(tr("Wide band (8 kHz)"), kBandwidthWideband);
  ui_->bandwidth->addItem(tr("Medium band (6 kHz)"), kBandwidthMediumband);
  ui_->bandwidth->addItem(tr("Narrow band (4 kHz)"), kBandwidthNarrowband);

  // The opusenc value for 2.5 ms is 2.
  ui_->frame_size->addItem(tr("2.5 ms"), 2);
  for (const int frame_size : {5, 10, 20, 40, 60}) {
    ui_->frame_size->addItem(tr("%1 ms").arg(frame_size), frame_size);
  }

}

TranscoderOptionsOpus::~TranscoderOptionsOpus() {
  delete ui_;
}

void TranscoderOptionsOpus::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  ui_->bitrate_slider->setValue(s.value(kBitrate, 320000).toInt() / 1000);
  SetCurrentData(ui_->bitrate_type, s.value(kBitrateType, kBitrateTypeConstrainedVBR).toInt(), kBitrateTypeConstrainedVBR);
  ui_->complexity->setValue(s.value(kComplexity, 10).toInt());
  SetCurrentData(ui_->audio_type, s.value(kAudioType, kAudioTypeGeneric).toInt(), kAudioTypeGeneric);
  SetCurrentData(ui_->bandwidth, s.value(kBandwidth, kBandwidthFullband).toInt(), kBandwidthFullband);
  SetCurrentData(ui_->frame_size, s.value(kFrameSize, kDefaultFrameSize).toInt(), kDefaultFrameSize);
  ui_->dtx->setChecked(s.value(kDtx, false).toBool());
  s.endGroup();

}

void TranscoderOptionsOpus::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue(kBitrate, ui_->bitrate_slider->value() * 1000);
  s.setValue(kBitrateType, ui_->bitrate_type->currentData().toInt());
  s.setValue(kComplexity, ui_->complexity->value());
  s.setValue(kAudioType, ui_->audio_type->currentData().toInt());
  s.setValue(kBandwidth, ui_->bandwidth->currentData().toInt());
  s.setValue(kFrameSize, ui_->frame_size->currentData().toInt());
  s.setValue(kDtx, ui_->dtx->isChecked());
  s.endGroup();

}

void TranscoderOptionsOpus::SetCurrentData(QComboBox *combobox, const int value, const int default_value) {

  // Unknown values select the default, so the combobox doesn't show an empty item.
  int index = combobox->findData(value);
  if (index == -1) index = combobox->findData(default_value);
  combobox->setCurrentIndex(index);

}
