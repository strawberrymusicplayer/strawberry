/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <QObject>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <QEvent>
#include <QSignalBlocker>

#include "includes/shared_ptr.h"
#include "settingsdialog.h"
#include "jellyfinsettingspage.h"
#include "ui_jellyfinsettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "jellyfin/jellyfinservice.h"
#include "credentialsmanager/credentialsmanager.h"
#include "constants/jellyfinsettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace JellyfinSettings;

JellyfinSettingsPage::JellyfinSettingsPage(SettingsDialog *dialog, const SharedPtr<JellyfinService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::JellyfinSettingsPage),
      service_(service),
      test_pending_(false),
      password_save_failed_(false),
      password_requested_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"jellyfin"_s, true, 0, 32));

  QObject::connect(ui_->button_test, &QPushButton::clicked, this, &JellyfinSettingsPage::TestClicked);
  QObject::connect(ui_->enable, &QCheckBox::toggled, this, &JellyfinSettingsPage::EnableToggled);
  QObject::connect(this, &JellyfinSettingsPage::Test, &*service_, &JellyfinService::SendPingWithCredentials);

  // The results show a dialog, don't show it while the service is updating its login state.
  QObject::connect(&*service_, &JellyfinService::TestFailure, this, &JellyfinSettingsPage::TestFailure, Qt::QueuedConnection);
  QObject::connect(&*service_, &JellyfinService::TestSuccess, this, &JellyfinSettingsPage::TestSuccess, Qt::QueuedConnection);

  dialog->installEventFilter(this);

}

JellyfinSettingsPage::~JellyfinSettingsPage() { delete ui_; }

void JellyfinSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  {
    // The password is read below, not when the checkbox is toggled here.
    const QSignalBlocker blocker(ui_->enable);
    ui_->enable->setChecked(s.value(kEnabled, kDefaultEnabled).toBool());
  }
  ui_->server_url->setText(s.value(kUrl).toString());
  confirmed_server_url_ = s.value(kHttpUrlAccepted).toUrl();
  ui_->username->setText(s.value(kUsername).toString());

  if (read_password_reply_) {
    QObject::disconnect(&*read_password_reply_, nullptr, this, nullptr);
    read_password_reply_.reset();
  }

  loaded_password_.clear();
  password_requested_ = false;
  ui_->password->setText(loaded_password_);
  // Only read the password when Jellyfin is enabled and configured, since reading it can show a keyring password prompt, EnableToggled() reads it when Jellyfin is enabled later.
  if (ui_->enable->isChecked()) ReadPassword();

  ui_->checkbox_http2->setChecked(s.value(kHTTP2, kDefaultHTTP2).toBool());
  ui_->checkbox_verify_certificate->setChecked(s.value(kVerifyCertificate, kDefaultVerifyCertificate).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, kDefaultDownloadAlbumCovers).toBool());
  ui_->checkbox_server_scrobbling->setChecked(s.value(kServerSideScrobbling, kDefaultServerSideScrobbling).toBool());

  s.endGroup();

  Init(ui_->layout_jellyfinsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void JellyfinSettingsPage::ReadPassword() {

  if (password_requested_ || ui_->username->text().isEmpty()) return;
  password_requested_ = true;

  read_password_reply_ = service_->credentials_manager()->ReadPasswordAsync(QLatin1String(kCredentialsService));
  QObject::connect(&*read_password_reply_, &CredentialsReply::Finished, this, &JellyfinSettingsPage::ReadPasswordFinished);

}

void JellyfinSettingsPage::EnableToggled(const bool enabled) {

  if (enabled) ReadPassword();

}

void JellyfinSettingsPage::ReadPasswordFinished() {

  if (!read_password_reply_ || sender() != &*read_password_reply_) return;

  // Don't overwrite a password the user started typing while the password was being read.
  if (read_password_reply_->success() && ui_->password->text() == loaded_password_) {
    loaded_password_ = read_password_reply_->password();
    // Show the password that failed to be written, it's written again on the next save.
    ui_->password->setText(password_save_failed_ ? unsaved_password_ : loaded_password_);
  }

  read_password_reply_.reset();

}

void JellyfinSettingsPage::SavePasswordFinished() {

  if (!save_password_reply_ || sender() != &*save_password_reply_) return;

  if (save_password_reply_->success()) {
    password_save_failed_ = false;
    unsaved_password_.clear();
  }
  else {
    // Try to write the password again on the next save, also when it was removed and the password field is empty.
    password_save_failed_ = true;
    unsaved_password_ = saving_password_;
    QMessageBox::warning(this, tr("Failed to save password"), tr("Failed to save the Jellyfin password: %1").arg(save_password_reply_->error()));
  }

  save_password_reply_.reset();

}

bool JellyfinSettingsPage::IsValidServerUrl(const QUrl &server_url) {

  // Credentials in the URL are not supported, they would be sent to the server with every request and end up in logs.
  return server_url.isValid() && (server_url.scheme() == "https"_L1 || server_url.scheme() == "http"_L1) && !server_url.host().isEmpty() && server_url.userInfo().isEmpty();

}

bool JellyfinSettingsPage::ConfirmServerUrl(const QUrl &server_url) {

  // Jellyfin serves plain HTTP by default, so it's allowed, but the user has to accept sending the credentials unencrypted, once for each server URL.
  if (server_url.scheme() != "http"_L1 || server_url == confirmed_server_url_) return true;

  const QMessageBox::StandardButton button = QMessageBox::warning(this, tr("Unencrypted connection"), tr("The server URL uses HTTP, so the username, password and access token are sent unencrypted and can be read by others on the network. Use HTTPS if the server supports it.\n\nDo you want to continue?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (button != QMessageBox::Yes) return false;

  confirmed_server_url_ = server_url;
  return true;

}

void JellyfinSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  // Don't enable Jellyfin with an invalid server URL.
  const QUrl server_url(ui_->server_url->text());
  bool enabled = ui_->enable->isChecked();
  if (enabled && !IsValidServerUrl(server_url)) {
    enabled = false;
    ui_->enable->setChecked(false);
    QMessageBox::warning(this, tr("Configuration incorrect"), tr("Server URL is invalid, Jellyfin was not enabled."));
  }
  else if (enabled && !ConfirmServerUrl(server_url)) {
    enabled = false;
    ui_->enable->setChecked(false);
  }

  s.setValue(kEnabled, enabled);
  s.setValue(kUrl, server_url);
  // Only remember an HTTP server URL the user accepted, a declined or unconfirmed URL is confirmed when Jellyfin is enabled.
  if (server_url.scheme() == "http"_L1 && server_url == confirmed_server_url_) {
    s.setValue(kHttpUrlAccepted, server_url);
  }
  else {
    s.remove(kHttpUrlAccepted);
  }
  s.setValue(kUsername, ui_->username->text());

  // Requests are run in order, so the service reads the new password when it reloads the settings after this.
  // Write the password when it was changed, or write the password that failed to be written before again when it wasn't.
  QString password = ui_->password->text();
  const bool password_changed = password != loaded_password_;
  if (!password_changed && password_save_failed_) password = unsaved_password_;
  if (password_changed || password_save_failed_) {
    // A pending read would finish with the old password and overwrite the new one.
    if (read_password_reply_) {
      QObject::disconnect(&*read_password_reply_, nullptr, this, nullptr);
      read_password_reply_.reset();
    }
    if (save_password_reply_) {
      QObject::disconnect(&*save_password_reply_, nullptr, this, nullptr);
    }
    saving_password_ = password;
    save_password_reply_ = service_->credentials_manager()->SavePasswordAsync(QLatin1String(kCredentialsService), password);
    QObject::connect(&*save_password_reply_, &CredentialsReply::Finished, this, &JellyfinSettingsPage::SavePasswordFinished);
    loaded_password_ = password;
    service_->PasswordSaved();
  }

  s.setValue(kHTTP2, ui_->checkbox_http2->isChecked());
  s.setValue(kVerifyCertificate, ui_->checkbox_verify_certificate->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.setValue(kServerSideScrobbling, ui_->checkbox_server_scrobbling->isChecked());

  s.endGroup();

}

void JellyfinSettingsPage::TestClicked() {

  if (ui_->server_url->text().isEmpty() || ui_->username->text().isEmpty() || ui_->password->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing server url, username or password."));
    return;
  }

  const QUrl server_url(ui_->server_url->text());
  if (!IsValidServerUrl(server_url)) {
    QMessageBox::critical(this, tr("Configuration incorrect"), tr("Server URL is invalid."));
    return;
  }

  if (!ConfirmServerUrl(server_url)) return;

  test_pending_ = true;
  Q_EMIT Test(server_url, ui_->username->text(), ui_->password->text(), ui_->checkbox_http2->isChecked(), ui_->checkbox_verify_certificate->isChecked());
  ui_->button_test->setEnabled(false);

}

bool JellyfinSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter && !test_pending_) {
    ui_->button_test->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void JellyfinSettingsPage::TestSuccess() {

  test_pending_ = false;
  ui_->button_test->setEnabled(true);

  if (!isVisible()) return;
  QMessageBox::information(this, tr("Test successful!"), tr("Test successful!"));

}

void JellyfinSettingsPage::TestFailure(const QString &failure_reason) {

  test_pending_ = false;
  ui_->button_test->setEnabled(true);

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Test failed!"), failure_reason);

}
