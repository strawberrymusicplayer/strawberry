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

#ifndef EQUALISERSLIDER_H
#define EQUALISERSLIDER_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>

class Ui_EqualizerSlider;

// Contains the slider and the label
class EqualizerSlider : public QWidget {
  Q_OBJECT

 public:
  explicit EqualizerSlider(const QString &label, QWidget *parent = nullptr);
  ~EqualizerSlider() override;

  int value() const;
  void set_value(const int value);

 Q_SIGNALS:
  void ValueChanged(const int value);

 public Q_SLOTS:
  void OnValueChanged(const int value);

 private:
  Ui_EqualizerSlider *ui_;
};

#endif  // EQUALISERSLIDER_H
