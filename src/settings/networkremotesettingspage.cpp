/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#include <QStyle>
#include "core/iconloader.h"
#include "core/logging.h"
#include "settings/settingsdialog.h"
#include "settings/networkremotesettingspage.h"
#include "ui_networkremotesettingspage.h"
#include "networkremote/networkremotesettings.h"
#include "networkremote/networkremote.h"

NetworkRemoteSettingsPage::NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui_NetworkRemoteSettingsPage),
      settings_(new NetworkRemoteSettings) {
  ui_->setupUi(this);
  const int iconSize = style()->pixelMetric(QStyle::PM_TabBarIconSize);
  setWindowIcon(IconLoader::Load(QStringLiteral("network-remote"), true, 0, iconSize));
  QObject::connect(ui_->useRemoteClient, &QAbstractButton::clicked, this, &NetworkRemoteSettingsPage::RemoteButtonClicked);
  QObject::connect(ui_->portSelected, &QAbstractSpinBox::editingFinished, this, &NetworkRemoteSettingsPage::PortChanged);
  QObject::connect(ui_->tokenValue, &QLineEdit::editingFinished, this, &NetworkRemoteSettingsPage::TokenChanged);
  QObject::connect(ui_->toggleTokenVisibility, &QAbstractButton::toggled, this, &NetworkRemoteSettingsPage::ToggleTokenVisibility);
  QObject::connect(ui_->playlistSizeValue, &QAbstractSpinBox::editingFinished, this, &NetworkRemoteSettingsPage::PlaylistSizeChanged);
}

NetworkRemoteSettingsPage::~NetworkRemoteSettingsPage() {
  delete ui_;
  delete settings_;
}

void NetworkRemoteSettingsPage::Load() {
  ui_->portSelected->setRange(8888, 65535);
  ui_->playlistSizeValue->setRange(NetworkRemoteSettings::kMinUpcomingRows, NetworkRemoteSettings::kMaxUpcomingRows);
  settings_->Load();

  ui_->useRemoteClient->setCheckable(true);
  ui_->useRemoteClient->setChecked(settings_->UseRemote());
  if (settings_->UseRemote()) {
    ui_->portSelected->setReadOnly(false);
    ui_->portSelected->setValue(settings_->GetPort());
    ui_->tokenValue->setEnabled(true);
    ui_->tokenValue->setText(settings_->GetToken());
    ui_->toggleTokenVisibility->setEnabled(true);
    ui_->playlistSizeValue->setEnabled(true);
    ui_->playlistSizeValue->setValue(settings_->GetPlaylistSize());
    DisplayIP();
  }
  else {
    ui_->portSelected->setReadOnly(true);
    ui_->portSelected->setValue(0);
    ui_->tokenValue->setEnabled(false);
    ui_->tokenValue->clear();
    ui_->toggleTokenVisibility->setEnabled(false);
    ui_->toggleTokenVisibility->setChecked(false);
    ui_->tokenValue->setEchoMode(QLineEdit::Password);
    ui_->playlistSizeValue->setEnabled(false);
    ui_->playlistSizeValue->setValue(0);
    ui_->ip_address->setText(QString());
  }

  qLog(Debug) << "SettingsPage Loaded QSettings ++++++++++++++++";
  Init(ui_->layout_networkremotesettingspage->parentWidget());
}

void NetworkRemoteSettingsPage::Save() {
  settings_->Save();
  qLog(Debug) << "Saving QSettings ++++++++++++++++";
}

void NetworkRemoteSettingsPage::RemoteButtonClicked() {
  settings_->SetUseRemote(ui_->useRemoteClient->isChecked());
  if (ui_->useRemoteClient->isChecked()) {
    ui_->portSelected->setReadOnly(false);
    ui_->portSelected->setValue(settings_->GetPort());
    ui_->tokenValue->setEnabled(true);
    ui_->tokenValue->setText(settings_->GetToken());
    ui_->toggleTokenVisibility->setEnabled(true);
    ui_->playlistSizeValue->setEnabled(true);
    ui_->playlistSizeValue->setValue(settings_->GetPlaylistSize());
    DisplayIP();
  }
  else {
    ui_->portSelected->setReadOnly(true);
    ui_->portSelected->setValue(0);
    ui_->tokenValue->setEnabled(false);
    ui_->tokenValue->clear();
    ui_->toggleTokenVisibility->setEnabled(false);
    ui_->toggleTokenVisibility->setChecked(false);
    ui_->tokenValue->setEchoMode(QLineEdit::Password);
    ui_->playlistSizeValue->setEnabled(false);
    ui_->playlistSizeValue->setValue(0);
    ui_->ip_address->setText(QString());
  }
}

void NetworkRemoteSettingsPage::PortChanged() {
  settings_->SetPort(ui_->portSelected->value());
}

void NetworkRemoteSettingsPage::DisplayIP() {
  ui_->ip_address->setText(NetworkRemote::DetectLocalIpAddress().toString());
}

void NetworkRemoteSettingsPage::TokenChanged() {
  settings_->SetToken(ui_->tokenValue->text());
}

void NetworkRemoteSettingsPage::ToggleTokenVisibility(bool visible) {
  ui_->tokenValue->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
}

void NetworkRemoteSettingsPage::PlaylistSizeChanged() {
  settings_->SetPlaylistSize(ui_->playlistSizeValue->value());
}