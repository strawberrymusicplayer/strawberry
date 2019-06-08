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

#include <QtGlobal>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QFontMetrics>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QListView>

#include "backendsettingspage.h"

#include "core/application.h"
#include "core/iconloader.h"
#include "core/player.h"
#include "core/logging.h"
#include "engine/engine_fwd.h"
#include "engine/enginebase.h"
#include "engine/enginedevice.h"
#include "engine/enginetype.h"
#include "engine/devicefinder.h"
#include "widgets/lineedit.h"
#include "widgets/stickyslider.h"
#include "dialogs/errordialog.h"
#include "settings/settingspage.h"
#include "settingsdialog.h"
#include "ui_backendsettingspage.h"

const char *BackendSettingsPage::kSettingsGroup = "Backend";

BackendSettingsPage::BackendSettingsPage(SettingsDialog *dialog) : SettingsPage(dialog), ui_(new Ui_BackendSettingsPage) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("soundcard"));

  ui_->label_bufferminfillvalue->setMinimumWidth(QFontMetrics(ui_->label_bufferminfillvalue->font()).width("WW%"));
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
  engineloaded_ = false;

  Engine::EngineType enginetype = Engine::EngineTypeFromName(s_.value("engine", EngineName(Engine::None)).toString());
  if (enginetype == Engine::None && engine()) enginetype = engine()->type();

  ui_->combobox_engine->clear();
#ifdef HAVE_GSTREAMER
  ui_->combobox_engine->addItem(IconLoader::Load("gstreamer"), EngineDescription(Engine::GStreamer), QVariant::fromValue(Engine::GStreamer));
#endif
#ifdef HAVE_XINE
  ui_->combobox_engine->addItem(IconLoader::Load("xine"), EngineDescription(Engine::Xine), QVariant::fromValue(Engine::Xine));
#endif
#ifdef HAVE_VLC
  ui_->combobox_engine->addItem(IconLoader::Load("vlc"), EngineDescription(Engine::VLC), QVariant::fromValue(Engine::VLC));
#endif
#ifdef HAVE_PHONON
  ui_->combobox_engine->addItem(IconLoader::Load("speaker"), EngineDescription(Engine::Phonon), QVariant::fromValue(Engine::Phonon));
#endif

  enginetype_current_ = enginetype;
  output_current_ = s_.value("output", QString()).toString();
  device_current_ = s_.value("device", QVariant());

  ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(enginetype));
  if (EngineInitialised()) Load_Engine(enginetype);

  ui_->spinbox_bufferduration->setValue(s_.value("bufferduration", 4000).toInt());
  ui_->slider_bufferminfill->setValue(s_.value("bufferminfill", 33).toInt());

  ui_->checkbox_replaygain->setChecked(s_.value("rgenabled", false).toBool());
  ui_->combobox_replaygainmode->setCurrentIndex(s_.value("rgmode", 0).toInt());
  ui_->stickslider_replaygainpreamp->setValue(s_.value("rgpreamp", 0.0).toDouble() * 10 + 150);
  ui_->checkbox_replaygaincompression->setChecked(s_.value("rgcompression", true).toBool());

#if defined(HAVE_ALSA)
  bool fade_default = false;
#else
  bool fade_default = true;
#endif

  ui_->checkbox_fadeout_stop->setChecked(s_.value("FadeoutEnabled", fade_default).toBool());
  ui_->checkbox_fadeout_cross->setChecked(s_.value("CrossfadeEnabled", fade_default).toBool());
  ui_->checkbox_fadeout_auto->setChecked(s_.value("AutoCrossfadeEnabled", false).toBool());
  ui_->checkbox_fadeout_samealbum->setChecked(s_.value("NoCrossfadeSameAlbum", true).toBool());
  ui_->checkbox_fadeout_pauseresume->setChecked(s_.value("FadeoutPauseEnabled", false).toBool());
  ui_->spinbox_fadeduration->setValue(s_.value("FadeoutDuration", 2000).toInt());
  ui_->spinbox_fadeduration_pauseresume->setValue(s_.value("FadeoutPauseDuration", 250).toInt());

#if defined(HAVE_ALSA)
  ui_->lineedit_device->show();
  ui_->widget_alsa_plugin->show();
  int alsaplug_int = alsa_plugin(s_.value("alsaplugin", 0).toInt());
  if (alsa_plugin(alsaplug_int)) {
    alsa_plugin alsaplugin = alsa_plugin(alsaplug_int);
    switch (alsaplugin) {
      case alsa_plugin::alsa_hw:
        ui_->radiobutton_alsa_hw->setChecked(true);
        break;
      case alsa_plugin::alsa_plughw:
        ui_->radiobutton_alsa_plughw->setChecked(true);
        break;
    }
  }
