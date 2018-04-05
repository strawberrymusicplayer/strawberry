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

#include "config.h"

#include "backendsettingspage.h"
#include "ui_backendsettingspage.h"

#include <QVariant>
#include <QString>
#include <QSettings>

#include "settingsdialog.h"
#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "core/utilities.h"
#include "core/iconloader.h"
#include "engine/enginetype.h"
#include "engine/enginebase.h"
#ifdef HAVE_GSTREAMER
#include "engine/gstengine.h"
#endif
#ifdef HAVE_XINE
#include "engine/xineengine.h"
#endif
#include "engine/devicefinder.h"

#include "dialogs/errordialog.h"

const char *BackendSettingsPage::kSettingsGroup = "Backend";
const char *BackendSettingsPage::EngineText_Xine = "Xine";
const char *BackendSettingsPage::EngineText_GStreamer = "GStreamer";
const char *BackendSettingsPage::EngineText_Phonon = "Phonon";
const char *BackendSettingsPage::EngineText_VLC = "VLC";

BackendSettingsPage::BackendSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_BackendSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("soundcard"));

  connect(ui_->combobox_engine, SIGNAL(currentIndexChanged(int)), SLOT(EngineChanged(int)));
  connect(ui_->combobox_output, SIGNAL(currentIndexChanged(int)), SLOT(OutputChanged(int)));
  connect(ui_->combobox_device, SIGNAL(currentIndexChanged(int)), SLOT(DeviceSelectionChanged(int)));
  connect(ui_->lineedit_device, SIGNAL(textChanged(const QString &)), SLOT(DeviceStringChanged()));

  connect(ui_->slider_bufferminfill, SIGNAL(valueChanged(int)), SLOT(BufferMinFillChanged(int)));
  ui_->label_bufferminfillvalue->setMinimumWidth(QFontMetrics(ui_->label_bufferminfillvalue->font()).width("WW%"));

  connect(ui_->stickslider_replaygainpreamp, SIGNAL(valueChanged(int)), SLOT(RgPreampChanged(int)));
  ui_->label_replaygainpreamp->setMinimumWidth(QFontMetrics(ui_->label_replaygainpreamp->font()).width("-WW.W dB"));

  RgPreampChanged(ui_->stickslider_replaygainpreamp->value());

  s_.beginGroup(BackendSettingsPage::kSettingsGroup);

}

BackendSettingsPage::~BackendSettingsPage() {

  s_.endGroup();
  delete ui_;

}

void BackendSettingsPage::Load() {

  configloaded_ = false;
  engineloaded_ = Engine::None;

  Engine::EngineType enginetype = Engine::EngineTypeFromName(s_.value("engine", EngineText_Xine).toString());

  ui_->combobox_engine->clear();
#ifdef HAVE_XINE
  ui_->combobox_engine->addItem(IconLoader::Load("xine"), EngineText_Xine, Engine::Xine);
#endif
#ifdef HAVE_GSTREAMER
  ui_->combobox_engine->addItem(IconLoader::Load("gstreamer"), EngineText_GStreamer, Engine::GStreamer);
#endif
#ifdef HAVE_PHONON
  ui_->combobox_engine->addItem(IconLoader::Load("speaker"),  EngineText_Phonon, Engine::Phonon);
#endif
#ifdef HAVE_VLC
  ui_->combobox_engine->addItem(IconLoader::Load("vlc"), EngineText_VLC, Engine::VLC);
#endif

  configloaded_ = true;
  enginereset_ = false;

  ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(enginetype));
  if (enginetype != engineloaded_) Load_Engine(enginetype);

  ui_->spinbox_bufferduration->setValue(s_.value("bufferduration", 4000).toInt());
  ui_->checkbox_monoplayback->setChecked(s_.value("monoplayback", false).toBool());
  ui_->slider_bufferminfill->setValue(s_.value("bufferminfill", 33).toInt());

  ui_->checkbox_replaygain->setChecked(s_.value("rgenabled", false).toBool());
  ui_->combobox_replaygainmode->setCurrentIndex(s_.value("rgmode", 0).toInt());
  ui_->stickslider_replaygainpreamp->setValue(s_.value("rgpreamp", 0.0).toDouble() * 10 + 150);
  ui_->checkbox_replaygaincompression->setChecked(s_.value("rgcompression", true).toBool());

  //if (dialog()->app()->player()->engine()->state() != Engine::Empty) ui_->combobox_engine->setEnabled(false);
  
