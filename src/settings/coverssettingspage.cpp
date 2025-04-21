/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>
#include <utility>

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QPalette>
#include <QSettings>
#include <QGroupBox>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>

#include "settingsdialog.h"
#include "coverssettingspage.h"
#include "ui_coverssettingspage.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "utilities/coveroptions.h"
#include "covermanager/coverproviders.h"
#include "covermanager/coverprovider.h"
#include "widgets/loginstatewidget.h"
#include "constants/coverssettings.h"

using namespace Qt::Literals::StringLiterals;
using namespace CoversSettings;

CoversSettingsPage::CoversSettingsPage(SettingsDialog *dialog, const SharedPtr<CoverProviders> cover_providers, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::CoversSettingsPage),
      cover_providers_(cover_providers),
      provider_selected_(false),
      types_selected_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"cdcase"_s, true, 0, 32));

  QObject::connect(ui_->providers_up, &QPushButton::clicked, this, &CoversSettingsPage::ProvidersMoveUp);
  QObject::connect(ui_->providers_down, &QPushButton::clicked, this, &CoversSettingsPage::ProvidersMoveDown);
  QObject::connect(ui_->providers, &QListWidget::currentItemChanged, this, &CoversSettingsPage::ProvidersCurrentItemChanged);
  QObject::connect(ui_->providers, &QListWidget::itemSelectionChanged, this, &CoversSettingsPage::ProvidersItemSelectionChanged);
  QObject::connect(ui_->providers, &QListWidget::itemChanged, this, &CoversSettingsPage::ProvidersItemChanged);

  QObject::connect(ui_->button_authenticate, &QPushButton::clicked, this, &CoversSettingsPage::AuthenticateClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &CoversSettingsPage::LogoutClicked);

  QObject::connect(ui_->types_up, &QPushButton::clicked, this, &CoversSettingsPage::TypesMoveUp);
  QObject::connect(ui_->types_down, &QPushButton::clicked, this, &CoversSettingsPage::TypesMoveDown);
  QObject::connect(ui_->types, &QListWidget::currentItemChanged, this, &CoversSettingsPage::TypesCurrentItemChanged);
  QObject::connect(ui_->types, &QListWidget::itemSelectionChanged, this, &CoversSettingsPage::TypesItemSelectionChanged);
  QObject::connect(ui_->types, &QListWidget::itemChanged, this, &CoversSettingsPage::TypesItemChanged);

  QObject::connect(ui_->radiobutton_save_albumcover_albumdir, &QRadioButton::toggled, this, &CoversSettingsPage::CoverSaveInAlbumDirChanged);
  QObject::connect(ui_->radiobutton_cover_hash, &QRadioButton::toggled, this, &CoversSettingsPage::CoverSaveInAlbumDirChanged);
  QObject::connect(ui_->radiobutton_cover_pattern, &QRadioButton::toggled, this, &CoversSettingsPage::CoverSaveInAlbumDirChanged);

  ui_->login_state->AddCredentialGroup(ui_->widget_authenticate);

  NoProviderSelected();
  DisableAuthentication();

  dialog->installEventFilter(this);

}

CoversSettingsPage::~CoversSettingsPage() { delete ui_; }

void CoversSettingsPage::showEvent(QShowEvent *e) {

  ProvidersCurrentItemChanged(ui_->providers->currentItem(), nullptr);

  SettingsPage::showEvent(e);

}

