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
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QSettings>
#include <QPushButton>
#include <QMessageBox>
#include <QEvent>

#include "settingsdialog.h"
#include "jellyfinsettingspage.h"
#include "ui_jellyfinsettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "jellyfin/jellyfinservice.h"
#include "constants/jellyfinsettings.h"

using namespace JellyfinSettings;

JellyfinSettingsPage::JellyfinSettingsPage(SettingsDialog *dialog, const SharedPtr<JellyfinService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::JellyfinSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(QStringLiteral("jellyfin"), true, 0, 32));

  QObject::connect(ui_->button_test, &QPushButton::clicked, this, &JellyfinSettingsPage::TestClicked);
  QObject::connect(this, &JellyfinSettingsPage::Test, &*service_, &JellyfinService::SendPingWithCredentials);

  QObject::connect(&*service_, &JellyfinService::TestFailure, this, &JellyfinSettingsPage::TestFailure);
  QObject::connect(&*service_, &JellyfinService::TestSuccess, this, &JellyfinSettingsPage::TestSuccess);

  dialog->installEventFilter(this);

  ui_->checkbox_http2->show();

}

JellyfinSettingsPage::~JellyfinSettingsPage() { delete ui_; }

void JellyfinSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  ui_->enable->setChecked(s.value(kEnabled, kDefaultEnabled).toBool());
  ui_->server_url->setText(s.value(kUrl).toString());
  ui_->username->setText(s.value(kUsername).toString());
  QByteArray password = s.value(kPassword).toByteArray();
  if (password.isEmpty()) ui_->password->clear();
  else ui_->password->setText(QString::fromUtf8(QByteArray::fromBase64(password)));
  ui_->checkbox_http2->setChecked(s.value(kHTTP2, kDefaultHTTP2).toBool());
  ui_->checkbox_verify_certificate->setChecked(s.value(kVerifyCertificate, kDefaultVerifyCertificate).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, kDefaultDownloadAlbumCovers).toBool());
  ui_->checkbox_server_scrobbling->setChecked(s.value(kServerSideScrobbling, kDefaultServerSideScrobbling).toBool());

  s.endGroup();

  Init(ui_->layout_jellyfinsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void JellyfinSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  s.setValue(kEnabled, ui_->enable->isChecked());
  s.setValue(kUrl, QUrl(ui_->server_url->text()));
  s.setValue(kUsername, ui_->username->text());
  s.setValue(kPassword, QString::fromUtf8(ui_->password->text().toUtf8().toBase64()));
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

  QUrl server_url(ui_->server_url->text());
  if (!server_url.isValid() || server_url.scheme().isEmpty() || server_url.host().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incorrect"), tr("Server URL is invalid."));
    return;
  }

  test_pending_ = true;
  Q_EMIT Test(server_url, ui_->username->text(), ui_->password->text());
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