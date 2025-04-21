/*
 * Strawberry Music Player
 * Copyright 2013-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QtGlobal>
#include <QWidget>
#include <QSettings>
#include <QMetaType>
#include <QVariant>
#include <QString>
#include <QRegularExpression>
#include <QFontMetrics>
#include <QAbstractItemView>
#include <QListView>
#include <QGroupBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QRadioButton>

#include "backendsettingspage.h"

#include "constants/backendsettings.h"
#include "core/iconloader.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/player.h"
#include "engine/enginebase.h"
#include "engine/enginedevice.h"
#include "engine/devicefinders.h"
#include "engine/devicefinder.h"
#include "widgets/lineedit.h"
#include "widgets/stickyslider.h"
#include "settings/settingspage.h"
#include "settingsdialog.h"
#include "ui_backendsettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace BackendSettings;

namespace {
constexpr char kOutputAutomaticallySelect[] = "Automatically select";
constexpr char kOutputCustom[] = "Custom";
static const QRegularExpression kRegex_ALSA_HW(u"^hw:.*"_s);
static const QRegularExpression kRegex_ALSA_PlugHW(u"^plughw:.*"_s);
#ifdef HAVE_ALSA
constexpr char kALSAHW[] = "hw:";
constexpr char kALSAPlugHW[] = "plughw:";
static const QRegularExpression kRegex_ALSA_PCM_Card(u"^.*:.*CARD=.*"_s);
static const QRegularExpression kRegex_ALSA_PCM_Dev(u"^.*:.*DEV=.*"_s);
#endif
}  // namespace

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif

BackendSettingsPage::BackendSettingsPage(SettingsDialog *dialog, const SharedPtr<Player> player, const SharedPtr<DeviceFinders> device_finders, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_BackendSettingsPage),
      player_(player),
      device_finders_(device_finders),
      configloaded_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"soundcard"_s, true, 0, 32));

  ui_->label_replaygainpreamp->setMinimumWidth(QFontMetrics(ui_->label_replaygainpreamp->font()).horizontalAdvance(u"-WW.W dB"_s));
  ui_->label_replaygainfallbackgain->setMinimumWidth(QFontMetrics(ui_->label_replaygainfallbackgain->font()).horizontalAdvance(u"-WW.W dB"_s));

  ui_->label_ebur128_target_level->setMinimumWidth(QFontMetrics(ui_->label_ebur128_target_level->font()).horizontalAdvance(u"-WW.W LUFS"_s));

  QObject::connect(ui_->combobox_output, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BackendSettingsPage::OutputChanged);
  QObject::connect(ui_->combobox_device, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BackendSettingsPage::DeviceSelectionChanged);
  QObject::connect(ui_->lineedit_device, &QLineEdit::textChanged, this, &BackendSettingsPage::DeviceStringChanged);
#ifdef HAVE_ALSA
  QObject::connect(ui_->radiobutton_alsa_hw, &QRadioButton::clicked, this, &BackendSettingsPage::radiobutton_alsa_hw_clicked);
  QObject::connect(ui_->radiobutton_alsa_plughw, &QRadioButton::clicked, this, &BackendSettingsPage::radiobutton_alsa_plughw_clicked);
  QObject::connect(ui_->radiobutton_alsa_pcm, &QRadioButton::clicked, this, &BackendSettingsPage::radiobutton_alsa_pcm_clicked);
#endif
  QObject::connect(ui_->stickyslider_replaygainpreamp, &StickySlider::valueChanged, this, &BackendSettingsPage::RgPreampChanged);
  QObject::connect(ui_->stickyslider_replaygainfallbackgain, &StickySlider::valueChanged, this, &BackendSettingsPage::RgFallbackGainChanged);
  QObject::connect(ui_->stickyslider_ebur128_target_level, &StickySlider::valueChanged, this, &BackendSettingsPage::EbuR128TargetLevelChanged);
  QObject::connect(ui_->checkbox_fadeout_stop, &QCheckBox::toggled, this, &BackendSettingsPage::FadingOptionsChanged);
  QObject::connect(ui_->checkbox_fadeout_cross, &QCheckBox::toggled, this, &BackendSettingsPage::FadingOptionsChanged);
  QObject::connect(ui_->checkbox_fadeout_auto, &QCheckBox::toggled, this, &BackendSettingsPage::FadingOptionsChanged);
  QObject::connect(ui_->checkbox_channels, &QCheckBox::toggled, ui_->widget_channels, &QSpinBox::setEnabled);
  QObject::connect(ui_->button_buffer_defaults, &QPushButton::clicked, this, &BackendSettingsPage::BufferDefaults);

#ifdef Q_OS_WIN32
  ui_->widget_exclusive_mode->show();
#else
  ui_->widget_exclusive_mode->hide();
#endif

}

BackendSettingsPage::~BackendSettingsPage() {

  delete ui_;

}

void BackendSettingsPage::Load() {

  configloaded_ = false;

  Settings s;
  s.beginGroup(kSettingsGroup);

  if (s.contains(kOutputU)) {
    output_current_ = s.value(kOutputU).toString();
  }
  else if (s.contains(kOutput)) {
    output_current_ = s.value(kOutput).toString();
  }

  if (s.contains(kDeviceU)) {
    device_current_ = s.value(kDeviceU);
  }
  else if (s.contains(kDevice)) {
    device_current_ = s.value(kDevice);
  }

#ifdef HAVE_ALSA
  ui_->lineedit_device->show();
  ui_->widget_alsa_plugin->show();
  const ALSAPluginType alsa_plugin_type = static_cast<ALSAPluginType>(s.value(kALSAPlugin, static_cast<int>(ALSAPluginType::PCM)).toInt());
  switch (alsa_plugin_type) {
    case ALSAPluginType::HW:
      ui_->radiobutton_alsa_hw->setChecked(true);
      break;
    case ALSAPluginType::PlugHW:
      ui_->radiobutton_alsa_plughw->setChecked(true);
      break;
    case ALSAPluginType::PCM:
      ui_->radiobutton_alsa_pcm->setChecked(true);
      break;
  }
#else
  ui_->lineedit_device->hide();
  ui_->widget_alsa_plugin->hide();
#endif

#ifdef Q_OS_WIN32
  ui_->checkbox_exclusive_mode->setChecked(s.value(kExclusiveMode, false).toBool());
#endif

  Load_Output(output_current_, device_current_);

  ui_->checkbox_volume_control->setChecked(s.value(kVolumeControl, true).toBool());

  ui_->checkbox_channels->setChecked(s.value(kChannelsEnabled, false).toBool());
  ui_->spinbox_channels->setValue(s.value(kChannels, 2).toInt());
  ui_->widget_channels->setEnabled(ui_->checkbox_channels->isChecked());

  ui_->checkbox_bs2b->setChecked(s.value(kBS2B, false).toBool());

  ui_->checkbox_playbin3->setChecked(s.value(kPlaybin3, true).toBool());

  ui_->checkbox_http2->setChecked(s.value(kHTTP2, false).toBool());
  ui_->checkbox_strict_ssl->setChecked(s.value(kStrictSSL, false).toBool());

  ui_->spinbox_bufferduration->setValue(s.value(kBufferDuration, kDefaultBufferDuration).toInt());
  ui_->spinbox_low_watermark->setValue(s.value(kBufferLowWatermark, kDefaultBufferLowWatermark).toDouble());
  ui_->spinbox_high_watermark->setValue(s.value(kBufferHighWatermark, kDefaultBufferHighWatermark).toDouble());

  ui_->radiobutton_replaygain->setChecked(s.value(kRgEnabled, false).toBool());
  ui_->combobox_replaygainmode->setCurrentIndex(s.value(kRgMode, 0).toInt());
  ui_->stickyslider_replaygainpreamp->setValue(static_cast<int>(s.value(kRgPreamp, 0.0).toDouble() * 10 + 600));
  ui_->checkbox_replaygaincompression->setChecked(s.value(kRgCompression, true).toBool());
  ui_->stickyslider_replaygainfallbackgain->setValue(static_cast<int>(s.value(kRgFallbackGain, 0.0).toDouble() * 10 + 600));

  ui_->radiobutton_ebur128_loudness_normalization->setChecked(s.value(kEBUR128LoudnessNormalization, false).toBool());
  ui_->stickyslider_ebur128_target_level->setValue(static_cast<int>(s.value(kEBUR128TargetLevelLUFS, -23.0).toDouble() * 10));

#ifdef HAVE_ALSA
  bool fade_default = false;
#else
  bool fade_default = true;
#endif

  ui_->checkbox_fadeout_stop->setChecked(s.value(kFadeoutEnabled, fade_default).toBool());
  ui_->checkbox_fadeout_cross->setChecked(s.value(kCrossfadeEnabled, fade_default).toBool());
  ui_->checkbox_fadeout_auto->setChecked(s.value(kAutoCrossfadeEnabled, false).toBool());
  ui_->checkbox_fadeout_samealbum->setChecked(s.value(kNoCrossfadeSameAlbum, true).toBool());
  ui_->checkbox_fadeout_pauseresume->setChecked(s.value(kFadeoutPauseEnabled, false).toBool());
  ui_->spinbox_fadeduration->setValue(s.value(kFadeoutDuration, 2000).toInt());
  ui_->spinbox_fadeduration_pauseresume->setValue(s.value(kFadeoutPauseDuration, 250).toInt());

  configloaded_ = true;

  FadingOptionsChanged();
  RgPreampChanged(ui_->stickyslider_replaygainpreamp->value());
  RgFallbackGainChanged(ui_->stickyslider_replaygainfallbackgain->value());
  EbuR128TargetLevelChanged(ui_->stickyslider_ebur128_target_level->value());

  Init(ui_->layout_backendsettingspage->parentWidget());
  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

  // Check if output or device is set to a different setting than the configured to force saving settings.

  QString output_name;
  if (ui_->combobox_output->currentText().isEmpty()) {
    output_name = player_->engine()->DefaultOutput();
  }
  else {
    EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
    output_name = output.name;
  }
  QVariant device_value;
  if (ui_->combobox_device->currentText().isEmpty()) device_value = QVariant();
  else if (ui_->combobox_device->currentText() == QLatin1String(kOutputCustom)) device_value = ui_->lineedit_device->text();
  else device_value = ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()).value<QVariant>();

  if (output_name != output_current_ || device_value != device_current_) {
    set_changed();
  }

  s.endGroup();

}

void BackendSettingsPage::Load_Output(QString output, QVariant device) {

  if (output.isEmpty()) output = player_->engine()->DefaultOutput();

  ui_->combobox_output->clear();
  const EngineBase::OutputDetailsList outputs = player_->engine()->GetOutputsList();
  for (const EngineBase::OutputDetails &o : outputs) {
    ui_->combobox_output->addItem(IconLoader::Load(o.iconname), o.description, QVariant::fromValue(o));
  }
  if (ui_->combobox_output->count() > 1) ui_->combobox_output->setEnabled(true);

  bool found = false;
  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (o.name == output) {
      ui_->combobox_output->setCurrentIndex(i);
      found = true;
      break;
    }
  }
  if (!found) {  // Output is invalid for this engine, reset to default output.
    output = player_->engine()->DefaultOutput();
    device = (player_->engine()->CustomDeviceSupport(output) ? QString() : QVariant());
    for (int i = 0; i < ui_->combobox_output->count(); ++i) {
      EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
      if (o.name == output) {
        ui_->combobox_output->setCurrentIndex(i);
        break;
      }
    }
  }

#ifdef Q_OS_WIN32
  ui_->widget_exclusive_mode->setEnabled(player_->engine()->ExclusiveModeSupport(output));
#endif

  if (ui_->combobox_output->count() >= 1) Load_Device(output, device);

  FadingOptionsChanged();

}

void BackendSettingsPage::Load_Device(const QString &output, const QVariant &device) {

  int devices = 0;
  EngineDevice df_device;

  ui_->combobox_device->clear();
  ui_->lineedit_device->clear();

#ifndef Q_OS_WIN32
  ui_->combobox_device->addItem(IconLoader::Load(u"soundcard"_s), QLatin1String(kOutputAutomaticallySelect), QVariant());
#endif

  const QList<DeviceFinder*> device_finders = device_finders_->ListFinders();
  for (DeviceFinder *f : device_finders) {
    if (!f->outputs().contains(output)) continue;
    const EngineDeviceList engine_devices = f->ListDevices();
    for (const EngineDevice &d : engine_devices) {
      devices++;
      ui_->combobox_device->addItem(IconLoader::Load(d.iconname), d.description, d.value);
      if (d.value == device) { df_device = d; }
    }
  }

  if (player_->engine()->CustomDeviceSupport(output)) {
    ui_->combobox_device->addItem(IconLoader::Load(u"soundcard"_s), QLatin1String(kOutputCustom), QVariant());
    ui_->lineedit_device->setEnabled(true);
  }
  else {
    ui_->lineedit_device->setEnabled(false);
  }

#ifdef HAVE_ALSA
  if (player_->engine()->ALSADeviceSupport(output)) {
    ui_->widget_alsa_plugin->setEnabled(true);
    ui_->radiobutton_alsa_hw->setEnabled(true);
    ui_->radiobutton_alsa_plughw->setEnabled(true);
    ui_->radiobutton_alsa_pcm->setEnabled(true);
    if (device.toString().contains(kRegex_ALSA_HW)) {
      ui_->radiobutton_alsa_hw->setChecked(true);
      SwitchALSADevices(ALSAPluginType::HW);
    }
    else if (device.toString().contains(kRegex_ALSA_PlugHW)) {
      ui_->radiobutton_alsa_plughw->setChecked(true);
      SwitchALSADevices(ALSAPluginType::PlugHW);
    }
    else if (device.toString().contains(kRegex_ALSA_PCM_Card) || device.toString().contains(kRegex_ALSA_PCM_Dev)) {
      ui_->radiobutton_alsa_pcm->setChecked(true);
      SwitchALSADevices(ALSAPluginType::PCM);
    }
    else {
      if (ui_->radiobutton_alsa_hw->isChecked()) {
        SwitchALSADevices(ALSAPluginType::HW);
      }
      else if (ui_->radiobutton_alsa_plughw->isChecked()) {
        SwitchALSADevices(ALSAPluginType::PlugHW);
      }
      else if (ui_->radiobutton_alsa_pcm->isChecked()) {
        SwitchALSADevices(ALSAPluginType::PCM);
      }
      else {
        ui_->radiobutton_alsa_hw->setChecked(true);
        SwitchALSADevices(ALSAPluginType::HW);
      }
    }
  }
  else {
    ui_->widget_alsa_plugin->setDisabled(true);
    ui_->radiobutton_alsa_hw->setChecked(false);
    ui_->radiobutton_alsa_plughw->setChecked(false);
    ui_->radiobutton_alsa_pcm->setChecked(false);
  }
#endif

  bool found = false;
  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant d = ui_->combobox_device->itemData(i).value<QVariant>();
    if (df_device.value.isValid() && df_device.value == d) {
      ui_->combobox_device->setCurrentIndex(i);
      found = true;
      break;
    }
  }

  // This allows a custom ALSA device string ie: "hw:0,0" even if it is not listed.
  if (player_->engine()->CustomDeviceSupport(output) && device.metaType().id() == QMetaType::QString && !device.toString().isEmpty()) {
    ui_->lineedit_device->setText(device.toString());
    if (!found) {
      for (int i = 0; i < ui_->combobox_device->count(); ++i) {
        if (ui_->combobox_device->itemText(i) == QLatin1String(kOutputCustom)) {
          if (ui_->combobox_device->currentText() != QLatin1String(kOutputCustom)) ui_->combobox_device->setCurrentIndex(i);
          break;
        }
      }
    }
  }

  ui_->combobox_device->setEnabled(devices > 0 || player_->engine()->CustomDeviceSupport(output));

  FadingOptionsChanged();

}

void BackendSettingsPage::Save() {

  QString output_name;
  QVariant device_value;

  if (ui_->combobox_output->currentText().isEmpty()) {
    output_name = player_->engine()->DefaultOutput();
  }
  else {
    EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
    output_name = output.name;
  }

  if (ui_->combobox_device->currentText().isEmpty()) device_value = QVariant();
  else if (ui_->combobox_device->currentText() == QLatin1String(kOutputCustom)) device_value = ui_->lineedit_device->text();
  else device_value = ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()).value<QVariant>();

  Settings s;
  s.beginGroup(kSettingsGroup);

  if (s.contains(kEngineU)) {
    s.remove(kEngineU);
  }
  if (s.contains(kEngine)) {
    s.remove(kEngine);
  }

  if (s.contains(kOutputU)) {
    s.remove(kOutputU);
  }
  if (s.contains(kDeviceU)) {
    s.remove(kDeviceU);
  }

  s.setValue(kOutput, output_name);
  s.setValue(kDevice, device_value);

#ifdef HAVE_ALSA
  if (ui_->radiobutton_alsa_hw->isChecked()) s.setValue(kALSAPlugin, static_cast<int>(ALSAPluginType::HW));
  else if (ui_->radiobutton_alsa_plughw->isChecked()) s.setValue(kALSAPlugin, static_cast<int>(ALSAPluginType::PlugHW));
  else if (ui_->radiobutton_alsa_pcm->isChecked()) s.setValue(kALSAPlugin, static_cast<int>(ALSAPluginType::PCM));
  else s.remove(kALSAPlugin);
#endif

#ifdef Q_OS_WIN32
  s.setValue(kExclusiveMode, ui_->checkbox_exclusive_mode->isChecked());
#endif

  s.setValue(kVolumeControl, ui_->checkbox_volume_control->isChecked());

  s.setValue(kChannelsEnabled, ui_->checkbox_channels->isChecked());
  s.setValue(kChannels, ui_->spinbox_channels->value());

  s.setValue(kBS2B, ui_->checkbox_bs2b->isChecked());

  s.setValue(kPlaybin3, ui_->checkbox_playbin3->isChecked());

  s.setValue(kHTTP2, ui_->checkbox_http2->isChecked());
  s.setValue(kStrictSSL, ui_->checkbox_strict_ssl->isChecked());

  s.setValue(kBufferDuration, ui_->spinbox_bufferduration->value());
  s.setValue(kBufferLowWatermark, ui_->spinbox_low_watermark->value());
  s.setValue(kBufferHighWatermark, ui_->spinbox_high_watermark->value());

  s.setValue(kRgEnabled, ui_->radiobutton_replaygain->isChecked());
  s.setValue(kRgMode, ui_->combobox_replaygainmode->currentIndex());
  s.setValue(kRgPreamp, static_cast<double>(ui_->stickyslider_replaygainpreamp->value()) / 10 - 60);
  s.setValue(kRgFallbackGain, static_cast<double>(ui_->stickyslider_replaygainfallbackgain->value()) / 10 - 60);
  s.setValue(kRgCompression, ui_->checkbox_replaygaincompression->isChecked());

  s.setValue(kEBUR128LoudnessNormalization, ui_->radiobutton_ebur128_loudness_normalization->isChecked());
  s.setValue(kEBUR128TargetLevelLUFS, static_cast<double>(ui_->stickyslider_ebur128_target_level->value()) / 10);

  s.setValue(kFadeoutEnabled, ui_->checkbox_fadeout_stop->isChecked());
  s.setValue(kCrossfadeEnabled, ui_->checkbox_fadeout_cross->isChecked());
  s.setValue(kAutoCrossfadeEnabled, ui_->checkbox_fadeout_auto->isChecked());
  s.setValue(kNoCrossfadeSameAlbum, ui_->checkbox_fadeout_samealbum->isChecked());
  s.setValue(kFadeoutPauseEnabled, ui_->checkbox_fadeout_pauseresume->isChecked());
  s.setValue(kFadeoutDuration, ui_->spinbox_fadeduration->value());
  s.setValue(kFadeoutPauseDuration, ui_->spinbox_fadeduration_pauseresume->value());

  s.endGroup();

}

void BackendSettingsPage::OutputChanged(const int index) {

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();

#ifdef Q_OS_WIN32
  ui_->widget_exclusive_mode->setEnabled(player_->engine()->ExclusiveModeSupport(output.name));
#endif

  Load_Device(output.name, QVariant());

}

void BackendSettingsPage::DeviceSelectionChanged(int index) {

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  QVariant device = ui_->combobox_device->itemData(index).value<QVariant>();

  if (player_->engine()->CustomDeviceSupport(output.name)) {
    ui_->lineedit_device->setEnabled(true);
    if (ui_->combobox_device->currentText() != QLatin1String(kOutputCustom)) {
      if (device.metaType().id() == QMetaType::QString)
        ui_->lineedit_device->setText(device.toString());
      else ui_->lineedit_device->clear();
    }
  }
  else {
    ui_->lineedit_device->setEnabled(false);
    if (!ui_->lineedit_device->text().isEmpty()) ui_->lineedit_device->clear();
  }

  FadingOptionsChanged();

}

void BackendSettingsPage::DeviceStringChanged() {

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  bool found = false;

#ifdef HAVE_ALSA
  if (player_->engine()->ALSADeviceSupport(output.name)) {
    if (ui_->lineedit_device->text().contains(kRegex_ALSA_HW) && !ui_->radiobutton_alsa_hw->isChecked()) {
      ui_->radiobutton_alsa_hw->setChecked(true);
      SwitchALSADevices(ALSAPluginType::HW);
    }
    else if (ui_->lineedit_device->text().contains(kRegex_ALSA_PlugHW) && !ui_->radiobutton_alsa_plughw->isChecked()) {
      ui_->radiobutton_alsa_plughw->setChecked(true);
      SwitchALSADevices(ALSAPluginType::PlugHW);
    }
    else if ((ui_->lineedit_device->text().contains(kRegex_ALSA_PCM_Card) || ui_->lineedit_device->text().contains(kRegex_ALSA_PCM_Dev)) && !ui_->radiobutton_alsa_pcm->isChecked()) {
      ui_->radiobutton_alsa_pcm->setChecked(true);
      SwitchALSADevices(ALSAPluginType::PCM);
    }
  }
#endif

  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QVariant device = ui_->combobox_device->itemData(i).value<QVariant>();
    if (device.metaType().id() != QMetaType::QString) continue;
    QString device_str = device.toString();
    if (device_str.isEmpty()) continue;
    if (ui_->combobox_device->itemText(i) == QLatin1String(kOutputCustom)) continue;
    if (device_str == ui_->lineedit_device->text()) {
      if (ui_->combobox_device->currentIndex() != i) ui_->combobox_device->setCurrentIndex(i);
      found = true;
    }
  }

  if (player_->engine()->CustomDeviceSupport(output.name)) {
    ui_->lineedit_device->setEnabled(true);
    if ((!found) && (ui_->combobox_device->currentText() != QLatin1String(kOutputCustom))) {
      for (int i = 0; i < ui_->combobox_device->count(); ++i) {
        if (ui_->combobox_device->itemText(i) == QLatin1String(kOutputCustom)) {
          ui_->combobox_device->setCurrentIndex(i);
          break;
        }
      }
    }
    if (ui_->combobox_device->currentText() == QLatin1String(kOutputCustom)) {
      if ((ui_->lineedit_device->text().isEmpty()) && (ui_->combobox_device->count() > 0) && (ui_->combobox_device->currentIndex() != 0)) ui_->combobox_device->setCurrentIndex(0);
    }
  }
  else {
    ui_->lineedit_device->setEnabled(false);
    if (!ui_->lineedit_device->text().isEmpty()) ui_->lineedit_device->clear();
    if ((!found) && (ui_->combobox_device->count() > 0) && (ui_->combobox_device->currentIndex() != 0)) ui_->combobox_device->setCurrentIndex(0);
  }

  FadingOptionsChanged();

}

void BackendSettingsPage::RgPreampChanged(const int value) {

  double db = static_cast<double>(value) / 10 - 60;
  QString db_str = QString::asprintf("%+.1f dB", db);
  ui_->label_replaygainpreamp->setText(db_str);

}

void BackendSettingsPage::RgFallbackGainChanged(const int value) {

  double db = static_cast<double>(value) / 10 - 60;
  QString db_str = QString::asprintf("%+.1f dB", db);
  ui_->label_replaygainfallbackgain->setText(db_str);

}

void BackendSettingsPage::EbuR128TargetLevelChanged(const int value) {

  double db = static_cast<double>(value) / 10;
  QString db_str = QString::asprintf("%+.1f LUFS", db);
  ui_->label_ebur128_target_level->setText(db_str);

}

#ifdef HAVE_ALSA
void BackendSettingsPage::SwitchALSADevices(const ALSAPluginType alsa_plugin_type) {

  // All ALSA devices are listed twice, one for "hw" and one for "plughw"
  // Only show one of them by making the other ones invisible based on the alsa plugin radiobuttons
  for (int i = 0; i < ui_->combobox_device->count(); ++i) {
    QListView *view = qobject_cast<QListView*>(ui_->combobox_device->view());
    if (!view) continue;
    if ((ui_->combobox_device->itemData(i).toString().contains(kRegex_ALSA_HW) && alsa_plugin_type != ALSAPluginType::HW)
        ||
        (ui_->combobox_device->itemData(i).toString().contains(kRegex_ALSA_PlugHW) && alsa_plugin_type != ALSAPluginType::PlugHW)
        ||
        ((ui_->combobox_device->itemData(i).toString().contains(kRegex_ALSA_PCM_Card) || ui_->combobox_device->itemData(i).toString().contains(kRegex_ALSA_PCM_Dev)) && alsa_plugin_type != ALSAPluginType::PCM)
    ) {
      view->setRowHidden(i, true);
    }
    else {
      view->setRowHidden(i, false);
    }
  }

}
#endif

void BackendSettingsPage::radiobutton_alsa_hw_clicked(const bool checked) {

  if (!checked) return;

#ifdef HAVE_ALSA

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!player_->engine()->ALSADeviceSupport(output.name)) return;

  SwitchALSADevices(ALSAPluginType::HW);

  QString device_new = ui_->lineedit_device->text();

  if (device_new.contains(kRegex_ALSA_PlugHW)) {
    device_new = device_new.replace(QLatin1String(kALSAPlugHW), QLatin1String(kALSAHW));
  }

  if (!device_new.contains(kRegex_ALSA_HW)) {
    device_new.clear();
  }

  SelectDevice(device_new);

#endif

}

void BackendSettingsPage::radiobutton_alsa_plughw_clicked(const bool checked) {

  if (!checked) return;

#ifdef HAVE_ALSA

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!player_->engine()->ALSADeviceSupport(output.name)) return;

  SwitchALSADevices(ALSAPluginType::PlugHW);

  QString device_new = ui_->lineedit_device->text();

  if (device_new.contains(kRegex_ALSA_HW)) {
    device_new = device_new.replace(QLatin1String(kALSAHW), QLatin1String(kALSAPlugHW));
  }

  if (!device_new.contains(kRegex_ALSA_PlugHW)) {
    device_new.clear();
  }

  SelectDevice(device_new);

#endif

}

void BackendSettingsPage::radiobutton_alsa_pcm_clicked(const bool checked) {

  if (!checked) return;

#ifdef HAVE_ALSA

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!player_->engine()->ALSADeviceSupport(output.name)) return;

  SwitchALSADevices(ALSAPluginType::PCM);

  QString device_new = ui_->lineedit_device->text();

  if (!device_new.contains(kRegex_ALSA_PCM_Card) && !device_new.contains(kRegex_ALSA_PCM_Dev)) {
    device_new.clear();
  }

  SelectDevice(device_new);

#endif

}

void BackendSettingsPage::SelectDevice(const QString &device_new) {

  if (device_new.isEmpty()) {
    for (int i = 0; i < ui_->combobox_device->count(); ++i) {
      if (ui_->combobox_device->itemText(i) == QLatin1String(kOutputAutomaticallySelect) && ui_->combobox_device->currentIndex() != i) {
        ui_->combobox_device->setCurrentIndex(i);
        break;
      }
    }
  }
  else {
    bool found = false;
    for (int i = 0; i < ui_->combobox_device->count(); ++i) {
      QListView *view = qobject_cast<QListView*>(ui_->combobox_device->view());
      if (view && view->isRowHidden(i)) continue;
      QVariant device = ui_->combobox_device->itemData(i).value<QVariant>();
      if (device.metaType().id() != QMetaType::QString) continue;
      QString device_str = device.toString();
      if (device_str.isEmpty()) continue;
      if (device_str == device_new) {
        if (ui_->combobox_device->currentIndex() != i) ui_->combobox_device->setCurrentIndex(i);
        found = true;
      }
    }
    if (!found) {
      ui_->lineedit_device->setText(device_new);
      for (int i = 0; i < ui_->combobox_device->count(); ++i) {
        if (ui_->combobox_device->itemText(i) == QLatin1String(kOutputCustom) && ui_->combobox_device->currentIndex() != i) {
          ui_->combobox_device->setCurrentIndex(i);
          break;
        }
      }
    }
  }

}

void BackendSettingsPage::FadingOptionsChanged() {

  if (!configloaded_) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!player_->engine()->ALSADeviceSupport(output.name) || ui_->lineedit_device->text().isEmpty() || (!ui_->lineedit_device->text().contains(kRegex_ALSA_HW) && !ui_->lineedit_device->text().contains(kRegex_ALSA_PlugHW))) {
    ui_->groupbox_fading->setEnabled(true);
  }
  else {
    ui_->groupbox_fading->setDisabled(true);
    ui_->checkbox_fadeout_stop->setChecked(false);
    ui_->checkbox_fadeout_cross->setChecked(false);
    ui_->checkbox_fadeout_auto->setChecked(false);
  }

  ui_->widget_fading_options->setEnabled(ui_->checkbox_fadeout_stop->isChecked() || ui_->checkbox_fadeout_cross->isChecked() || ui_->checkbox_fadeout_auto->isChecked());
  ui_->checkbox_fadeout_samealbum->setEnabled(ui_->checkbox_fadeout_auto->isChecked());

}


void BackendSettingsPage::BufferDefaults() {

  ui_->spinbox_bufferduration->setValue(kDefaultBufferDuration);
  ui_->spinbox_low_watermark->setValue(kDefaultBufferLowWatermark);
  ui_->spinbox_high_watermark->setValue(kDefaultBufferHighWatermark);

}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