#else
  ui_->lineedit_device->hide();
  ui_->widget_alsa_plugin->hide();
#endif

  ui_->checkbox_volume_control->setChecked(s_.value("volume_control", true).toBool());

  if (!EngineInitialised()) return;

  if (engine()->state() == Engine::Empty) {
    if (ui_->combobox_engine->count() > 1) ui_->combobox_engine->setEnabled(true);
    else ui_->combobox_engine->setEnabled(false);
  }
  else {
    ui_->combobox_engine->setEnabled(false);
  }

  ConnectSignals();

  configloaded_ = true;

}

void BackendSettingsPage::ConnectSignals() {

  connect(ui_->combobox_engine, SIGNAL(currentIndexChanged(int)), SLOT(EngineChanged(int)));
  connect(ui_->combobox_output, SIGNAL(currentIndexChanged(int)), SLOT(OutputChanged(int)));
  connect(ui_->combobox_device, SIGNAL(currentIndexChanged(int)), SLOT(DeviceSelectionChanged(int)));
  connect(ui_->lineedit_device, SIGNAL(textChanged(const QString &)), SLOT(DeviceStringChanged()));
#if defined(HAVE_ALSA)
  connect(ui_->radiobutton_alsa_hw, SIGNAL(clicked(bool)), SLOT(radiobutton_alsa_hw_clicked(bool)));
  connect(ui_->radiobutton_alsa_plughw, SIGNAL(clicked(bool)), SLOT(radiobutton_alsa_plughw_clicked(bool)));
#endif
  connect(ui_->slider_bufferminfill, SIGNAL(valueChanged(int)), SLOT(BufferMinFillChanged(int)));
  connect(ui_->stickslider_replaygainpreamp, SIGNAL(valueChanged(int)), SLOT(RgPreampChanged(int)));
  connect(ui_->checkbox_fadeout_stop, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->checkbox_fadeout_cross, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->checkbox_fadeout_auto, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));
  connect(ui_->checkbox_volume_control, SIGNAL(toggled(bool)), SLOT(FadingOptionsChanged()));

}

bool BackendSettingsPage::EngineInitialised() {

  if (!engine() || engine()->type() == Engine::None) {
    errordialog_.ShowMessage("Engine is not initialized! Please restart.");
    return false;
  }
  return true;

}

void BackendSettingsPage::Load_Engine(Engine::EngineType enginetype) {

  if (!EngineInitialised()) return;

  QString output = output_current_;
  QVariant device = device_current_;

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();

  ui_->combobox_output->setEnabled(false);
  ui_->combobox_device->setEnabled(false);

  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->setText("");

  ui_->groupbox_replaygain->setEnabled(false);

  if (engine()->type() != enginetype) {
    qLog(Debug) << "Switching engine.";
    Engine::EngineType new_enginetype = dialog()->app()->player()->CreateEngine(enginetype);
    dialog()->app()->player()->Init();
    if (new_enginetype != enginetype) {
      ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(engine()->type()));
    }
  }

  engineloaded_ = true;

  Load_Output(output, device);

}

void BackendSettingsPage::Load_Output(QString output, QVariant device) {

  if (!EngineInitialised()) return;

  if (output.isEmpty()) output = engine()->DefaultOutput();

  ui_->combobox_output->clear();
  int i = 0;
  for (const EngineBase::OutputDetails &o : engine()->GetOutputsList()) {
    i++;
    ui_->combobox_output->addItem(IconLoader::Load(o.iconname), o.description, QVariant::fromValue(o));
  }
  if (i > 1) ui_->combobox_output->setEnabled(true);

  bool found(false);
  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (o.name == output) {
      ui_->combobox_output->setCurrentIndex(i);
      found = true;
      break;
    }
  }
  if (!found) { // Output is invalid for this engine, reset to default output.
    output = engine()->DefaultOutput();
    device = (engine()->CustomDeviceSupport(output) ? QString() : QVariant());
    for (int i = 0; i < ui_->combobox_output->count(); ++i) {
      EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
      if (o.name == output) {
        ui_->combobox_output->setCurrentIndex(i);
        break;
      }
    }
  }

  if (engine()->type() == Engine::GStreamer) {
    ui_->groupbox_buffer->setEnabled(true);
    ui_->groupbox_replaygain->setEnabled(true);
  }
  else {
    ui_->groupbox_buffer->setEnabled(false);
    ui_->groupbox_replaygain->setEnabled(false);
  }

  if (ui_->combobox_output->count() >= 1) Load_Device(output, device);

}