void CoversSettingsPage::Load() {

  ui_->providers->clear();

  QList<CoverProvider*> cover_providers_sorted = cover_providers_->List();
  std::stable_sort(cover_providers_sorted.begin(), cover_providers_sorted.end(), ProviderCompareOrder);

  for (CoverProvider *provider : std::as_const(cover_providers_sorted)) {
    QListWidgetItem *item = new QListWidgetItem(ui_->providers);
    item->setText(provider->name());
    item->setCheckState(provider->enabled() ? Qt::Checked : Qt::Unchecked);
    item->setForeground(provider->enabled() ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));
  }

  Settings s;
  s.beginGroup(kSettingsGroup);

  const QStringList all_types = QStringList() << u"art_unset"_s
                                              << u"art_manual"_s
                                              << u"art_automatic"_s
                                              << u"art_embedded"_s;

  const QStringList types = s.value(kTypes, all_types).toStringList();

  ui_->types->clear();
  for (const QString &type : types) {
    AddAlbumCoverArtType(type, AlbumCoverArtTypeDescription(type), true);
  }

  for (const QString &type : all_types) {
    if (!types.contains(type)) {
      AddAlbumCoverArtType(type, AlbumCoverArtTypeDescription(type), false);
    }
  }

  const CoverOptions::CoverType save_cover_type = static_cast<CoverOptions::CoverType>(s.value(kSaveType, static_cast<int>(CoverOptions::CoverType::Cache)).toInt());
  switch (save_cover_type) {
    case CoverOptions::CoverType::Cache:
      ui_->radiobutton_save_albumcover_cache->setChecked(true);
      break;
    case CoverOptions::CoverType::Album:
      ui_->radiobutton_save_albumcover_albumdir->setChecked(true);
      break;
    case CoverOptions::CoverType::Embedded:
      ui_->radiobutton_save_albumcover_embedded->setChecked(true);
      break;
  }

  const CoverOptions::CoverFilename save_cover_filename = static_cast<CoverOptions::CoverFilename>(s.value(kSaveFilename, static_cast<int>(CoverOptions::CoverFilename::Pattern)).toInt());
  switch (save_cover_filename) {
    case CoverOptions::CoverFilename::Hash:
      ui_->radiobutton_cover_hash->setChecked(true);
      break;
    case CoverOptions::CoverFilename::Pattern:
      ui_->radiobutton_cover_pattern->setChecked(true);
      break;
  }
  QString cover_pattern = s.value(kSavePattern).toString();
  if (!cover_pattern.isEmpty()) ui_->lineedit_cover_pattern->setText(cover_pattern);
  ui_->checkbox_cover_overwrite->setChecked(s.value(kSaveOverwrite, false).toBool());
  ui_->checkbox_cover_lowercase->setChecked(s.value(kSaveLowercase, true).toBool());
  ui_->checkbox_cover_replace_spaces->setChecked(s.value(kSaveReplaceSpaces, true).toBool());

  s.endGroup();

  Init(ui_->layout_coverssettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void CoversSettingsPage::Save() {

  Settings s;
  s.beginGroup(kSettingsGroup);

  QStringList providers;
  for (int i = 0; i < ui_->providers->count(); ++i) {
    const QListWidgetItem *item = ui_->providers->item(i);
    if (item->checkState() == Qt::Checked) providers << item->text();
  }
  s.setValue(kProviders, providers);

  QStringList types;
  for (int i = 0; i < ui_->types->count(); ++i) {
    const QListWidgetItem *item = ui_->types->item(i);
    if (item->checkState() == Qt::Checked) {
      types << item->data(Type_Role_Name).toString();
    }
  }

  s.setValue(kTypes, types);

  CoverOptions::CoverType save_cover_type = CoverOptions::CoverType::Cache;
  if (ui_->radiobutton_save_albumcover_cache->isChecked()) save_cover_type = CoverOptions::CoverType::Cache;
  else if (ui_->radiobutton_save_albumcover_albumdir->isChecked()) save_cover_type = CoverOptions::CoverType::Album;
  else if (ui_->radiobutton_save_albumcover_embedded->isChecked()) save_cover_type = CoverOptions::CoverType::Embedded;
  s.setValue(kSaveType, static_cast<int>(save_cover_type));

  CoverOptions::CoverFilename save_cover_filename = CoverOptions::CoverFilename::Hash;
  if (ui_->radiobutton_cover_hash->isChecked()) save_cover_filename = CoverOptions::CoverFilename::Hash;
  else if (ui_->radiobutton_cover_pattern->isChecked()) save_cover_filename = CoverOptions::CoverFilename::Pattern;
  s.setValue(kSaveFilename, static_cast<int>(save_cover_filename));

  s.setValue(kSavePattern, ui_->lineedit_cover_pattern->text());
  s.setValue(kSaveOverwrite, ui_->checkbox_cover_overwrite->isChecked());
  s.setValue(kSaveLowercase, ui_->checkbox_cover_lowercase->isChecked());
  s.setValue(kSaveReplaceSpaces, ui_->checkbox_cover_replace_spaces->isChecked());

  s.endGroup();

}

void CoversSettingsPage::ProvidersCurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous) {

  if (item_previous) {
    CoverProvider *provider = cover_providers_->ProviderByName(item_previous->text());
    if (provider && provider->authentication_required()) DisconnectAuthentication(provider);
  }

  if (item_current) {
    const int row = ui_->providers->row(item_current);
    ui_->providers_up->setEnabled(row != 0);
    ui_->providers_down->setEnabled(row != ui_->providers->count() - 1);
    CoverProvider *provider = cover_providers_->ProviderByName(item_current->text());
    if (provider) {
      if (provider->authentication_required()) {
        if (provider->name() == "Tidal"_L1 && !provider->authenticated()) {
          DisableAuthentication();
          ui_->label_auth_info->setText(tr("Use Tidal settings to authenticate."));
        }
        else if (provider->name() == "Spotify"_L1 && !provider->authenticated()) {
          DisableAuthentication();
          ui_->label_auth_info->setText(tr("Use Spotify settings to authenticate."));
        }
        else if (provider->name() == "Qobuz"_L1 && !provider->authenticated()) {
          DisableAuthentication();
          ui_->label_auth_info->setText(tr("Use Qobuz settings to authenticate."));
        }
        else {
          ui_->login_state->SetLoggedIn(provider->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
          ui_->button_authenticate->setEnabled(true);
          ui_->button_authenticate->show();
          ui_->login_state->show();
          ui_->label_auth_info->setText(tr("%1 needs authentication.").arg(provider->name()));
        }
      }
      else {
        DisableAuthentication();
        ui_->label_auth_info->setText(tr("%1 does not need authentication.").arg(provider->name()));
      }
    }
    provider_selected_ = true;
  }
  else {
    DisableAuthentication();
    NoProviderSelected();
    ui_->providers_up->setEnabled(false);
    ui_->providers_down->setEnabled(false);
    provider_selected_ = false;
  }

}

void CoversSettingsPage::ProvidersItemSelectionChanged() {

  if (ui_->providers->selectedItems().count() == 0) {
    DisableAuthentication();
    NoProviderSelected();
    ui_->providers_up->setEnabled(false);
    ui_->providers_down->setEnabled(false);
    provider_selected_ = false;
  }
  else {
    if (ui_->providers->currentItem() && !provider_selected_) {
      ProvidersCurrentItemChanged(ui_->providers->currentItem(), nullptr);
    }
  }

}

void CoversSettingsPage::ProvidersMoveUp() { ProvidersMove(-1); }

void CoversSettingsPage::ProvidersMoveDown() { ProvidersMove(+1); }

void CoversSettingsPage::ProvidersMove(const int d) {

  const int row = ui_->providers->currentRow();
  QListWidgetItem *item = ui_->providers->takeItem(row);
  ui_->providers->insertItem(row + d, item);
  ui_->providers->setCurrentRow(row + d);

  set_changed();

}

void CoversSettingsPage::ProvidersItemChanged(QListWidgetItem *item) {

  item->setForeground((item->checkState() == Qt::Checked) ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));

  set_changed();

}

