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
#include <QMessageBox>
#include <QErrorMessage>

#include "settingsdialog.h"
#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "core/utilities.h"
#include "core/iconloader.h"
#include "engine/enginetype.h"
#include "engine/enginebase.h"
#include "engine/gstengine.h"
#include "engine/xineengine.h"
#include "engine/devicefinder.h"

const char *BackendSettingsPage::kSettingsGroup = "Backend";
const char *BackendSettingsPage::EngineText_Xine = "Xine";
const char *BackendSettingsPage::EngineText_GStreamer = "GStreamer";
const char *BackendSettingsPage::EngineText_Phonon = "Phonon";
const char *BackendSettingsPage::EngineText_VLC = "VLC";

BackendSettingsPage::BackendSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_BackendSettingsPage) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

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
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  //dialog()->app()->player()->CreateEngine(engineloaded_);
  //dialog()->app()->player()->ReloadSettings();
  //dialog()->app()->player()->Init();
  
  s_.endGroup();

  delete ui_;

}

void BackendSettingsPage::Load() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

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

  ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(enginetype));
  if (enginetype != engineloaded_) Load_Engine(enginetype);
  
  ui_->spinbox_bufferduration->setValue(s_.value("bufferduration", 4000).toInt());
  ui_->checkbox_monoplayback->setChecked(s_.value("monoplayback", false).toBool());
  ui_->slider_bufferminfill->setValue(s_.value("bufferminfill", 33).toInt());

  ui_->checkbox_replaygain->setChecked(s_.value("rgenabled", false).toBool());
  ui_->combobox_replaygainmode->setCurrentIndex(s_.value("rgmode", 0).toInt());
  ui_->stickslider_replaygainpreamp->setValue(s_.value("rgpreamp", 0.0).toDouble() * 10 + 150);
  ui_->checkbox_replaygaincompression->setChecked(s_.value("rgcompression", true).toBool());

}

void BackendSettingsPage::Load_Engine(Engine::EngineType enginetype) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  output_ = s_.value("output", "").toString();
  device_ = s_.value("device", "").toString();

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();

  ui_->combobox_output->setEnabled(false);
  ui_->combobox_device->setEnabled(false);

  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->setText("");

  // If a engine is loaded (!= Engine::None) AND engine has been switched reset output and device.
  if ((engineloaded_ != Engine::None) && (engineloaded_ != enginetype)) {
    output_ = "";
    device_ = "";
    s_.setValue("output", "");
    s_.setValue("device", "");
  }

  if (dialog()->app()->player()->engine()->type() != enginetype) {
    dialog()->app()->player()->CreateEngine(enginetype);
    dialog()->app()->player()->ReloadSettings();
    dialog()->app()->player()->Init();
  }

  switch(enginetype) {
#ifdef HAVE_XINE
    case Engine::Xine:
      Xine_Load();
      break;
#endif
#ifdef HAVE_GSTREAMER
    case Engine::GStreamer:
      Gst_Load();
      break;
#endif
#ifdef HAVE_PHONON
    case Engine::Phonon:
      Phonon_Load();
      break;
#endif
#ifdef HAVE_VLC
    case Engine::VLC:
      VLC_Load();
      break;
#endif
    default:
      QMessageBox messageBox;
      QString msg = QString("Missing engine %1!").arg(Engine::EngineNameFromType(enginetype));
      messageBox.critical(nullptr, "Error", msg);
      messageBox.setFixedSize(500, 200);
      return;
  }
    
}

void BackendSettingsPage::Load_Device(QString output) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  ui_->combobox_device->setEnabled(false);
  ui_->combobox_device->clear();
  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->setText("");

  ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Automatically select", "");
  ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Custom", "");
  int i = 0;
  for (DeviceFinder *finder : dialog()->app()->enginedevice()->device_finders_) {
    if (finder->output() != output) continue;
    for (const DeviceFinder::Device &device : finder->ListDevices()) {
      i++;
      ui_->combobox_device->addItem(IconLoader::Load(device.iconname), device.description, device.string);
    }
  }
  if (i > 0) {
    ui_->combobox_device->setEnabled(true);
    ui_->lineedit_device->setEnabled(true);
    ui_->lineedit_device->setText(device_);

  }

}

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_Load() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (output_ == "") output_ = GstEngine::kAutoSink;

  if (dialog()->app()->player()->engine()->type() != Engine::GStreamer) {
    QMessageBox messageBox;
    messageBox.critical(nullptr, "Error", "GStramer not initialized! Please restart.");
    messageBox.setFixedSize(500, 200);
    return;
  }
  GstEngine *gstengine = qobject_cast<GstEngine*>(dialog()->app()->player()->engine());

  ui_->combobox_output->clear();
  int i = 0;
  for (const EngineBase::OutputDetails &output : gstengine->GetOutputsList()) {
    i++;
    ui_->combobox_output->addItem(IconLoader::Load(output.iconname), output.description, QVariant::fromValue(output));
    //qLog(Debug) << output.description;
  }
  if (i > 0) ui_->combobox_output->setEnabled(true);

  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails details = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (details.name == output_) {
      ui_->combobox_output->setCurrentIndex(i);
      break;
    }
  }

  engineloaded_=Engine::GStreamer;

}
#endif

