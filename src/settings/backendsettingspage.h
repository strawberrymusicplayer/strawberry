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

#include <QString>
#include <QSettings>

#include "backendsettingspage.h"

#include "settingspage.h"

#include "engine/engine_fwd.h"
#include "engine/enginetype.h"

#include "dialogs/errordialog.h"

class Ui_BackendSettingsPage;

class BackendSettingsPage : public SettingsPage {
  Q_OBJECT

public:
  BackendSettingsPage(SettingsDialog *dialog);
  ~BackendSettingsPage();
  
  static const char *kSettingsGroup;
  static const char *EngineText_Xine;
  static const char *EngineText_GStreamer;
  static const char *EngineText_Phonon;
  static const char *EngineText_VLC;

  void Load();
  void Save();

 private slots:
  void EngineChanged(int index);
  void OutputChanged(int index);
  void DeviceSelectionChanged(int index);
  void DeviceStringChanged();
  void RgPreampChanged(int value);
  void BufferMinFillChanged(int value);

private:
  Ui_BackendSettingsPage *ui_;
  
  void EngineChanged(Engine::EngineType enginetype);
  void OutputChanged(int index, Engine::EngineType enginetype);

  void Load_Engine(Engine::EngineType enginetype);
  void Load_Device(QString output, QVariant device, bool alsa);

#ifdef HAVE_XINE
  void Xine_Load(QString output, QVariant device);
  void Xine_Save();
  void Xine_OutputChanged(int index);
#endif

#ifdef HAVE_GSTREAMER
  void Gst_Load(QString output, QVariant device);
  void Gst_Save();
  void Gst_OutputChanged(int index);
#endif
  
#ifdef HAVE_PHONON
  void Phonon_Load(QString output, QVariant device);
  void Phonon_Save();
  void Phonon_OutputChanged(int index);
#endif

#ifdef HAVE_VLC
  void VLC_Load(QString output, QVariant device);
  void VLC_Save();
  void VLC_OutputChanged(int index);
#endif
  
  bool configloaded_;
  Engine::EngineType engineloaded_;
  QSettings s_;
  ErrorDialog errordialog_;
  bool enginereset_;
  
};

#endif  // BACKENDSETTINGSPAGE_H