#ifdef Q_OS_WIN32
  ui_->combobox_engine->setEnabled(false);
#endif

}

void BackendSettingsPage::Load_Engine(Engine::EngineType enginetype) {

  QString output = s_.value("output", "").toString();
  QVariant device = s_.value("device", QVariant());

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();

  ui_->combobox_output->setEnabled(false);
  ui_->combobox_device->setEnabled(false);

  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->setText("");
  
  ui_->groupbox_replaygain->setEnabled(false);

  // If a engine is loaded (!= Engine::None) AND engine has been switched reset output and device.
  if ((engineloaded_ != Engine::None) && (engineloaded_ != enginetype)) {
    output = "";
    device = QVariant();
    s_.setValue("output", "");
    s_.setValue("device", QVariant());
  }

  if (dialog()->app()->player()->engine()->type() != enginetype) {
    dialog()->app()->player()->CreateEngine(enginetype);
    dialog()->app()->player()->ReloadSettings();
    dialog()->app()->player()->Init();
  }

  switch(enginetype) {
#ifdef HAVE_XINE
    case Engine::Xine:
      Xine_Load(output, device);
      break;
#endif
#ifdef HAVE_GSTREAMER
    case Engine::GStreamer:
      Gst_Load(output, device);
      break;
#endif
#ifdef HAVE_PHONON
    case Engine::Phonon:
      Phonon_Load(output, device);
      break;
#endif
#ifdef HAVE_VLC
    case Engine::VLC:
      VLC_Load(output, device);
      break;
#endif
    default:
      QString msg = QString("Missing engine %1!").arg(Engine::EngineNameFromType(enginetype));
      errordialog_.ShowMessage(msg);
      return;
  }
    
}

void BackendSettingsPage::Load_Device(QString output, QVariant device, bool alsa) {

  int devices = 0;
  DeviceFinder::Device dfdevice;

  ui_->combobox_device->clear();
  ui_->combobox_device->setEnabled(false);
  ui_->lineedit_device->setText("");

#ifdef Q_OS_LINUX
  ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Automatically select", "");
#endif

  if (alsa) ui_->lineedit_device->setEnabled(true);
  else ui_->lineedit_device->setEnabled(false);

  for (DeviceFinder *f : dialog()->app()->enginedevice()->device_finders_) {
    if (f->name() == "alsa" && !alsa) continue;
    for (const DeviceFinder::Device &d : f->ListDevices()) {
      devices++;
      ui_->combobox_device->addItem(IconLoader::Load(d.iconname), d.description, d.value);
      if (d.value == device) { dfdevice = d; }
    }
  }

  if (alsa) ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Custom", QVariant(""));

  bool found = false;
  if (devices > 0) ui_->combobox_device->setEnabled(true);
  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant d = ui_->combobox_device->itemData(i).value<QVariant>();
    if (dfdevice.value == d) {
      ui_->combobox_device->setCurrentIndex(i);
      found = true;
      break;
    }
  }

  // This allows a custom ALSA device string ie: "hw:0,0" even if it is not listed.
  if (found == false && alsa && device.type() == QVariant::String && !device.toString().isEmpty()) {
    ui_->lineedit_device->setText(device.toString());
  }

}

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_Load(QString output, QVariant device) {

  if (output == "") output = GstEngine::kAutoSink;

  if (dialog()->app()->player()->engine()->type() != Engine::GStreamer) {
    errordialog_.ShowMessage("GStramer not initialized! Please restart.");
    return;
  }
  
  GstEngine *gstengine = qobject_cast<GstEngine*>(dialog()->app()->player()->engine());

  ui_->combobox_output->clear();
  int i = 0;
  for (const EngineBase::OutputDetails &o : gstengine->GetOutputsList()) {
    i++;
    ui_->combobox_output->addItem(IconLoader::Load(o.iconname), o.description, QVariant::fromValue(o));
  }
  if (i > 0) ui_->combobox_output->setEnabled(true);

  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (o.name == output) {
      ui_->combobox_output->setCurrentIndex(i);
      break;
    }
  }

  engineloaded_=Engine::GStreamer;
  ui_->groupbox_replaygain->setEnabled(true);
  
  Load_Device(output, device, GstEngine::ALSADeviceSupport(output));

}
#endif

