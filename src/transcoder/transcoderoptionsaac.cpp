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

#include <QString>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsaac.h"
#include "ui_transcoderoptionsaac.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/faac";
}

TranscoderOptionsAAC::TranscoderOptionsAAC(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsAAC) {
  ui_->setupUi(this);
}

TranscoderOptionsAAC::~TranscoderOptionsAAC() {
  delete ui_;
}

void TranscoderOptionsAAC::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  ui_->bitrate_slider->setValue(s.value("bitrate", 320000).toInt() / 1000);
  ui_->profile->setCurrentIndex(s.value("profile", 2).toInt() - 1);
  ui_->tns->setChecked(s.value("tns", false).toBool());
  ui_->midside->setChecked(s.value("midside", true).toBool());
  ui_->shortctl->setCurrentIndex(s.value("shortctl", 0).toInt());
  s.endGroup();

}

void TranscoderOptionsAAC::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue("bitrate", ui_->bitrate_slider->value() * 1000);
  s.setValue("profile", ui_->profile->currentIndex() + 1);
  s.setValue("tns", ui_->tns->isChecked());
  s.setValue("midside", ui_->midside->isChecked());
  s.setValue("shortctl", ui_->shortctl->currentIndex());
  s.endGroup();

}
