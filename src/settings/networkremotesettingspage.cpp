/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include "networkremotesettingspage.h"

#include <algorithm>

#include <QString>
#include <QUrl>
#include <QFile>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QDesktopServices>
#include <QSettings>
#include <QRandomGenerator>

#include "constants/networkremotesettingsconstants.h"
#include "constants/networkremoteconstants.h"
#include "core/iconloader.h"
#include "networkremote/networkremote.h"
#include "transcoder/transcoder.h"
#include "transcoder/transcoderoptionsdialog.h"
#include "settingsdialog.h"
#include "ui_networkremotesettingspage.h"

using namespace Qt::Literals::StringLiterals;
using namespace NetworkRemoteSettingsConstants;
using namespace NetworkRemoteConstants;

namespace {

static bool ComparePresetsByName(const TranscoderPreset &left, const TranscoderPreset &right) {
  return left.name_ < right.name_;
}

}  // namespace

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog *dialog)
    : SettingsPage(dialog),
      ui_(new Ui_NetworkRemoteSettingsPage) {

  ui_->setupUi(this);

  setWindowIcon(IconLoader::Load(u"ipodtouchicon"_s));

  QObject::connect(ui_->options, &QPushButton::clicked, this, &NetworkRemoteSettingsPage::Options);

  QList<TranscoderPreset> presets = Transcoder::GetAllPresets();
  std::sort(presets.begin(), presets.end(), ComparePresetsByName);
  for (const TranscoderPreset &preset : std::as_const(presets)) {
    ui_->format->addItem(QStringLiteral("%1 (.%2)").arg(preset.name_, preset.extension_), QVariant::fromValue(preset));
  }

}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage() { delete ui_; }

void NetworkRemoteSettingsPage::Load() {

  QSettings s;

  s.beginGroup(kSettingsGroup);

  ui_->enabled->setChecked(s.value(kEnabled).toBool());
  ui_->spinbox_port->setValue(s.value(kPort, kDefaultServerPort).toInt());
  ui_->checkbox_allow_public_access->setChecked(s.value(kAllowPublicAccess, false).toBool());

  ui_->checkbox_use_auth_code->setChecked(s.value(kUseAuthCode, false).toBool());
  ui_->spinbox_auth_code->setValue(s.value(kAuthCode, QRandomGenerator::global()->bounded(100000)).toInt());

  ui_->allow_downloads->setChecked(s.value("allow_downloads", false).toBool());
  ui_->convert_lossless->setChecked(s.value("convert_lossless", false).toBool());

  QString last_output_format = s.value("last_output_format", u"audio/x-vorbis"_s).toString();
  for (int i = 0; i < ui_->format->count(); ++i) {
    if (last_output_format == ui_->format->itemData(i).value<TranscoderPreset>().codec_mimetype_) {
      ui_->format->setCurrentIndex(i);
      break;
    }
  }

  ui_->files_root_folder->SetPath(s.value("files_root_folder").toString());
  ui_->files_music_extensions->setText(s.value("files_music_extensions", kDefaultMusicExtensionsAllowedRemotely).toStringList().join(u','));

  s.endGroup();

  // Get local IP addresses
  QString ip_addresses;
  QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
  for (const QHostAddress &address : addresses) {
    // TODO: Add IPv6 support to tinysvcmdns
    if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isInSubnet(QHostAddress::parseSubnet(u"127.0.0.1/8"_s))) {
      if (!ip_addresses.isEmpty()) {
        ip_addresses.append(u", "_s);
      }
      ip_addresses.append(address.toString());
    }
  }
  ui_->label_ip_address->setText(ip_addresses);

}

void NetworkRemoteSettingsPage::Save() {

  QSettings s;

  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enabled->isChecked());
  s.setValue(kPort, ui_->spinbox_port->value());
  s.setValue(kAllowPublicAccess, ui_->checkbox_allow_public_access->isChecked());
  s.setValue(kUseAuthCode, ui_->checkbox_use_auth_code->isChecked());
  s.setValue(kAuthCode, ui_->spinbox_auth_code->value());

  TranscoderPreset preset = ui_->format->itemData(ui_->format->currentIndex()).value<TranscoderPreset>();
  s.setValue("last_output_format", preset.codec_mimetype_);

  s.setValue(kFilesRootFolder, ui_->files_root_folder->Path());

  QStringList files_music_extensions;
  for (const QString &extension : ui_->files_music_extensions->text().split(u',')) {
    QString ext = extension.trimmed();
    if (ext.size() > 0 && ext.size() < 8)  // no empty string, less than 8 char
      files_music_extensions << ext;
  }
  s.setValue("files_music_extensions", files_music_extensions);

  s.endGroup();

}

void NetworkRemoteSettingsPage::Options() {

  TranscoderPreset preset = ui_->format->itemData(ui_->format->currentIndex()).value<TranscoderPreset>();

  TranscoderOptionsDialog dialog(preset.filetype_, this);
  dialog.set_settings_postfix(QLatin1String(kTranscoderSettingPostfix));
  dialog.exec();

}
