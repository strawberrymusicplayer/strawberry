/*
 * Strawberry Music Player
 * Copyright 2019-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QLabel>
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
#include "qobuz/qobuzapicredentialfetcher.h"
#include "constants/qobuzsettings.h"
#ifdef Q_OS_ANDROID
#include "core/browserauthentication_android.h"
#include "widgets/actiondialog.h"
#endif

using namespace Qt::Literals::StringLiterals;
using namespace QobuzSettings;

QobuzSettingsPage::QobuzSettingsPage(SettingsDialog *dialog, const SharedPtr<QobuzService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::QobuzSettingsPage),
      service_(service),
      api_credential_fetcher_(nullptr) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"qobuz"_s, true, 0, 32));

#if defined(Q_OS_ANDROID) && !defined(HAVE_ANDROID_QOBUZ_BROWSER_AUTH)
  ui_->button_login->hide();
  ui_->button_fetch_api_credentials->hide();
  auto *authentication_notice = new QLabel(tr("Browser authentication is currently unavailable on Android."), this);
  authentication_notice->setWordWrap(true);
  ui_->layout_qobuzsettingspage->insertWidget(1, authentication_notice);
#elif defined(Q_OS_ANDROID)
  ui_->app_secret->setEchoMode(QLineEdit::Password);
  ui_->private_key->setEchoMode(QLineEdit::Password);
  auto *authentication_notice = new QLabel(this);
  authentication_notice->setWordWrap(true);
  authentication_notice->setText(AndroidBrowserAuthentication::WasInterrupted(QStringLiteral("Qobuz"))
      ? tr("The previous browser login was interrupted. Please try again.")
      : tr("Finish browser login within two minutes. Return here to cancel or retry."));
  ui_->layout_qobuzsettingspage->insertWidget(1, authentication_notice);
#endif

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &QobuzSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &QobuzSettingsPage::LogoutClicked);
  QObject::connect(ui_->button_fetch_api_credentials, &QPushButton::clicked, this, &QobuzSettingsPage::FetchApiCredentialsClicked);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &QobuzSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &QobuzSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->format->addItem(u"MP3 320"_s, 5);
  ui_->format->addItem(u"FLAC Lossless"_s, 6);
  ui_->format->addItem(u"FLAC Hi-Res <= 96kHz"_s, 7);
  ui_->format->addItem(u"FLAC Hi-Res > 96kHz"_s, 27);

#ifdef Q_OS_ANDROID
  SetupAndroidAutoSave();
#endif
}

QobuzSettingsPage::~QobuzSettingsPage() { delete ui_; }

void QobuzSettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void QobuzSettingsPage::Load() {

#ifdef Q_OS_ANDROID
  android_settings_loaded_ = false;
#endif
  Settings s;
  if (!s.contains(kSettingsGroup)) set_changed();

  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, kDefaultEnabled).toBool());
  ui_->app_id->setText(s.value(kAppId).toString());
  ui_->app_secret->setText(s.value(kAppSecret).toString());
  ui_->private_key->setText(s.value(kPrivateKey).toString());

  ComboBoxLoadFromSettings(s, ui_->format, QLatin1String(kFormat), kDefaultFormat);
  ui_->searchdelay->setValue(s.value(kSearchDelay, kDefaultSearchDelay).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, kDefaultArtistsSearchLimit).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, kDefaultAlbumsSearchLimit).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, kDefaultSongsSearchLimit).toInt());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, kDefaultDownloadAlbumCovers).toBool());
  ui_->checkbox_remove_remastered->setChecked(s.value(kRemoveRemastered, kDefaultRemoveRemastered).toBool());
  ui_->checkbox_show_search_album_quality->setChecked(s.value(kShowSearchAlbumQuality, kDefaultShowSearchAlbumQuality).toBool());
  ui_->checkbox_show_search_album_edition->setChecked(s.value(kShowSearchAlbumEdition, kDefaultShowSearchAlbumEdition).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_qobuzsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

#ifdef Q_OS_ANDROID
  android_settings_loaded_ = true;
#endif
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
  s.setValue(kShowSearchAlbumQuality, ui_->checkbox_show_search_album_quality->isChecked());
  s.setValue(kShowSearchAlbumEdition, ui_->checkbox_show_search_album_edition->isChecked());
  s.endGroup();

#ifdef Q_OS_ANDROID
  s.sync();
  android_save_succeeded_ = s.status() == QSettings::NoError;
  if (!android_save_succeeded_) {
    qLog(Error) << "Could not save Qobuz settings";
    ShowAuthenticationMessage(QMessageBox::Critical, tr("Settings could not be saved"), tr("Could not save Qobuz settings. Please check storage access and try again."));
  }
#endif
}

void QobuzSettingsPage::LoginClicked() {

#ifdef Q_OS_ANDROID
  if (service_->AndroidAuthenticationInProgress()) {
    service_->CancelAndroidAuthentication();
    return;
  }
#endif
  if (ui_->app_id->text().isEmpty()) {
    ShowAuthenticationMessage(QMessageBox::Critical, tr("Configuration incomplete"), tr("Missing app id. Please fetch API credentials first."));
    return;
  }
  if (ui_->app_secret->text().isEmpty()) {
    ShowAuthenticationMessage(QMessageBox::Critical, tr("Configuration incomplete"), tr("Missing app secret. Please fetch API credentials first."));
    return;
  }
  if (ui_->private_key->text().isEmpty()) {
    ShowAuthenticationMessage(QMessageBox::Critical, tr("Configuration incomplete"), tr("Missing private key. Please fetch API credentials first."));
    return;
  }

#ifdef Q_OS_ANDROID
  if (!SaveAndroidSettings()) return;
  ui_->button_login->setText(tr("Cancel login"));
#endif
  service_->Authenticate(ui_->app_id->text(), ui_->app_secret->text(), ui_->private_key->text());
#ifndef Q_OS_ANDROID
  ui_->button_login->setEnabled(false);
#endif

}

bool QobuzSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void QobuzSettingsPage::LogoutClicked() {

  service_->ClearSession();
#ifdef Q_OS_ANDROID
  ui_->button_login->setText(tr("Login"));
#endif
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginSuccess() {

#ifdef Q_OS_ANDROID
  ui_->button_login->setText(tr("Login"));
  ui_->button_login->setEnabled(true);
#endif
  if (!IsActive()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::LoginFailure(const QString &failure_reason) {

#ifdef Q_OS_ANDROID
  ui_->button_login->setText(tr("Login"));
  ui_->button_login->setEnabled(true);
#endif
  if (!IsActive()) return;
  ShowAuthenticationMessage(QMessageBox::Warning, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}

void QobuzSettingsPage::FetchApiCredentialsClicked() {

  ui_->button_fetch_api_credentials->setEnabled(false);
  ui_->button_fetch_api_credentials->setText(tr("Fetching..."));

  if (!api_credential_fetcher_) {
    api_credential_fetcher_ = new QobuzApiCredentialFetcher(service_->network(), this);
    QObject::connect(api_credential_fetcher_, &QobuzApiCredentialFetcher::ApiCredentialsFetched, this, &QobuzSettingsPage::ApiCredentialsFetched);
    QObject::connect(api_credential_fetcher_, &QobuzApiCredentialFetcher::ApiCredentialsFetchError, this, &QobuzSettingsPage::ApiCredentialsFetchError);
  }

  api_credential_fetcher_->FetchApiCredentials();

}

void QobuzSettingsPage::ApiCredentialsFetched(const QString &app_id, const QString &app_secret, const QString &login_app_id, const QString &private_key) {

  Q_UNUSED(login_app_id)

  ui_->app_id->setText(app_id);
  ui_->app_secret->setText(app_secret);
  ui_->private_key->setText(private_key);

#ifdef Q_OS_ANDROID
  if (!SaveAndroidSettings()) {
    ui_->button_fetch_api_credentials->setEnabled(true);
    ui_->button_fetch_api_credentials->setText(tr("Fetch API Credentials"));
    return;
  }
#endif
  ui_->button_fetch_api_credentials->setEnabled(true);
  ui_->button_fetch_api_credentials->setText(tr("Fetch API Credentials"));

#ifdef Q_OS_ANDROID
  if (!IsActive()) return;
#endif
  ShowAuthenticationMessage(QMessageBox::Information, tr("API credentials fetched"), tr("API credentials have been successfully fetched. Click Login to authenticate via your browser."));

}

void QobuzSettingsPage::ApiCredentialsFetchError(const QString &error) {

  ui_->button_fetch_api_credentials->setEnabled(true);
  ui_->button_fetch_api_credentials->setText(tr("Fetch API Credentials"));

#ifdef Q_OS_ANDROID
  if (!IsActive()) return;
#endif
  ShowAuthenticationMessage(QMessageBox::Warning, tr("Credential fetch failed"), error);

}

void QobuzSettingsPage::ShowAuthenticationMessage(QMessageBox::Icon icon, const QString &title, const QString &message) {

#ifdef Q_OS_ANDROID
  Q_UNUSED(icon)
  auto *notification = new ActionDialog(dialog()->parentWidget(), title, message, ActionDialog::TitlePosition::Top);
  notification->AddCancelButton(tr("Close"));
  notification->Open();
#else
  QMessageBox notification(icon, title, message, QMessageBox::Ok, this);
  notification.exec();
#endif

}
