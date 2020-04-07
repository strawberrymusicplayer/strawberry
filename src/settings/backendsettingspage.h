/*
 * Strawberry Music Player
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#ifndef BACKENDSETTINGSPAGE_H
#define BACKENDSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QVariant>
#include <QString>
#include <QSettings>

#include "engine/enginetype.h"
#include "dialogs/errordialog.h"
#include "settingspage.h"

#include "core/application.h"
#include "core/player.h"
#include "engine/engine_fwd.h"

class SettingsDialog;
class Ui_BackendSettingsPage;

class BackendSettingsPage : public SettingsPage {
  Q_OBJECT

public:
  explicit BackendSettingsPage(SettingsDialog *dialog);
  ~BackendSettingsPage();

  static const char *kSettingsGroup;

  void Load();
  void Save();
  void Cancel();

  EngineBase *engine() const { return dialog()->app()->player()->engine(); }

 private slots:
  void EngineChanged(int index);
  void OutputChanged(int index);
  void DeviceSelectionChanged(int index);
  void DeviceStringChanged();
  void RgPreampChanged(int value);
  void BufferMinFillChanged(int value);
  void radiobutton_alsa_hw_clicked(bool checked);
  void radiobutton_alsa_plughw_clicked(bool checked);
  void FadingOptionsChanged();

private:
#ifdef HAVE_ALSA
  enum alsa_plugin {
    alsa_hw = 1,
    alsa_plughw = 2
  };
#endif

  Ui_BackendSettingsPage *ui_;

  bool EngineInitialised();

  void EngineChanged(Engine::EngineType enginetype);

  void Load_Engine(Engine::EngineType enginetype);
  void Load_Output(QString output, QVariant device);
  void Load_Device(QString output, QVariant device);
#ifdef HAVE_ALSA
  void SwitchALSADevices(alsa_plugin alsaplugin);
#endif

  QSettings s_;
  bool configloaded_;
  bool engineloaded_;
  ErrorDialog errordialog_;

  Engine::EngineType enginetype_current_;
  QString output_current_;
  QVariant device_current_;

};

#endif  // BACKENDSETTINGSPAGE_H