void CoversSettingsPage::NoProviderSelected() {
  ui_->label_auth_info->setText(tr("No provider selected."));
}

void CoversSettingsPage::DisableAuthentication() {

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_authenticate->setEnabled(false);
  ui_->login_state->hide();
  ui_->button_authenticate->hide();

}

void CoversSettingsPage::DisconnectAuthentication(CoverProvider *provider) const {

  QObject::disconnect(provider, &CoverProvider::AuthenticationFailure, this, &CoversSettingsPage::AuthenticationFailure);
  QObject::disconnect(provider, &CoverProvider::AuthenticationSuccess, this, &CoversSettingsPage::AuthenticationSuccess);

}

void CoversSettingsPage::AuthenticateClicked() {

  if (!ui_->providers->currentItem()) return;
  CoverProvider *provider = cover_providers_->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;
  ui_->button_authenticate->setEnabled(false);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoginInProgress);
  QObject::connect(provider, &CoverProvider::AuthenticationFailure, this, &CoversSettingsPage::AuthenticationFailure);
  QObject::connect(provider, &CoverProvider::AuthenticationSuccess, this, &CoversSettingsPage::AuthenticationSuccess);
  provider->Authenticate();

}

void CoversSettingsPage::LogoutClicked() {

  if (!ui_->providers->currentItem()) return;
  CoverProvider *provider = cover_providers_->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;
  provider->ClearSession();

  if (provider->name() == "Tidal"_L1) {
    DisableAuthentication();
    ui_->label_auth_info->setText(tr("Use Tidal settings to authenticate."));
  }
  else if (provider->name() == "Spotify"_L1) {
    DisableAuthentication();
    ui_->label_auth_info->setText(tr("Use Spotify settings to authenticate."));
  }
  else if (provider->name() == "Qobuz"_L1) {
    DisableAuthentication();
    ui_->label_auth_info->setText(tr("Use Qobuz settings to authenticate."));
  }
  else {
    ui_->button_authenticate->setEnabled(true);
    ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  }

}

