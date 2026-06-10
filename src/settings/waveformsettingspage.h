/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry contributors
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

#ifndef WAVEFORMSETTINGSPAGE_H
#define WAVEFORMSETTINGSPAGE_H

#include "settingspage.h"

#include <QObject>
#include <QColor>

class SettingsDialog;
class Ui_WaveformSettingsPage;

class WaveformSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit WaveformSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~WaveformSettingsPage() override;

  void Load() override;
  void Save() override;
  void Cancel() override;

 private Q_SLOTS:
  void WaveformSelectColor();

 private:
  static void UpdateColorButtonStyle(QWidget *button, const QColor &color);

  Ui_WaveformSettingsPage *ui_;

  QColor current_waveform_color_;
  bool color_is_custom_;
};

#endif  // WAVEFORMSETTINGSPAGE_H
