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
#include "transcoderoptionsflac.h"
#include "ui_transcoderoptionsflac.h"

#include "core/settings.h"

namespace {
constexpr char kSettingsGroup[] = "Transcoder/flacenc";
}

TranscoderOptionsFLAC::TranscoderOptionsFLAC(QWidget *parent) : TranscoderOptionsInterface(parent), ui_(new Ui_TranscoderOptionsFLAC) {
  ui_->setupUi(this);
}

TranscoderOptionsFLAC::~TranscoderOptionsFLAC() {
  delete ui_;
}

void TranscoderOptionsFLAC::Load() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  ui_->quality->setValue(s.value("quality", 5).toInt());
  s.endGroup();

}

void TranscoderOptionsFLAC::Save() {

  Settings s;
  s.beginGroup(QLatin1String(kSettingsGroup) + settings_postfix_);
  s.setValue("quality", ui_->quality->value());
  s.endGroup();

}
