/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QByteArray>
#include <QString>
#include <QSettings>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QEvent>

#include "settingsdialog.h"
#include "qobuzsettingspage.h"
#include "ui_qobuzsettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "widgets/loginstatewidget.h"
#include "qobuz/qobuzservice.h"
#include "qobuz/qobuzcredentialfetcher.h"
#include "constants/qobuzsettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace QobuzSettings;

QobuzSettingsPage::QobuzSettingsPage(SettingsDialog *dialog, const SharedPtr<QobuzService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::QobuzSettingsPage),
      service_(service),
      credential_fetcher_(nullptr) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"qobuz"_s, true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &QobuzSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &QobuzSettingsPage::LogoutClicked);
  QObject::connect(ui_->button_fetch_credentials, &QPushButton::clicked, this, &QobuzSettingsPage::FetchCredentialsClicked);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &QobuzSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &QobuzSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->format->addItem(u"MP3 320"_s, 5);
  ui_->format->addItem(u"FLAC Lossless"_s, 6);
  ui_->format->addItem(u"FLAC Hi-Res <= 96kHz"_s, 7);
  ui_->format->addItem(u"FLAC Hi-Res > 96kHz"_s, 27);

}

QobuzSettingsPage::~QobuzSettingsPage() { delete ui_; }

void QobuzSettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void QobuzSettingsPage::Load() {

  Settings s;
  if (!s.contains(kSettingsGroup)) set_changed();

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, false).toBool());
  ui_->app_id->setText(s.value(kAppId).toString());
  ui_->app_secret->setText(s.value(kAppSecret).toString());
  ui_->private_key->setText(s.value(kPrivateKey).toString());

  ComboBoxLoadFromSettings(s, ui_->format, QLatin1String(kFormat), 27);
  ui_->searchdelay->setValue(s.value(kSearchDelay, 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, 4).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, 10).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, 10).toInt());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, true).toBool());
  ui_->checkbox_remove_remastered->setChecked(s.value(kRemoveRemastered, true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_qobuzsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void QobuzSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  s.setValue(kAppId, ui_->app_id->text());
  s.setValue(kAppSecret, ui_->app_secret->text());
  s.setValue(kPrivateKey, ui_->private_key->text());

  s.setValue(kFormat, ui_->format->itemData(ui_->format->currentIndex()));
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.setValue(kRemoveRemastered, ui_->checkbox_remove_remastered->isChecked());
  s.endGroup();

}

void QobuzSettingsPage::LoginClicked() {

  if (ui_->app_id->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing app id. Please fetch credentials first."));
    return;
  }
  if (ui_->app_secret->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing app secret. Please fetch credentials first."));
    return;
  }
  if (ui_->private_key->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing private key. Please fetch credentials first."));
    return;
  }

  service_->Authenticate(ui_->app_id->text(), ui_->app_secret->text(), ui_->private_key->text());
  ui_->button_login->setEnabled(false);

}

bool QobuzSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void QobuzSettingsPage::LogoutClicked() {

  service_->ClearSession();
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::FetchCredentialsClicked() {

  ui_->button_fetch_credentials->setEnabled(false);
  ui_->button_fetch_credentials->setText(tr("Fetching..."));

  if (!credential_fetcher_) {
    credential_fetcher_ = new QobuzCredentialFetcher(service_->network(), this);
    QObject::connect(credential_fetcher_, &QobuzCredentialFetcher::CredentialsFetched, this, &QobuzSettingsPage::CredentialsFetched);
    QObject::connect(credential_fetcher_, &QobuzCredentialFetcher::CredentialsFetchError, this, &QobuzSettingsPage::CredentialsFetchError);
  }

  credential_fetcher_->FetchCredentials();

}

void QobuzSettingsPage::CredentialsFetched(const QString &app_id, const QString &app_secret, const QString &login_app_id, const QString &private_key) {

  Q_UNUSED(login_app_id)

  ui_->app_id->setText(app_id);
  ui_->app_secret->setText(app_secret);
  ui_->private_key->setText(private_key);

  ui_->button_fetch_credentials->setEnabled(true);
  ui_->button_fetch_credentials->setText(tr("Fetch Credentials"));

  QMessageBox::information(this, tr("Credentials fetched"), tr("Credentials have been successfully fetched. Click Login to authenticate via your browser."));

}

void QobuzSettingsPage::CredentialsFetchError(const QString &error) {

  ui_->button_fetch_credentials->setEnabled(true);
  ui_->button_fetch_credentials->setText(tr("Fetch Credentials"));

  QMessageBox::warning(this, tr("Credential fetch failed"), error);

}