void BackendSettingsPage::Load_Device(QString output, QVariant device) {

  if (!EngineInitialised()) return;

  int devices = 0;
  DeviceFinder::Device df_device;

  ui_->combobox_device->clear();
  ui_->combobox_device->setEnabled(false);
  ui_->lineedit_device->setText("");

#ifdef Q_OS_WIN
  if (engine()->type() != Engine::GStreamer)
#endif
    ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Automatically select", QVariant());

  for (DeviceFinder *f : dialog()->app()->enginedevice()->device_finders_) {
    if (!f->outputs().contains(output)) continue;
    for (const DeviceFinder::Device &d : f->ListDevices()) {
      devices++;
      ui_->combobox_device->addItem(IconLoader::Load(d.iconname), d.description, d.value);
      if (d.value == device) { df_device = d; }
    }
  }

  if (engine()->CustomDeviceSupport(output)) {
    ui_->combobox_device->addItem(IconLoader::Load("soundcard"), "Custom", QVariant());
    ui_->lineedit_device->setEnabled(true);
  }
  else {
    ui_->lineedit_device->setEnabled(false);
  }

#ifdef HAVE_ALSA
  if (engine()->ALSADeviceSupport(output)) {
    ui_->radiobutton_alsa_hw->setEnabled(true);
    ui_->radiobutton_alsa_plughw->setEnabled(true);
    if (device.toString().contains(QRegExp("^plughw:.*"))) {
      ui_->radiobutton_alsa_hw->setChecked(false);
      ui_->radiobutton_alsa_plughw->setChecked(true);
      SwitchALSADevices(alsa_plugin::alsa_plughw);
    }
    else {
      ui_->radiobutton_alsa_plughw->setChecked(false);
      ui_->radiobutton_alsa_hw->setChecked(true);
      SwitchALSADevices(alsa_plugin::alsa_hw);
    }
  }
  else {
    ui_->radiobutton_alsa_hw->setEnabled(false);
    ui_->radiobutton_alsa_hw->setChecked(false);
    ui_->radiobutton_alsa_plughw->setEnabled(false);
    ui_->radiobutton_alsa_plughw->setChecked(false);
  }
#endif

  bool found(false);
  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant d = ui_->combobox_device->itemData(i).value<QVariant>();
    if (df_device.value.isValid() && df_device.value == d) {
      ui_->combobox_device->setCurrentIndex(i);
      found = true;
      break;
    }
  }

  // This allows a custom ALSA device string ie: "hw:0,0" even if it is not listed.
  if (engine()->CustomDeviceSupport(output) && device.type() == QVariant::String && !device.toString().isEmpty()) {
    ui_->lineedit_device->setText(device.toString());
    if (!found) {
      for (int i = 0; i < ui_->combobox_device->count(); ++i) {
        if (ui_->combobox_device->itemText(i) == "Custom") {
          if (ui_->combobox_device->currentText() != "Custom") ui_->combobox_device->setCurrentIndex(i);
          break;
        }
      }
    }
  }

  if (devices > 0 || ui_->combobox_device->currentText() == "Custom") ui_->combobox_device->setEnabled(true);

  FadingOptionsChanged();

}

void BackendSettingsPage::Save() {

  if (!EngineInitialised()) return;

  QVariant enginetype_v = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex());
  Engine::EngineType enginetype = enginetype_v.value<Engine::EngineType>();
  QString output_name;
  QVariant device_value;

  if (ui_->combobox_output->currentText().isEmpty()) output_name = engine()->DefaultOutput();
  else {
    EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
    output_name = output.name;
  }

  if (ui_->combobox_device->currentText().isEmpty()) device_value = QVariant();
  else if (ui_->combobox_device->currentText() == "Custom") device_value = ui_->lineedit_device->text();
  else device_value = ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()).value<QVariant>();

  s_.setValue("engine", EngineName(enginetype));
  s_.setValue("output", output_name);
  s_.setValue("device", device_value);

  s_.setValue("bufferduration", ui_->spinbox_bufferduration->value());
  s_.setValue("bufferminfill", ui_->slider_bufferminfill->value());

  s_.setValue("rgenabled", ui_->checkbox_replaygain->isChecked());
  s_.setValue("rgmode", ui_->combobox_replaygainmode->currentIndex());
  s_.setValue("rgpreamp", float(ui_->stickslider_replaygainpreamp->value()) / 10 - 15);
  s_.setValue("rgcompression", ui_->checkbox_replaygaincompression->isChecked());

  s_.setValue("FadeoutEnabled", ui_->checkbox_fadeout_stop->isChecked());
  s_.setValue("CrossfadeEnabled", ui_->checkbox_fadeout_cross->isChecked());
  s_.setValue("AutoCrossfadeEnabled", ui_->checkbox_fadeout_auto->isChecked());
  s_.setValue("NoCrossfadeSameAlbum", ui_->checkbox_fadeout_samealbum->isChecked());
  s_.setValue("FadeoutPauseEnabled", ui_->checkbox_fadeout_pauseresume->isChecked());
  s_.setValue("FadeoutDuration", ui_->spinbox_fadeduration->value());
  s_.setValue("FadeoutPauseDuration", ui_->spinbox_fadeduration_pauseresume->value());