#ifdef HAVE_XINE
void BackendSettingsPage::Xine_Load(QString output, QVariant device) {

  if (output == "") output = "auto";

  if (dialog()->app()->player()->engine()->type() != Engine::Xine) {
    errordialog_.ShowMessage("Xine not initialized! Please restart.");
    return;
  }
  XineEngine *xineengine = qobject_cast<XineEngine*>(dialog()->app()->player()->engine());
  
  ui_->combobox_output->clear();
  int i = 0;
  for (const EngineBase::OutputDetails &o : xineengine->GetOutputsList()) {
    i++;
    ui_->combobox_output->addItem(IconLoader::Load(o.iconname), o.description, QVariant::fromValue(o));
  }
  if (i > 0) ui_->combobox_output->setEnabled(true);

  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (o.name == output) {
      ui_->combobox_output->setCurrentIndex(i);
      break;
    }
  }
  
  engineloaded_=Engine::Xine;
  
  Load_Device(output, device, false);
    
}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_Load(QString output, QVariant device) {

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();
  ui_->lineedit_device->setText("");
  
  engineloaded_=Engine::Phonon;
  
  Load_Device(output, device, false);

}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_Load(QString output, QVariant device) {

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();
  ui_->lineedit_device->setText("");

  engineloaded_=Engine::VLC;
  
  Load_Device(output, device, false);

}
#endif

void BackendSettingsPage::Save() {

  s_.setValue("engine", ui_->combobox_engine->itemText(ui_->combobox_engine->currentIndex()).toLower());
  
  QVariant myVariant = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex());
  Engine::EngineType enginetype = myVariant.value<Engine::EngineType>();

  switch(enginetype) {
#ifdef HAVE_XINE
    case Engine::Xine:
      Xine_Save();
      break;
#endif
#ifdef HAVE_GSTREAMER
    case Engine::GStreamer:
      Gst_Save();
      break;
#endif
#ifdef HAVE_PHONON
    case Engine::Phonon:
      Phonon_Save();
      break;
#endif
#ifdef HAVE_VLC
    case Engine::VLC:
      VLC_Save();
      break;
#endif
    default:
      break;
  }

  s_.setValue("device", ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()).value<QVariant>());

  s_.setValue("bufferduration", ui_->spinbox_bufferduration->value());
  s_.setValue("monoplayback", ui_->checkbox_monoplayback->isChecked());
  s_.setValue("bufferminfill", ui_->slider_bufferminfill->value());
  s_.setValue("rgenabled", ui_->checkbox_replaygain->isChecked());
  s_.setValue("rgmode", ui_->combobox_replaygainmode->currentIndex());
  s_.setValue("rgpreamp", float(ui_->stickslider_replaygainpreamp->value()) / 10 - 15);
  s_.setValue("rgcompression", ui_->checkbox_replaygaincompression->isChecked());
  
}

#ifdef HAVE_XINE
void BackendSettingsPage::Xine_Save() {

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  s_.setValue("output", output.name);

}
#endif

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_Save() {

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  s_.setValue("output", output.name);

}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_Save() {
}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_Save() {
}
#endif

