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

#ifndef TRANSCODEROPTIONSVORBIS_H
#define TRANSCODEROPTIONSVORBIS_H

#include "config.h"

#include <QWidget>

#include "transcoderoptionsinterface.h"

class Ui_TranscoderOptionsVorbis;

class TranscoderOptionsVorbis : public TranscoderOptionsInterface {
  Q_OBJECT

 public:
  explicit TranscoderOptionsVorbis(QWidget *parent = nullptr);
  ~TranscoderOptionsVorbis() override;

  void Load() override;
  void Save() override;

 private:
  Ui_TranscoderOptionsVorbis *ui_;
};

#endif  // TRANSCODEROPTIONSVORBIS_H