#ifdef HAVE_ALSA
  if (ui_->radiobutton_alsa_hw->isChecked()) s_.setValue("alsaplugin", static_cast<int>(alsa_plugin::alsa_hw));
  else if (ui_->radiobutton_alsa_plughw->isChecked()) s_.setValue("alsaplugin", static_cast<int>(alsa_plugin::alsa_plughw));
  else s_.remove("alsaplugin");
#endif

  s_.setValue("volume_control", ui_->checkbox_volume_control->isChecked());

}

void BackendSettingsPage::Cancel() {
  if (engine() && engine()->type() != enginetype_current_) { // Reset engine back to the original because user cancelled.
    dialog()->app()->player()->CreateEngine(enginetype_current_);
    dialog()->app()->player()->Init();
  }
}

void BackendSettingsPage::EngineChanged(int index) {

  if (!configloaded_ || !EngineInitialised()) return;

  QVariant v = ui_->combobox_engine->itemData(index);
  Engine::EngineType enginetype = v.value<Engine::EngineType>();

  if (engine()->type() == enginetype) return;

  if (engine()->state() != Engine::Empty) {
      errordialog_.ShowMessage("Can't switch engine while playing!");
      ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(engine()->type()));
      return;
  }

  engineloaded_ = false;
  Load_Engine(enginetype);

}