void CoversSettingsPage::AuthenticationSuccess() {

  CoverProvider *provider = qobject_cast<CoverProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_authenticate->setEnabled(true);

}

void CoversSettingsPage::AuthenticationFailure(const QString &error) {

  CoverProvider *provider = qobject_cast<CoverProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  QMessageBox::warning(this, tr("Authentication failed"), error);

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_authenticate->setEnabled(true);

}

bool CoversSettingsPage::ProviderCompareOrder(CoverProvider *a, CoverProvider *b) {
  return a->order() < b->order();
}

void CoversSettingsPage::CoverSaveInAlbumDirChanged() {

  if (ui_->radiobutton_save_albumcover_albumdir->isChecked()) {
    if (!ui_->groupbox_cover_filename->isEnabled()) {
      ui_->groupbox_cover_filename->setEnabled(true);
    }
    if (ui_->radiobutton_cover_pattern->isChecked()) {
      if (!ui_->lineedit_cover_pattern->isEnabled()) ui_->lineedit_cover_pattern->setEnabled(true);
      if (!ui_->checkbox_cover_overwrite->isEnabled()) ui_->checkbox_cover_overwrite->setEnabled(true);
      if (!ui_->checkbox_cover_lowercase->isEnabled()) ui_->checkbox_cover_lowercase->setEnabled(true);
      if (!ui_->checkbox_cover_replace_spaces->isEnabled()) ui_->checkbox_cover_replace_spaces->setEnabled(true);
    }
    else {
      if (ui_->lineedit_cover_pattern->isEnabled()) ui_->lineedit_cover_pattern->setEnabled(false);
      if (ui_->checkbox_cover_overwrite->isEnabled()) ui_->checkbox_cover_overwrite->setEnabled(false);
      if (ui_->checkbox_cover_lowercase->isEnabled()) ui_->checkbox_cover_lowercase->setEnabled(false);
      if (ui_->checkbox_cover_replace_spaces->isEnabled()) ui_->checkbox_cover_replace_spaces->setEnabled(false);
    }
  }
  else {
    if (ui_->groupbox_cover_filename->isEnabled()) {
      ui_->groupbox_cover_filename->setEnabled(false);
    }
  }

}

void CoversSettingsPage::AddAlbumCoverArtType(const QString &name, const QString &description, const bool enabled) {

  QListWidgetItem *item = new QListWidgetItem;
  item->setData(Type_Role_Name, name);
  item->setText(description);
  item->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
  ui_->types->addItem(item);

}

QString CoversSettingsPage::AlbumCoverArtTypeDescription(const QString &type) const {

  if (type == "art_unset"_L1) {
    return tr("Manually unset (%1)").arg(type);
  }
  if (type == "art_manual"_L1) {
    return tr("Set through album cover search (%1)").arg(type);
  }
  if (type == "art_automatic"_L1) {
    return tr("Automatically picked up from album directory (%1)").arg(type);
  }
  if (type == "art_embedded"_L1) {
    return tr("Embedded album cover art (%1)").arg(type);
  }

  return QString();

}

void CoversSettingsPage::TypesMoveUp() { TypesMove(-1); }

void CoversSettingsPage::TypesMoveDown() { TypesMove(+1); }

void CoversSettingsPage::TypesMove(const int d) {

  const int row = ui_->types->currentRow();
  QListWidgetItem *item = ui_->types->takeItem(row);
  ui_->types->insertItem(row + d, item);
  ui_->types->setCurrentRow(row + d);

  set_changed();

}

void CoversSettingsPage::TypesItemChanged(QListWidgetItem *item) {

  item->setForeground((item->checkState() == Qt::Checked) ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));

  set_changed();

}

void CoversSettingsPage::TypesCurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous) {

  Q_UNUSED(item_previous)

  if (item_current) {
    const int row = ui_->types->row(item_current);
    ui_->types_up->setEnabled(row != 0);
    ui_->types_down->setEnabled(row != ui_->types->count() - 1);
    types_selected_ = true;
  }
  else {
    ui_->types_up->setEnabled(false);
    ui_->types_down->setEnabled(false);
    types_selected_ = false;
  }

}

void CoversSettingsPage::TypesItemSelectionChanged() {

  if (ui_->types->selectedItems().count() == 0) {
    ui_->types_up->setEnabled(false);
    ui_->types_down->setEnabled(false);
  }
  else {
    if (ui_->providers->currentItem() && !types_selected_) {
      TypesCurrentItemChanged(ui_->types->currentItem(), nullptr);
    }
  }

}
