/*
 * Strawberry Music Player
 * Copyright 2013-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/application.h"
#include "core/iconloader.h"
#include "core/player.h"
#include "core/logging.h"
#include "core/settings.h"
#include "engine/enginebase.h"
#include "engine/enginedevice.h"
#include "engine/devicefinders.h"
#include "engine/devicefinder.h"
#include "widgets/lineedit.h"
#include "widgets/stickyslider.h"
#include "dialogs/errordialog.h"
#include "settings/settingspage.h"
#include "settingsdialog.h"
#include "ui_backendsettingspage.h"

const char *BackendSettingsPage::kSettingsGroup = "Backend";
const qint64 BackendSettingsPage::kDefaultBufferDuration = 4000;
const double BackendSettingsPage::kDefaultBufferLowWatermark = 0.33;
const double BackendSettingsPage::kDefaultBufferHighWatermark = 0.99;

namespace {
constexpr char kOutputAutomaticallySelect[] = "Automatically select";
constexpr char kOutputCustom[] = "Custom";
static const QRegularExpression kRegex_ALSA_HW(QStringLiteral("^hw:.*"));
static const QRegularExpression kRegex_ALSA_PlugHW(QStringLiteral("^plughw:.*"));
static const QRegularExpression kRegex_ALSA_PCM_Card(QStringLiteral("^.*:.*CARD=.*"));
static const QRegularExpression kRegex_ALSA_PCM_Dev(QStringLiteral("^.*:.*DEV=.*"));
}  // namespace

BackendSettingsPage::BackendSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_BackendSettingsPage),
      configloaded_(false),
      engineloaded_(false),
      enginetype_current_(EngineBase::Type::None) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("soundcard"), true, 0, 32));

  ui_->label_replaygainpreamp->setMinimumWidth(QFontMetrics(ui_->label_replaygainpreamp->font()).horizontalAdvance(QStringLiteral("-WW.W dB")));
  ui_->label_replaygainfallbackgain->setMinimumWidth(QFontMetrics(ui_->label_replaygainfallbackgain->font()).horizontalAdvance(QStringLiteral("-WW.W dB")));

  ui_->label_ebur128_target_level->setMinimumWidth(QFontMetrics(ui_->label_ebur128_target_level->font()).horizontalAdvance(QStringLiteral("-WW.W LUFS")));

  QObject::connect(ui_->combobox_engine, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &BackendSettingsPage::EngineChanged);
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
#ifdef HAVE_GSTREAMER
  QObject::connect(ui_->stickyslider_ebur128_target_level, &StickySlider::valueChanged, this, &BackendSettingsPage::EbuR128TargetLevelChanged);
#endif
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
  engineloaded_ = false;

  Settings s;
  s.beginGroup(kSettingsGroup);

  EngineBase::Type enginetype = EngineBase::TypeFromName(s.value("engine", EngineBase::Name(EngineBase::Type::None)).toString());
  if (enginetype == EngineBase::Type::None && engine()) enginetype = engine()->type();

  ui_->combobox_engine->clear();
#ifdef HAVE_GSTREAMER
  ui_->combobox_engine->addItem(IconLoader::Load(QStringLiteral("gstreamer")), EngineBase::Description(EngineBase::Type::GStreamer), static_cast<int>(EngineBase::Type::GStreamer));
#endif
#ifdef HAVE_VLC
  ui_->combobox_engine->addItem(IconLoader::Load(QStringLiteral("vlc")), EngineBase::Description(EngineBase::Type::VLC), static_cast<int>(EngineBase::Type::VLC));
#endif

  enginetype_current_ = enginetype;
  output_current_ = s.value("output", QString()).toString();
  device_current_ = s.value("device", QVariant());

  ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(static_cast<int>(enginetype)));

#ifdef HAVE_ALSA
  ui_->lineedit_device->show();
  ui_->widget_alsa_plugin->show();
  const ALSAPluginType alsa_plugin_type = static_cast<ALSAPluginType>(s.value("alsaplugin", static_cast<int>(ALSAPluginType::PCM)).toInt());
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
  ui_->checkbox_exclusive_mode->setChecked(s.value("exclusive_mode", false).toBool());
#endif

  if (EngineInitialized()) Load_Engine(enginetype);

  ui_->checkbox_volume_control->setChecked(s.value("volume_control", true).toBool());

  ui_->checkbox_channels->setChecked(s.value("channels_enabled", false).toBool());
  ui_->spinbox_channels->setValue(s.value("channels", 2).toInt());
  ui_->widget_channels->setEnabled(ui_->checkbox_channels->isChecked());

  ui_->checkbox_bs2b->setChecked(s.value("bs2b", false).toBool());

  ui_->checkbox_http2->setChecked(s.value("http2", false).toBool());
  ui_->checkbox_strict_ssl->setChecked(s.value("strict_ssl", false).toBool());

  ui_->spinbox_bufferduration->setValue(s.value("bufferduration", kDefaultBufferDuration).toInt());
  ui_->spinbox_low_watermark->setValue(s.value("bufferlowwatermark", kDefaultBufferLowWatermark).toDouble());
  ui_->spinbox_high_watermark->setValue(s.value("bufferhighwatermark", kDefaultBufferHighWatermark).toDouble());

  ui_->radiobutton_replaygain->setChecked(s.value("rgenabled", false).toBool());
  ui_->combobox_replaygainmode->setCurrentIndex(s.value("rgmode", 0).toInt());
  ui_->stickyslider_replaygainpreamp->setValue(static_cast<int>(s.value("rgpreamp", 0.0).toDouble() * 10 + 600));
  ui_->checkbox_replaygaincompression->setChecked(s.value("rgcompression", true).toBool());
  ui_->stickyslider_replaygainfallbackgain->setValue(static_cast<int>(s.value("rgfallbackgain", 0.0).toDouble() * 10 + 600));

#ifdef HAVE_GSTREAMER
  ui_->groupbox_ebur128->show();
#else
  ui_->groupbox_ebur128->hide();
#endif

  ui_->radiobutton_ebur128_loudness_normalization->setChecked(s.value("ebur128_loudness_normalization", false).toBool());
  ui_->stickyslider_ebur128_target_level->setValue(static_cast<int>(s.value("ebur128_target_level_lufs", -23.0).toDouble() * 10));

#ifdef HAVE_ALSA
  bool fade_default = false;
#else
  bool fade_default = true;
#endif

  ui_->checkbox_fadeout_stop->setChecked(s.value("FadeoutEnabled", fade_default).toBool());
  ui_->checkbox_fadeout_cross->setChecked(s.value("CrossfadeEnabled", fade_default).toBool());
  ui_->checkbox_fadeout_auto->setChecked(s.value("AutoCrossfadeEnabled", false).toBool());
  ui_->checkbox_fadeout_samealbum->setChecked(s.value("NoCrossfadeSameAlbum", true).toBool());
  ui_->checkbox_fadeout_pauseresume->setChecked(s.value("FadeoutPauseEnabled", false).toBool());
  ui_->spinbox_fadeduration->setValue(s.value("FadeoutDuration", 2000).toInt());
  ui_->spinbox_fadeduration_pauseresume->setValue(s.value("FadeoutPauseDuration", 250).toInt());

  if (!EngineInitialized()) return;

  if (engine()->state() == EngineBase::State::Empty) {
    if (ui_->combobox_engine->count() > 1) ui_->combobox_engine->setEnabled(true);
    else ui_->combobox_engine->setEnabled(false);
  }
  else {
    ui_->combobox_engine->setEnabled(false);
  }

  configloaded_ = true;

  FadingOptionsChanged();
  RgPreampChanged(ui_->stickyslider_replaygainpreamp->value());
  RgFallbackGainChanged(ui_->stickyslider_replaygainfallbackgain->value());

#ifdef HAVE_GSTREAMER
  EbuR128TargetLevelChanged(ui_->stickyslider_ebur128_target_level->value());
#endif

  Init(ui_->layout_backendsettingspage->parentWidget());
  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

  // Check if engine, output or device is set to a different setting than the configured to force saving settings.

  enginetype = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex()).value<EngineBase::Type>();
  QString output_name;
  if (ui_->combobox_output->currentText().isEmpty()) {
    output_name = engine()->DefaultOutput();
  }
  else {
    EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
    output_name = output.name;
  }
  QVariant device_value;
  if (ui_->combobox_device->currentText().isEmpty()) device_value = QVariant();
  else if (ui_->combobox_device->currentText() == QLatin1String(kOutputCustom)) device_value = ui_->lineedit_device->text();
  else device_value = ui_->combobox_device->itemData(ui_->combobox_device->currentIndex()).value<QVariant>();

  if (enginetype_current_ != enginetype || output_name != output_current_ || device_value != device_current_) {
    set_changed();
  }

  s.endGroup();

}

bool BackendSettingsPage::EngineInitialized() {

  if (!engine() || engine()->type() == EngineBase::Type::None) {
    errordialog_.ShowMessage(QStringLiteral("Engine is not initialized! Please restart."));
    return false;
  }
  return true;

}

void BackendSettingsPage::Load_Engine(const EngineBase::Type enginetype) {

  if (!EngineInitialized()) return;

  QString output = output_current_;
  QVariant device = device_current_;

  ui_->combobox_output->clear();
  ui_->combobox_device->clear();

  ui_->combobox_output->setEnabled(false);
  ui_->combobox_device->setEnabled(false);

  ui_->lineedit_device->setEnabled(false);
  ui_->lineedit_device->clear();

  ui_->groupbox_replaygain->setEnabled(false);
  ui_->groupbox_ebur128->setEnabled(false);

  if (engine()->type() != enginetype) {
    qLog(Debug) << "Switching engine.";
    EngineBase::Type new_enginetype = dialog()->app()->player()->CreateEngine(enginetype);
    dialog()->app()->player()->Init();
    if (new_enginetype != enginetype) {
      ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(static_cast<int>(engine()->type())));
    }
    set_changed();
  }

  engineloaded_ = true;

  Load_Output(output, device);

}

void BackendSettingsPage::Load_Output(QString output, QVariant device) {

  if (!EngineInitialized()) return;

  if (output.isEmpty()) output = engine()->DefaultOutput();

  ui_->combobox_output->clear();
  const EngineBase::OutputDetailsList outputs = engine()->GetOutputsList();
  for (const EngineBase::OutputDetails &o : outputs) {
    ui_->combobox_output->addItem(IconLoader::Load(o.iconname), o.description, QVariant::fromValue(o));
  }
  if (ui_->combobox_output->count() > 1) ui_->combobox_output->setEnabled(true);

  bool found(false);
  for (int i = 0; i < ui_->combobox_output->count(); ++i) {
    EngineBase::OutputDetails o = ui_->combobox_output->itemData(i).value<EngineBase::OutputDetails>();
    if (o.name == output) {
      ui_->combobox_output->setCurrentIndex(i);
      found = true;
      break;
    }
  }
  if (!found) {  // Output is invalid for this engine, reset to default output.
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

  if (engine()->type() == EngineBase::Type::GStreamer) {
    ui_->groupbox_buffer->setEnabled(true);
    ui_->groupbox_replaygain->setEnabled(true);
    ui_->groupbox_ebur128->setEnabled(true);
  }
  else {
    ui_->groupbox_buffer->setEnabled(false);
    ui_->groupbox_replaygain->setEnabled(false);
    ui_->groupbox_ebur128->setEnabled(false);
  }

#ifdef Q_OS_WIN32
  ui_->widget_exclusive_mode->setEnabled(engine()->ExclusiveModeSupport(output));
#endif

  if (ui_->combobox_output->count() >= 1) Load_Device(output, device);

  FadingOptionsChanged();

}

void BackendSettingsPage::Load_Device(const QString &output, const QVariant &device) {

  if (!EngineInitialized()) return;

  int devices = 0;
  EngineDevice df_device;

  ui_->combobox_device->clear();
  ui_->lineedit_device->clear();

#ifdef Q_OS_WIN
  if (engine()->type() != EngineBase::Type::GStreamer)
#endif
    ui_->combobox_device->addItem(IconLoader::Load(QStringLiteral("soundcard")), QLatin1String(kOutputAutomaticallySelect), QVariant());

  const QList<DeviceFinder*> device_finders = dialog()->app()->device_finders()->ListFinders();
  for (DeviceFinder *f : device_finders) {
    if (!f->outputs().contains(output)) continue;
    const EngineDeviceList engine_devices = f->ListDevices();
    for (const EngineDevice &d : engine_devices) {
      devices++;
      ui_->combobox_device->addItem(IconLoader::Load(d.iconname), d.description, d.value);
      if (d.value == device) { df_device = d; }
    }
  }

  if (engine()->CustomDeviceSupport(output)) {
    ui_->combobox_device->addItem(IconLoader::Load(QStringLiteral("soundcard")), QLatin1String(kOutputCustom), QVariant());
    ui_->lineedit_device->setEnabled(true);
  }
  else {
    ui_->lineedit_device->setEnabled(false);
  }

#ifdef HAVE_ALSA
  if (engine()->ALSADeviceSupport(output)) {
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
  if (engine()->CustomDeviceSupport(output) && device.metaType().id() == QMetaType::QString && !device.toString().isEmpty()) {
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

  ui_->combobox_device->setEnabled(devices > 0 || engine()->CustomDeviceSupport(output));

  FadingOptionsChanged();

}

void BackendSettingsPage::Save() {

  if (!EngineInitialized()) return;

  QVariant enginetype_v = ui_->combobox_engine->itemData(ui_->combobox_engine->currentIndex());
  EngineBase::Type enginetype = enginetype_v.value<EngineBase::Type>();
  QString output_name;
  QVariant device_value;

  if (ui_->combobox_output->currentText().isEmpty()) {
    output_name = engine()->DefaultOutput();
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

  s.setValue("engine", EngineBase::Name(enginetype));
  s.setValue("output", output_name);
  s.setValue("device", device_value);

#ifdef HAVE_ALSA
  if (ui_->radiobutton_alsa_hw->isChecked()) s.setValue("alsaplugin", static_cast<int>(ALSAPluginType::HW));
  else if (ui_->radiobutton_alsa_plughw->isChecked()) s.setValue("alsaplugin", static_cast<int>(ALSAPluginType::PlugHW));
  else if (ui_->radiobutton_alsa_pcm->isChecked()) s.setValue("alsaplugin", static_cast<int>(ALSAPluginType::PCM));
  else s.remove("alsaplugin");
#endif

#ifdef Q_OS_WIN32
  s.setValue("exclusive_mode", ui_->checkbox_exclusive_mode->isChecked());
#endif

  s.setValue("volume_control", ui_->checkbox_volume_control->isChecked());

  s.setValue("channels_enabled", ui_->checkbox_channels->isChecked());
  s.setValue("channels", ui_->spinbox_channels->value());

  s.setValue("bs2b", ui_->checkbox_bs2b->isChecked());

  s.setValue("http2", ui_->checkbox_http2->isChecked());
  s.setValue("strict_ssl", ui_->checkbox_strict_ssl->isChecked());

  s.setValue("bufferduration", ui_->spinbox_bufferduration->value());
  s.setValue("bufferlowwatermark", ui_->spinbox_low_watermark->value());
  s.setValue("bufferhighwatermark", ui_->spinbox_high_watermark->value());

  s.setValue("rgenabled", ui_->radiobutton_replaygain->isChecked());
  s.setValue("rgmode", ui_->combobox_replaygainmode->currentIndex());
  s.setValue("rgpreamp", static_cast<double>(ui_->stickyslider_replaygainpreamp->value()) / 10 - 60);
  s.setValue("rgfallbackgain", static_cast<double>(ui_->stickyslider_replaygainfallbackgain->value()) / 10 - 60);
  s.setValue("rgcompression", ui_->checkbox_replaygaincompression->isChecked());

  s.setValue("ebur128_loudness_normalization", ui_->radiobutton_ebur128_loudness_normalization->isChecked());
  s.setValue("ebur128_target_level_lufs", static_cast<double>(ui_->stickyslider_ebur128_target_level->value()) / 10);

  s.setValue("FadeoutEnabled", ui_->checkbox_fadeout_stop->isChecked());
  s.setValue("CrossfadeEnabled", ui_->checkbox_fadeout_cross->isChecked());
  s.setValue("AutoCrossfadeEnabled", ui_->checkbox_fadeout_auto->isChecked());
  s.setValue("NoCrossfadeSameAlbum", ui_->checkbox_fadeout_samealbum->isChecked());
  s.setValue("FadeoutPauseEnabled", ui_->checkbox_fadeout_pauseresume->isChecked());
  s.setValue("FadeoutDuration", ui_->spinbox_fadeduration->value());
  s.setValue("FadeoutPauseDuration", ui_->spinbox_fadeduration_pauseresume->value());

  s.endGroup();

}

void BackendSettingsPage::Cancel() {

  if (engine() && engine()->type() != enginetype_current_) {  // Reset engine back to the original because user cancelled.
    dialog()->app()->player()->CreateEngine(enginetype_current_);
    dialog()->app()->player()->Init();
  }

}

void BackendSettingsPage::EngineChanged(const int index) {

  if (!configloaded_ || !EngineInitialized()) return;

  QVariant v = ui_->combobox_engine->itemData(index);
  EngineBase::Type enginetype = v.value<EngineBase::Type>();

  if (engine()->type() == enginetype) return;

  if (engine()->state() != EngineBase::State::Empty) {
    errordialog_.ShowMessage(QStringLiteral("Can't switch engine while playing!"));
    ui_->combobox_engine->setCurrentIndex(ui_->combobox_engine->findData(static_cast<int>(engine()->type())));
    return;
  }

  engineloaded_ = false;
  Load_Engine(enginetype);

}

void BackendSettingsPage::OutputChanged(const int index) {

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(index).value<EngineBase::OutputDetails>();

#ifdef Q_OS_WIN32
  ui_->widget_exclusive_mode->setEnabled(engine()->ExclusiveModeSupport(output.name));
#endif

  Load_Device(output.name, QVariant());

}

void BackendSettingsPage::DeviceSelectionChanged(int index) {

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  QVariant device = ui_->combobox_device->itemData(index).value<QVariant>();

  if (engine()->CustomDeviceSupport(output.name)) {
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

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  bool found = false;

#ifdef HAVE_ALSA
  if (engine()->ALSADeviceSupport(output.name)) {
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

  if (engine()->CustomDeviceSupport(output.name)) {
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

#ifdef HAVE_GSTREAMER
void BackendSettingsPage::EbuR128TargetLevelChanged(const int value) {

  double db = static_cast<double>(value) / 10;
  QString db_str = QString::asprintf("%+.1f LUFS", db);
  ui_->label_ebur128_target_level->setText(db_str);

}
#endif

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

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!engine()->ALSADeviceSupport(output.name)) return;

  SwitchALSADevices(ALSAPluginType::HW);

  QString device_new = ui_->lineedit_device->text();

  if (device_new.contains(kRegex_ALSA_PlugHW)) {
    device_new = device_new.replace(kRegex_ALSA_PlugHW, QStringLiteral("hw:"));
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

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!engine()->ALSADeviceSupport(output.name)) return;

  SwitchALSADevices(ALSAPluginType::PlugHW);

  QString device_new = ui_->lineedit_device->text();

  if (device_new.contains(kRegex_ALSA_HW)) {
    device_new = device_new.replace(kRegex_ALSA_HW, QStringLiteral("plughw:"));
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

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (!engine()->ALSADeviceSupport(output.name)) return;

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

  if (!configloaded_ || !EngineInitialized()) return;

  EngineBase::OutputDetails output = ui_->combobox_output->itemData(ui_->combobox_output->currentIndex()).value<EngineBase::OutputDetails>();
  if (engine()->type() == EngineBase::Type::GStreamer &&
      (!engine()->ALSADeviceSupport(output.name) || ui_->lineedit_device->text().isEmpty() || (!ui_->lineedit_device->text().contains(kRegex_ALSA_HW) && !ui_->lineedit_device->text().contains(kRegex_ALSA_PlugHW)))) {
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
