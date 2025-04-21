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
#include "constants/qobuzsettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace QobuzSettings;

QobuzSettingsPage::QobuzSettingsPage(SettingsDialog *dialog, const SharedPtr<QobuzService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::QobuzSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"qobuz"_s, true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &QobuzSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &QobuzSettingsPage::LogoutClicked);

  QObject::connect(this, &QobuzSettingsPage::Login, &*service_, &StreamingService::LoginWithCredentials);

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

  ui_->username->setText(s.value(kUsername).toString());
  QByteArray password = s.value(kPassword).toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));

  ComboBoxLoadFromSettings(s, ui_->format, QLatin1String(kFormat), 27);
  ui_->searchdelay->setValue(s.value(kSearchDelay, 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, 4).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, 10).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, 10).toInt());
  ui_->checkbox_base64_secret->setChecked(s.value(kBase64Secret, false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, true).toBool());

  s.endGroup();

  if (service_->authenticated()) ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);

  Init(ui_->layout_qobuzsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void QobuzSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("enabled", ui_->enable->isChecked());
  s.setValue(kAppId, ui_->app_id->text());
  s.setValue(kAppSecret, ui_->app_secret->text());

  s.setValue(kUsername, ui_->username->text());
  s.setValue(kPassword, QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));

  s.setValue(kFormat, ui_->format->itemData(ui_->format->currentIndex()));
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kBase64Secret, ui_->checkbox_base64_secret->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.endGroup();

}

void QobuzSettingsPage::LoginClicked() {

  if (ui_->app_id->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing app id."));
    return;
  }
  if (ui_->username->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing username."));
    return;
  }
  if (ui_->password->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing password."));
    return;
  }

  Q_EMIT Login(ui_->app_id->text(), ui_->username->text(), ui_->password->text());
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

}