void BackendSettingsPage::EngineChanged(int index) {

  if (configloaded_ == false) return;
  
  if ((engineloaded_ != Engine::None) && (dialog()->app()->player()->engine()->state() != Engine::Empty)) {
      if (enginereset_ == true) { enginereset_ = false; return; }
      errordialog_.ShowMessage("Can't switch engine while playing!");
      enginereset_ = true;
      ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(engineloaded_));
      return;
  }
  
  QVariant v = ui_->combobox_engine->itemData(index);
  Engine::EngineType enginetype = v.value<Engine::EngineType>();

  Load_Engine(enginetype);

}

void BackendSettingsPage::OutputChanged(int index) {

  QVariant v = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex());
  Engine::EngineType enginetype = v.value<Engine::EngineType>();
  OutputChanged(index, enginetype);

}

void BackendSettingsPage::OutputChanged(int index, Engine::EngineType enginetype) {

  switch(enginetype) {
    case Engine::Xine:
#ifdef HAVE_XINE
      Xine_OutputChanged(index);
      break;
#endif
    case Engine::GStreamer:
#ifdef HAVE_GSTREAMER
      Gst_OutputChanged(index);
      break;
#endif
    case Engine::Phonon:
#ifdef HAVE_PHONON
      Phonon_OutputChanged(index);
      break;
#endif
    case Engine::VLC:
#ifdef HAVE_VLC
      VLC_OutputChanged(index);
      break;
#endif
    default:
      break;
  }

}

#ifdef HAVE_XINE
void BackendSettingsPage::Xine_OutputChanged(int index) {

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();
  Load_Device(output.name, QVariant(), false);

}
#endif

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_OutputChanged(int index) {

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();
  Load_Device(output.name, QVariant(), GstEngine::ALSADeviceSupport(output.name));

}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_OutputChanged(int index) {
  Load_Device("", QVariant(), false);
}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_OutputChanged(int index) {
  Load_Device("", QVariant(), false);
}
#endif

void BackendSettingsPage::DeviceSelectionChanged(int index) {

  if (ui_->combobox_device->currentText() == "Custom") {
    ui_->lineedit_device->setEnabled(true);
    ui_->combobox_device->setItemData(index, QVariant(ui_->lineedit_device->text()));
    return;
  }

  QVariant device = ui_->combobox_device->itemData(index).value<QVariant>();
  if (device.type() == QVariant::String) {
    ui_->lineedit_device->setEnabled(true);
    ui_->lineedit_device->setText(device.toString());
    return;
  }

  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->setText("");

}

void BackendSettingsPage::DeviceStringChanged() {

  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant v = ui_->combobox_device->itemData(i).value<QVariant>();
    if (v.type() != QVariant::String) continue;
    if (v.toString() == ui_->lineedit_device->text()) {
      ui_->combobox_device->setCurrentIndex(i);
      return;
    }
  }
  
  // Assume this is a custom alsa device string

  if (ui_->combobox_device->currentText() != "Custom") {
    for (int i = 0; i < ui_->combobox_device->count(); ++i) {
      if (ui_->combobox_device->itemText(i) == "Custom") {
        ui_->combobox_device->setCurrentIndex(i);
        break;
      }
    }
  }
  if (ui_->combobox_device->currentText() == "Custom") {      
    ui_->combobox_device->setItemData(ui_->combobox_device->currentIndex(), QVariant(ui_->lineedit_device->text()));
  }

}

void BackendSettingsPage::RgPreampChanged(int value) {

  float db = float(value) / 10 - 15;
  QString db_str;
  db_str.sprintf("%+.1f dB", db);
  ui_->label_replaygainpreamp->setText(db_str);

}

void BackendSettingsPage::BufferMinFillChanged(int value) {
  ui_->label_bufferminfillvalue->setText(QString::number(value) + "%");
}
