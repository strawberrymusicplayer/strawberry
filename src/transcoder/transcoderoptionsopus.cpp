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
#include <QSettings>

#include "transcoderoptionsinterface.h"
#include "transcoderoptionsopus.h"
#include "ui_transcoderoptionsopus.h"

#include "core/settings.h"

// TODO: Add more options than only bitrate as soon as gst doesn't crash anymore while using the cbr parameter (like cbr=false)

namespace {
constexpr char kSettingsGroup[] = "Transcoder/opusenc";
}

TranscoderOptionsOpus::TranscoderOptionsOpus(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsOpus) {
  ui_->setupUi(this);
}

TranscoderOptionsOpus::~TranscoderOptionsOpus() {
  delete ui_;
}

void TranscoderOptionsOpus::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  ui_->bitrate_slider->setValue(s.value("bitrate", 320000).toInt() / 1000);
  s.endGroup();

}

void TranscoderOptionsOpus::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue("bitrate", ui_->bitrate_slider->value() * 1000);
  s.endGroup();

}
