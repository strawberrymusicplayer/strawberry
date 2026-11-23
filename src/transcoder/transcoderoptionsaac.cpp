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

#include <gst/gst.h>

#include <QString>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QSettings>
#include <QFormLayout>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsaac.h"
#include "ui_transcoderoptionsaac.h"

#include "core/settings.h"
#include "core/song.h"
#include "transcoder.h"
#include "constants/transcodersettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "Transcoder/faac";
constexpr char kQuality[] = "quality";
constexpr char kTns[] = "tns";
constexpr char kMidside[] = "midside";
constexpr char kShortctl[] = "shortctl";
}

TranscoderOptionsAAC::TranscoderOptionsAAC(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsAAC), faac_(true) {

  ui_->setupUi(this);

  // The options are for faac, other AAC encoders, like fdkaacenc, mfaacenc and atenc, only use the bitrate, so the other options are hidden when faac is not the encoder used.
  const QString encoder = Transcoder::BestElementForMimeType(GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER, Transcoder::PresetForFileType(Song::FileType::MP4).codec_mimetype_);
  faac_ = encoder.isEmpty() || encoder == "faac"_L1;
  if (!faac_) {
    ui_->formLayout->setRowVisible(ui_->rate_control, false);
    ui_->formLayout->setRowVisible(ui_->horizontalLayout_quality, false);
    ui_->formLayout->setRowVisible(ui_->shortctl, false);
    ui_->formLayout->setRowVisible(ui_->tns, false);
    ui_->formLayout->setRowVisible(ui_->midside, false);
    ui_->encoder_info->setText(tr("The AAC encoder %1 only uses the bitrate.").arg(encoder));
  }
  ui_->formLayout->setRowVisible(ui_->encoder_info, !faac_);

  QObject::connect(ui_->rate_control, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TranscoderOptionsAAC::RateControlChanged);
  RateControlChanged(ui_->rate_control->currentIndex());

}

TranscoderOptionsAAC::~TranscoderOptionsAAC() {
  delete ui_;
}

void TranscoderOptionsAAC::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  // ABR is the default, since the bitrate was the only setting for the rate before.
  ui_->rate_control->setCurrentIndex(s.value(TranscoderSettings::FaacSettings::kRateControl, TranscoderSettings::FaacSettings::kRateControlABR).toInt() == TranscoderSettings::FaacSettings::kRateControlVBR ? 0 : 1);
  ui_->bitrate_slider->setValue(s.value(TranscoderSettings::FaacSettings::kBitrate, TranscoderSettings::FaacSettings::kDefaultBitrate).toInt() / 1000);
  ui_->quality_slider->setValue(s.value(kQuality, 100).toInt());
  ui_->tns->setChecked(s.value(kTns, false).toBool());
  ui_->midside->setChecked(s.value(kMidside, true).toBool());
  ui_->shortctl->setCurrentIndex(s.value(kShortctl, 0).toInt());
  s.endGroup();

}

void TranscoderOptionsAAC::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue(TranscoderSettings::FaacSettings::kRateControl, ui_->rate_control->currentIndex() == 0 ? TranscoderSettings::FaacSettings::kRateControlVBR : TranscoderSettings::FaacSettings::kRateControlABR);
  s.setValue(TranscoderSettings::FaacSettings::kBitrate, ui_->bitrate_slider->value() * 1000);
  s.setValue(kQuality, ui_->quality_slider->value());
  s.setValue(kTns, ui_->tns->isChecked());
  s.setValue(kMidside, ui_->midside->isChecked());
  s.setValue(kShortctl, ui_->shortctl->currentIndex());
  s.endGroup();

}

void TranscoderOptionsAAC::RateControlChanged(const int index) {

  // Other encoders always use the bitrate.
  const bool vbr = faac_ && index == 0;

  ui_->label_1->setEnabled(!vbr);
  ui_->bitrate_slider->setEnabled(!vbr);
  ui_->bitrate_spinbox->setEnabled(!vbr);

  ui_->label_quality->setEnabled(vbr);
  ui_->quality_slider->setEnabled(vbr);
  ui_->quality_spinbox->setEnabled(vbr);

}