#ifdef HAVE_XINE
void BackendSettingsPage::Xine_Load() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (output_ == "") output_ = "auto";

  if (dialog()->app()->player()->engine()->type() != Engine::Xine) {
    QMessageBox messageBox;
    messageBox.critical(nullptr, "Error", "Xine not initialized! Please restart.");
    messageBox.setFixedSize(500, 200);
    return;
  }
  XineEngine *xineengine = qobject_cast<XineEngine*>(dialog()->app()->player()->engine());
  
  ui_->combobox_output->clear();
  int i = 0;
  for (const EngineBase::OutputDetails &output : xineengine->GetOutputsList()) {
    i++;
    ui_->combobox_output->addItem(IconLoader::Load(output.iconname), output.description, QVariant::fromValue(output));
    //qLog(Debug) << output.description;
  }
  if (i > 0) ui_->combobox_output->setEnabled(true);

  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails details = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (details.name == output_) {
      ui_->combobox_output->setCurrentIndex(i);
      break;
    }
  }
  
  engineloaded_=Engine::Xine;
    
}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_Load() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();
  ui_->lineedit_device->setText("");
  
  engineloaded_=Engine::Phonon;

}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_Load() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();
  ui_->lineedit_device->setText("");

  engineloaded_=Engine::VLC;

}
#endif

void BackendSettingsPage::Save() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

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

  s_.setValue("device", ui_->lineedit_device->text());
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
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  XineEngine *xineengine = qobject_cast<XineEngine*>(dialog()->app()->player()->engine());

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  s_.setValue("output", output.name);
  
  for (EngineBase::OutputDetails &output : xineengine->GetOutputsList()) {
    if (xineengine->ALSADeviceSupport(output.name)) output.device_property_value = QVariant(ui_->lineedit_device->text());
    else output.device_property_value = QVariant(ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()));
  }

}
#endif

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_Save() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  GstEngine *gstengine = qobject_cast<GstEngine*>(dialog()->app()->player()->engine());

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  s_.setValue("output", output.name);

  for (EngineBase::OutputDetails &output : gstengine->GetOutputsList()) {
    if (GstEngine::ALSADeviceSupport(output.name)) output.device_property_value = QVariant(ui_->lineedit_device->text());
    else output.device_property_value = QVariant(ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()));
  }

}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_Save() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_Save() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

}
#endif

void BackendSettingsPage::EngineChanged(int index) {

  if (configloaded_ == false) return;

  //qLog(Debug) << __PRETTY_FUNCTION__;

  QVariant myVariant = ui_->combobox_engine->itemData(index);
  Engine::EngineType enginetype = myVariant.value<Engine::EngineType>();

  Load_Engine(enginetype);

}

void BackendSettingsPage::OutputChanged(int index) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  QVariant myVariant = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex());
  Engine::EngineType enginetype = myVariant.value<Engine::EngineType>();
  OutputChanged(index, enginetype);

}

void BackendSettingsPage::OutputChanged(int index, Engine::EngineType enginetype) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

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

  //qLog(Debug) << __PRETTY_FUNCTION__;

  EngineBase::OutputDetails details = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();
  QString name = details.name;
  if (XineEngine::ALSADeviceSupport(name)) Load_Device("alsa");
  else Load_Device(name);
}
#endif

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::Gst_OutputChanged(int index) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  EngineBase::OutputDetails details = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();
  QString name = details.name;
  if (GstEngine::ALSADeviceSupport(name)) Load_Device("alsa");
  else Load_Device(name);

}
#endif

#ifdef HAVE_PHONON
void BackendSettingsPage::Phonon_OutputChanged(int index) {
  //qLog(Debug) << __PRETTY_FUNCTION__;
  Load_Device("");
}
#endif

#ifdef HAVE_VLC
void BackendSettingsPage::VLC_OutputChanged(int index) {
  //qLog(Debug) << __PRETTY_FUNCTION__;
  Load_Device("");
}
#endif

void BackendSettingsPage::DeviceSelectionChanged(int index) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  if (index == 1) return;

  QString string = ui_->combobox_device->itemData(index).value<QString>();

  ui_->lineedit_device->setText(string);

}

void BackendSettingsPage::DeviceStringChanged() {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  QString string = ui_->lineedit_device->text();

  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QString s = ui_->combobox_device->itemData(i).value<QString>();
    if (s == string ) {
      ui_->combobox_device->setCurrentIndex(i);
      return;
    }
  }

  ui_->combobox_device->setCurrentIndex(1);

}

void BackendSettingsPage::RgPreampChanged(int value) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  float db = float(value) / 10 - 15;
  QString db_str;
  db_str.sprintf("%+.1f dB", db);
  ui_->label_replaygainpreamp->setText(db_str);

}

void BackendSettingsPage::BufferMinFillChanged(int value) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  ui_->label_bufferminfillvalue->setText(QString::number(value) + "%");
}
