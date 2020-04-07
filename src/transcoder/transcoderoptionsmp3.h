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

#ifndef TRANSCODEROPTIONSMP3_H
#define TRANSCODEROPTIONSMP3_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

#include "transcoderoptionsinterface.h"

class Ui_TranscoderOptionsMP3;

class TranscoderOptionsMP3 : public TranscoderOptionsInterface {
  Q_OBJECT

 public:
  explicit TranscoderOptionsMP3(QWidget *parent = nullptr);
  ~TranscoderOptionsMP3();

  void Load();
  void Save();

 private slots:
  void QualitySliderChanged(int value);
  void QualitySpinboxChanged(double value);

 private:
  static const char* kSettingsGroup;

  Ui_TranscoderOptionsMP3 *ui_;
};

#endif  // TRANSCODEROPTIONSMP3_H