void BackendSettingsPage::OutputChanged(int index) {

  if (!configloaded_ || !EngineInitialised()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();
  Load_Device(output.name, QVariant());

}

void BackendSettingsPage::DeviceSelectionChanged(int index) {

  if (!configloaded_ || !EngineInitialised()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  QVariant device = ui_->combobox_device->itemData(index).value<QVariant>();

  if (engine()->CustomDeviceSupport(output.name)) {
    ui_->lineedit_device->setEnabled(true);
    if (ui_->combobox_device->currentText() != "Custom") {
      if (device.type() == QVariant::String) ui_->lineedit_device->setText(device.toString());
      else ui_->lineedit_device->setText("");
    }
  }
  else {
    ui_->lineedit_device->setEnabled(false);
    if (!ui_->lineedit_device->text().isEmpty()) ui_->lineedit_device->setText("");
  }

  FadingOptionsChanged();

}

void BackendSettingsPage::DeviceStringChanged() {

  if (!configloaded_ || !EngineInitialised()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  bool found(false);

#ifdef HAVE_ALSA
  if (engine()->ALSADeviceSupport(output.name)) {
    if (ui_->lineedit_device->text().contains(QRegExp("^hw:.*")) && !ui_->radiobutton_alsa_hw->isChecked()) {
      ui_->radiobutton_alsa_hw->setChecked(true);
      SwitchALSADevices(alsa_plugin::alsa_hw);
    }
    else if (ui_->lineedit_device->text().contains(QRegExp("^plughw:.*")) && !ui_->radiobutton_alsa_plughw->isChecked()) {
      ui_->radiobutton_alsa_plughw->setChecked(true);
      SwitchALSADevices(alsa_plugin::alsa_plughw);
    }
  }
#endif

  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant device = ui_->combobox_device->itemData(i).value<QVariant>();
    if (device.type() != QVariant::String) continue;
    if (device.toString().isEmpty()) continue;
    if (ui_->combobox_device->itemText(i) == "Custom") continue;
    if (device.toString() == ui_->lineedit_device->text()) {
      if (ui_->combobox_device->currentIndex() != i) ui_->combobox_device->setCurrentIndex(i);
      found = true;
    }
  }

  if (engine()->CustomDeviceSupport(output.name)) {
    ui_->lineedit_device->setEnabled(true);
    if ((!found) && (ui_->combobox_device->currentText() != "Custom")) {
      for (int i = 0; i < ui_->combobox_device->count(); ++i) {
        if (ui_->combobox_device->itemText(i) == "Custom") {
          ui_->combobox_device->setCurrentIndex(i);
          break;
        }
      }
    }
    if (ui_->combobox_device->currentText() == "Custom") {
      if ((ui_->lineedit_device->text().isEmpty()) && (ui_->combobox_device->count() > 0) && (ui_->combobox_device->currentIndex() != 0)) ui_->combobox_device->setCurrentIndex(0);
    }
  }
  else {
    ui_->lineedit_device->setEnabled(false);
    if (!ui_->lineedit_device->text().isEmpty()) ui_->lineedit_device->setText("");
    if ((!found) && (ui_->combobox_device->count() > 0) && (ui_->combobox_device->currentIndex() != 0)) ui_->combobox_device->setCurrentIndex(0);
  }

  FadingOptionsChanged();

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

#ifdef HAVE_ALSA
void BackendSettingsPage::SwitchALSADevices(alsa_plugin alsaplugin) {

  // All ALSA devices are listed twice, one for "hw" and one for "plughw"
  // Only show one of them by making the other ones invisible based on the alsa plugin radiobuttons
  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QListView *view = qobject_cast<QListView *>(ui_->combobox_device->view());
    if (!view) continue;
    if (alsaplugin == alsa_plugin::alsa_hw && ui_->combobox_device->itemData(i).toString().contains(QRegExp("^plughw:.*"))) {
      view->setRowHidden(i, true);
    }
    else if (alsaplugin == alsa_plugin::alsa_plughw && ui_->combobox_device->itemData(i).toString().contains(QRegExp("^hw:.*"))) {
      view->setRowHidden(i, true);
    }
    else {
      view->setRowHidden(i, false);
    }
  }

}

void BackendSettingsPage::radiobutton_alsa_hw_clicked(bool checked) {

  if (!configloaded_ || !EngineInitialised()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!engine()->ALSADeviceSupport(output.name)) return;

  if (ui_->lineedit_device->text().contains(QRegExp("^plughw:.*"))) {
    SwitchALSADevices(alsa_plugin::alsa_hw);
    QString device_new = ui_->lineedit_device->text().replace(QRegExp("^plughw:"), "hw:");
    bool found(false);
    for (int i = 0; i < ui_->combobox_device->count(); ++i) {
      QVariant device = ui_->combobox_device->itemData(i).value<QVariant>();
      if (device.type() != QVariant::String) continue;
      if (device.toString().isEmpty()) continue;
      if (device.toString() == device_new) {
        if (ui_->combobox_device->currentIndex() != i) ui_->combobox_device->setCurrentIndex(i);
        found = true;
      }
    }
    if (!found) ui_->lineedit_device->setText(device_new);
  }

}

void BackendSettingsPage::radiobutton_alsa_plughw_clicked(bool checked) {

  if (!configloaded_ || !EngineInitialised()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!engine()->ALSADeviceSupport(output.name)) return;

  if (ui_->lineedit_device->text().contains(QRegExp("^hw:.*"))) {
    SwitchALSADevices(alsa_plugin::alsa_plughw);
    QString device_new = ui_->lineedit_device->text().replace(QRegExp("^hw:"), "plughw:");
    bool found(false);
    for (int i = 0; i < ui_->combobox_device->count(); ++i) {
      QVariant device = ui_->combobox_device->itemData(i).value<QVariant>();
      if (device.type() != QVariant::String) continue;
      if (device.toString().isEmpty()) continue;
      if (device.toString() == device_new) {
        if (ui_->combobox_device->currentIndex() != i) ui_->combobox_device->setCurrentIndex(i);
        found = true;
      }
    }
    if (!found) ui_->lineedit_device->setText(device_new);
  }

}

#endif

void BackendSettingsPage::FadingOptionsChanged() {

  if (EngineInitialised()) {
    EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
    if (engine()->type() == Engine::GStreamer && !(engine()->ALSADeviceSupport(output.name) && !ui_->lineedit_device->text().isEmpty()) && ui_->checkbox_volume_control->isChecked()) {
      ui_->groupbox_fading->setDisabled(false);
    }
    else {
      ui_->groupbox_fading->setDisabled(true);
      ui_->checkbox_fadeout_stop->setChecked(false);
      ui_->checkbox_fadeout_cross->setChecked(false);
      ui_->checkbox_fadeout_auto->setChecked(false);
    }
  }

  ui_->widget_fading_options->setEnabled(ui_->checkbox_fadeout_stop->isChecked() || ui_->checkbox_fadeout_cross->isChecked() || ui_->checkbox_fadeout_auto->isChecked());

}
