/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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
#include <QString>
#include <QUrl>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QMessageBox>
#include <QEvent>
#include <QShowEvent>

#include "settingsdialog.h"
#include "plexsettingspage.h"
#include "ui_plexsettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "plex/plexservice.h"
#include "widgets/loginstatewidget.h"
#include "constants/plexsettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace PlexSettings;

PlexSettingsPage::PlexSettingsPage(SettingsDialog *dialog, const SharedPtr<PlexService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::PlexSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"plex"_s, true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &PlexSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &PlexSettingsPage::LogoutClicked);
  QObject::connect(ui_->button_refresh_servers, &QPushButton::clicked, this, &PlexSettingsPage::RefreshServersClicked);
  QObject::connect(ui_->button_test, &QPushButton::clicked, this, &PlexSettingsPage::TestClicked);
  QObject::connect(ui_->button_deletesongs, &QPushButton::clicked, &*service_, &PlexService::DeleteSongs);

  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &PlexSettingsPage::LoginSuccess);
  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &PlexSettingsPage::LoginFailure);
  QObject::connect(&*service_, &PlexService::ServersFound, this, &PlexSettingsPage::ServersFound);
  QObject::connect(&*service_, &StreamingService::TestSuccess, this, &PlexSettingsPage::TestSuccess);
  QObject::connect(&*service_, &StreamingService::TestFailure, this, &PlexSettingsPage::TestFailure);

  dialog->installEventFilter(this);

}

PlexSettingsPage::~PlexSettingsPage() { delete ui_; }

void PlexSettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void PlexSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, kDefaultEnabled).toBool());
  const QUrl server_url = s.value(kServerUrl).toUrl();
  const QString server_name = s.value(kServerName).toString();
  if (!server_url.isEmpty()) {
    int idx = ui_->server_url->findData(server_url);
    if (idx == -1) {
      ui_->server_url->addItem(server_name.isEmpty() ? server_url.toString() : server_name, server_url);
      idx = ui_->server_url->findData(server_url);
    }
    ui_->server_url->setCurrentIndex(idx);
  }
  ui_->checkbox_verify_certificate->setChecked(s.value(kVerifyCertificate, kDefaultVerifyCertificate).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, kDefaultDownloadAlbumCovers).toBool());
  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_plexsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

QUrl PlexSettingsPage::ServerUrlFromUi() const {

  QUrl server_url;
  const int idx = ui_->server_url->currentIndex();
  if (idx != -1 && ui_->server_url->itemText(idx) == ui_->server_url->currentText()) {
    server_url = ui_->server_url->itemData(idx).toUrl();
  }
  if (server_url.isEmpty()) {
    server_url = QUrl(ui_->server_url->currentText());
  }

  return server_url;

}

QString PlexSettingsPage::ServerTokenForUrl(const QUrl &url) const {

  for (const PlexService::Server &server : servers_) {
    if (server.url == url) {
      return server.owned ? QString() : server.access_token;
    }
  }

  return service_->authenticated() && url == service_->server_url() ? service_->server_token() : QString();

}

void PlexSettingsPage::Save() {

  const QUrl server_url = ServerUrlFromUi();

  QString server_name;
  const int idx = ui_->server_url->currentIndex();
  if (idx != -1 && ui_->server_url->itemData(idx).toUrl() == server_url) {
    server_name = ui_->server_url->itemText(idx);
  }

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  s.setValue(kServerUrl, server_url);
  s.setValue(kServerName, server_name);
  if (!servers_.isEmpty()) {
    s.setValue(kServerToken, ServerTokenForUrl(server_url).toUtf8().toBase64());
  }
  s.setValue(kVerifyCertificate, ui_->checkbox_verify_certificate->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

}

void PlexSettingsPage::LoginClicked() {

  service_->Authenticate();
  ui_->button_login->setEnabled(false);

}

void PlexSettingsPage::LogoutClicked() {

  service_->Deauthenticate();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void PlexSettingsPage::LoginSuccess() {

  ui_->button_login->setEnabled(true);
  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

}

void PlexSettingsPage::LoginFailure(const QString &failure_reason) {

  ui_->button_login->setEnabled(true);
  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);

}

void PlexSettingsPage::ServersFound(const PlexService::ServerList &servers) {

  servers_ = servers;

  const QUrl current = ServerUrlFromUi();

  ui_->server_url->clear();
  for (const PlexService::Server &server : servers) {
    ui_->server_url->addItem(server.name, server.url);
  }

  if (!current.isEmpty()) {
    const int idx = ui_->server_url->findData(current);
    if (idx != -1) {
      ui_->server_url->setCurrentIndex(idx);
    }
    else {
      ui_->server_url->setCurrentText(current.toString());
    }
  }
  else if (ui_->server_url->count() > 0) {
    ui_->server_url->setCurrentIndex(0);
  }

}

void PlexSettingsPage::RefreshServersClicked() {

  if (!service_->authenticated()) {
    QMessageBox::critical(this, tr("Not authenticated"), tr("Log in with Plex first."));
    return;
  }

  service_->GetServers();

}

void PlexSettingsPage::TestClicked() {

  if (!service_->authenticated()) {
    QMessageBox::critical(this, tr("Not authenticated"), tr("Log in with Plex first."));
    return;
  }

  QUrl server_url = ServerUrlFromUi();

  if (!server_url.isValid() || server_url.scheme().isEmpty() || server_url.host().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incorrect"), tr("Server URL is invalid."));
    return;
  }

  const QString server_token = ServerTokenForUrl(server_url);
  service_->SendPingWithSettings(server_url, server_token.isEmpty() ? service_->token() : server_token);
  ui_->button_test->setEnabled(false);

}

bool PlexSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
    ui_->button_test->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void PlexSettingsPage::TestSuccess() {

  ui_->button_test->setEnabled(true);
  if (!isVisible()) return;

  QMessageBox::information(this, tr("Test successful!"), tr("Test successful!"));

}

void PlexSettingsPage::TestFailure(const QString &failure_reason) {

  ui_->button_test->setEnabled(true);
  if (!isVisible()) return;

  QMessageBox::warning(this, tr("Test failed!"), failure_reason);

}
