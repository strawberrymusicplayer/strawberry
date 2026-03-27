/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#ifndef RADIOSETTINGSPAGE_H
#define RADIOSETTINGSPAGE_H

#include <QObject>
#include <QList>
#include <QPair>
#include <QString>

#include "settings/settingspage.h"

class QComboBox;
class SettingsDialog;
class Ui_RadioSettingsPage;

class RadioSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit RadioSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~RadioSettingsPage() override;

  static QList<QPair<QString, QString>> CountryList();
  static void PopulateCountries(QComboBox *combo);

  void Load() override;
  void Save() override;

 private:
  Ui_RadioSettingsPage *ui_;
};

#endif  // RADIOSETTINGSPAGE_H
