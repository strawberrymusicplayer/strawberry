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
#include <QSlider>
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsasf.h"
#include "ui_transcoderoptionsasf.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/ffenc_wmav2";
}

TranscoderOptionsASF::TranscoderOptionsASF(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsASF) {
  ui_->setupUi(this);
}

TranscoderOptionsASF::~TranscoderOptionsASF() {
  delete ui_;
}

void TranscoderOptionsASF::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  ui_->bitrate_slider->setValue(s.value("bitrate", 320000).toInt() / 1000);
  s.endGroup();

}

void TranscoderOptionsASF::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue("bitrate", ui_->bitrate_slider->value() * 1000);
  s.endGroup();

}
