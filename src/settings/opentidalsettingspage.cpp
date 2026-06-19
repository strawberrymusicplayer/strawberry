/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include "opentidalsettingspage.h"
#include "ui_opentidalsettingspage.h"
#include "constants/opentidalsettings.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "opentidal/opentidalservice.h"
#include "widgets/loginstatewidget.h"

using namespace Qt::Literals::StringLiterals;
using namespace OpenTidalSettings;

OpenTidalSettingsPage::OpenTidalSettingsPage(SettingsDialog *dialog, SharedPtr<OpenTidalService> service, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::OpenTidalSettingsPage),
      service_(service) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"tidal"_s, true, 0, 32));

  QObject::connect(ui_->button_login, &QPushButton::clicked, this, &OpenTidalSettingsPage::LoginClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &OpenTidalSettingsPage::LogoutClicked);

  QObject::connect(this, &OpenTidalSettingsPage::Authorize, &*service_, &OpenTidalService::StartAuthorization);

  QObject::connect(&*service_, &StreamingService::LoginFailure, this, &OpenTidalSettingsPage::LoginFailure);
  QObject::connect(&*service_, &StreamingService::LoginSuccess, this, &OpenTidalSettingsPage::LoginSuccess);

  dialog->installEventFilter(this);

  ui_->urischeme->addItem(u"data"_s, static_cast<int>(UriScheme::DATA));
  ui_->urischeme->addItem(u"https"_s, static_cast<int>(UriScheme::HTTPS));

  ui_->manifesttype->addItem(u"MPEG-DASH"_s, static_cast<int>(ManifestType::MPEG_DASH));
  ui_->manifesttype->addItem(u"HLS"_s, static_cast<int>(ManifestType::HLS));

  ui_->format->addItem(u"HEAACV1"_s, u"HEAACV1"_s);
  ui_->format->addItem(u"AACLC"_s, u"AACLC"_s);
  ui_->format->addItem(u"FLAC"_s, u"FLAC"_s);
  ui_->format->addItem(u"FLAC_HIRES"_s, u"FLAC_HIRES"_s);
  ui_->format->addItem(u"EAC3_JOC"_s, u"EAC3_JOC"_s);

  ui_->coversize->addItem(u"160x160"_s, u"160x160"_s);
  ui_->coversize->addItem(u"320x320"_s, u"320x320"_s);
  ui_->coversize->addItem(u"640x640"_s, u"640x640"_s);
  ui_->coversize->addItem(u"750x750"_s, u"750x750"_s);
  ui_->coversize->addItem(u"1280x1280"_s, u"1280x1280"_s);

}

OpenTidalSettingsPage::~OpenTidalSettingsPage() { delete ui_; }

void OpenTidalSettingsPage::showEvent(QShowEvent *e) {

  ui_->login_state->SetLoggedIn(service_->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
  SettingsPage::showEvent(e);

}

void OpenTidalSettingsPage::Load() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  ui_->enable->setChecked(s.value(kEnabled, false).toBool());
  //ui_->client_id->setText(s.value(kClientId).toString());
  ui_->urischeme->setCurrentIndex(ui_->urischeme->findData(s.value(kUriScheme, static_cast<int>(UriScheme::DATA)).toInt()));
  ui_->manifesttype->setCurrentIndex(ui_->manifesttype->findData(s.value(kManifestType, static_cast<int>(ManifestType::MPEG_DASH)).toInt()));
  ComboBoxLoadFromSettings(s, ui_->format, QLatin1String(kFormat), u"FLAC_HIRES"_s);
  ui_->searchdelay->setValue(s.value(kSearchDelay, 1500).toInt());
  ui_->artistssearchlimit->setValue(s.value(kArtistsSearchLimit, 4).toInt());
  ui_->albumssearchlimit->setValue(s.value(kAlbumsSearchLimit, 10).toInt());
  ui_->songssearchlimit->setValue(s.value(kSongsSearchLimit, 10).toInt());
  ui_->checkbox_fetchalbums->setChecked(s.value(kFetchAlbums, false).toBool());
  ui_->checkbox_download_album_covers->setChecked(s.value(kDownloadAlbumCovers, true).toBool());
  ComboBoxLoadFromSettings(s, ui_->coversize, QLatin1String(kCoverSize), u"640x640"_s);
  ui_->checkbox_album_explicit->setChecked(s.value(kAlbumExplicit, false).toBool());
  ui_->checkbox_remove_remastered->setChecked(s.value(kRemoveRemastered, true).toBool());
  s.endGroup();

  if (service_->authenticated()) {
    ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  }

  Init(ui_->layout_opentidalsettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void OpenTidalSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kEnabled, ui_->enable->isChecked());
  //s.setValue(kClientId, ui_->client_id->text());
  s.setValue(kUriScheme, ui_->urischeme->currentData().toInt());
  s.setValue(kManifestType, ui_->manifesttype->currentData().toInt());
  s.setValue(kFormat, ui_->format->currentData().toString());
  s.setValue(kSearchDelay, ui_->searchdelay->value());
  s.setValue(kArtistsSearchLimit, ui_->artistssearchlimit->value());
  s.setValue(kAlbumsSearchLimit, ui_->albumssearchlimit->value());
  s.setValue(kSongsSearchLimit, ui_->songssearchlimit->value());
  s.setValue(kFetchAlbums, ui_->checkbox_fetchalbums->isChecked());
  s.setValue(kDownloadAlbumCovers, ui_->checkbox_download_album_covers->isChecked());
  s.setValue(kCoverSize, ui_->coversize->currentData().toString());
  s.setValue(kAlbumExplicit, ui_->checkbox_album_explicit->isChecked());
  s.setValue(kRemoveRemastered, ui_->checkbox_remove_remastered->isChecked());
  s.endGroup();

}

void OpenTidalSettingsPage::LoginClicked() {

  //if (ui_->client_id->text().isEmpty()) {
    //QMessageBox::critical(this, tr("Configuration incomplete"), tr("Missing Open Tidal client ID."));
    //return;
  //}
  Q_EMIT Authorize(ui_->client_id->text());
  ui_->button_login->setEnabled(false);

}

bool OpenTidalSettingsPage::eventFilter(QObject *object, QEvent *event) {

  if (object == dialog() && event->type() == QEvent::Enter) {
    ui_->button_login->setEnabled(true);
  }

  return SettingsPage::eventFilter(object, event);

}

void OpenTidalSettingsPage::LogoutClicked() {

  service_->ClearSession();
  ui_->button_login->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void OpenTidalSettingsPage::LoginSuccess() {

  if (!isVisible()) return;
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_login->setEnabled(true);

}

void OpenTidalSettingsPage::LoginFailure(const QString &failure_reason) {

  if (!isVisible()) return;
  QMessageBox::warning(this, tr("Authentication failed"), failure_reason);
  ui_->button_login->setEnabled(true);

}
