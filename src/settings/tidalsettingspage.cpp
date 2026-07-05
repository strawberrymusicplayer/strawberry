/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QMessageBox>
#include <QEvent>

#include "settingsdialog.h"
#include "tidalsettingspage.h"
#include "ui_tidalsettingspage.h"
#include "constants/tidalsettings.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "tidal/tidalservice.h"
#include "widgets/loginstatewidget.h"

using namespace Qt::Literals::StringLiterals;
using namespace TidalSettings;

TidalSettingsPage::TidalSettingsPage(SettingsDialog *dialog, SharedPtr<TidalService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::TidalSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"tidal"_s, true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &TidalSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &TidalSettingsPage::LogoutClicked);

  QObject::connect(this, &TidalSettingsPage::Authorize, &*service_, &TidalService::StartAuthorization);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &TidalSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &TidalSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->quality->addItem(u"Low"_s, u"LOW"_s);
  ui_->quality->addItem(u"High"_s, u"HIGH"_s);
  ui_->quality->addItem(u"Lossless"_s, u"LOSSLESS"_s);
  ui_->quality->addItem(u"Hi resolution"_s, u"HI_RES"_s);
  ui_->quality->addItem(u"Hi resolution lossless"_s, u"HI_RES_LOSSLESS"_s);

  ui_->coversize->addItem(u"160x160"_s, u"160x160"_s);
  ui_->coversize->addItem(u"320x320"_s, u"320x320"_s);
  ui_->coversize->addItem(u"640x640"_s, u"640x640"_s);
  ui_->coversize->addItem(u"750x750"_s, u"750x750"_s);
  ui_->coversize->addItem(u"1280x1280"_s, u"1280x1280"_s);

  ui_->streamurl->addItem(u"streamurl"_s, static_cast<int>(StreamUrlMethod::StreamUrl));
  ui_->streamurl->addItem(u"urlpostpaywall"_s, static_cast<int>(StreamUrlMethod::UrlPostPaywall));
  ui_->streamurl->addItem(u"playbackinfopostpaywall"_s, static_cast<int>(StreamUrlMethod::PlaybackInfoPostPaywall));

}

TidalSettingsPage::~TidalSettingsPage() { delete ui_; }

void TidalSettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void TidalSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, kDefaultEnabled).toBool());
  ui_->client_id->setText(s.value(kClientId).toString());
  ComboBoxLoadFromSettings(s, ui_->quality, QLatin1String(kQuality), QLatin1String(kDefaultQuality));
  ui_->searchdelay->setValue(s.value(kSearchDelay, kDefaultSearchDelay).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, kDefaultArtistsSearchLimit).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, kDefaultAlbumsSearchLimit).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, kDefaultSongsSearchLimit).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value(kFetchAlbums, kDefaultFetchAlbums).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, kDefaultDownloadAlbumCovers).toBool());
  ComboBoxLoadFromSettings(s, ui_->coversize, QLatin1String(kCoverSize), QLatin1String(kDefaultCoverSize));
  ui_->streamurl->setCurrentIndex(ui_->streamurl->findData(s.value(kStreamUrl, static_cast<int>(kDefaultStreamUrl)).toInt()));
  ui_->checkbox_album_explicit->setChecked(s.value(kAlbumExplicit, kDefaultAlbumExplicit).toBool());
  ui_->checkbox_remove_remastered->setChecked(s.value(kRemoveRemastered, kDefaultRemoveRemastered).toBool());
  s.endGroup();

  if (service_->authenticated()) {
    ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  }

  Init(ui_->layout_tidalsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void TidalSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  if (s.contains(kOAuth)) {
    s.remove(kOAuth);
  }
  if (s.contains(kApiToken)) {
    s.remove(kApiToken);
  }
  if (s.contains(kUsername)) {
    s.remove(kUsername);
  }
  if (s.contains(kPassword)) {
    s.remove(kPassword);
  }
  s.setValue(kClientId, ui_->client_id->text());
  s.setValue(kQuality, ui_->quality->currentData().toString());
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kFetchAlbums, ui_->checkbox_fetchalbums->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.setValue(kCoverSize, ui_->coversize->currentData().toString());
  s.setValue(kStreamUrl, ui_->streamurl->currentData().toInt());
  s.setValue(kAlbumExplicit, ui_->checkbox_album_explicit->isChecked());
  s.setValue(kRemoveRemastered, ui_->checkbox_remove_remastered->isChecked());
  s.endGroup();

}

void TidalSettingsPage::LoginClicked() {

  if (ui_->client_id->text().isEmpty()) {
    QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing Tidal client ID."));
    return;
  }
  Q_EMIT Authorize(ui_->client_id->text());
  ui_->button_login->setEnabled(false);

}

bool TidalSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void TidalSettingsPage::LogoutClicked() {

  service_->ClearSession();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void TidalSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void TidalSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}
