/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QVariant>
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
#include "core/application.h"
#include "core/iconloader.h"
#include "covermanager/coverproviders.h"
#include "covermanager/coverprovider.h"
#include "widgets/loginstatewidget.h"

const char *CoversSettingsPage::kSettingsGroup = "Covers";

CoversSettingsPage::CoversSettingsPage(SettingsDialog *parent) : SettingsPage(parent), ui_(new Ui::CoversSettingsPage), provider_selected_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load("cdcase"));

  connect(ui_->providers_up, SIGNAL(clicked()), SLOT(ProvidersMoveUp()));
  connect(ui_->providers_down, SIGNAL(clicked()), SLOT(ProvidersMoveDown()));
  connect(ui_->providers, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)), SLOT(CurrentItemChanged(QListWidgetItem*, QListWidgetItem*)));
  connect(ui_->providers, SIGNAL(itemSelectionChanged()), SLOT(ItemSelectionChanged()));
  connect(ui_->providers, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(ItemChanged(QListWidgetItem*)));

  connect(ui_->button_authenticate, SIGNAL(clicked()), SLOT(AuthenticateClicked()));
  connect(ui_->login_state, SIGNAL(LogoutClicked()), SLOT(LogoutClicked()));

  NoProviderSelected();
  DisableAuthentication();

  dialog()->installEventFilter(this);

}

CoversSettingsPage::~CoversSettingsPage() { delete ui_; }

void CoversSettingsPage::Load() {

  ui_->providers->clear();

  QList<CoverProvider*> cover_providers_sorted = dialog()->app()->cover_providers()->List();
  std::stable_sort(cover_providers_sorted.begin(), cover_providers_sorted.end(), ProviderCompareOrder);

  for (CoverProvider *provider : cover_providers_sorted) {
    QListWidgetItem *item = new QListWidgetItem(ui_->providers);
    item->setText(provider->name());
    item->setCheckState(provider->is_enabled() ? Qt::Checked : Qt::Unchecked);
    item->setForeground(provider->is_enabled() ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));
  }

}

void CoversSettingsPage::Save() {

  QStringList providers;
  for (int i = 0 ; i < ui_->providers->count() ; ++i) {
    const QListWidgetItem *item = ui_->providers->item(i);
    if (item->checkState() == Qt::Checked) providers << item->text();
  }

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("providers", providers);
  s.endGroup();

}

void CoversSettingsPage::CurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous) {

  if (item_previous) {
    CoverProvider *provider = dialog()->app()->cover_providers()->ProviderByName(item_previous->text());
    if (provider && provider->AuthenticationRequired()) DisconnectAuthentication(provider);
  }

  if (item_current) {
    const int row = ui_->providers->row(item_current);
    ui_->providers_up->setEnabled(row != 0);
    ui_->providers_down->setEnabled(row != ui_->providers->count() - 1);
    CoverProvider *provider = dialog()->app()->cover_providers()->ProviderByName(item_current->text());
    if (provider && provider->AuthenticationRequired()) {
      if (provider->name() == "Tidal" && !provider->IsAuthenticated()) {
        DisableAuthentication();
        ui_->label_auth_info->setText(tr("Use Tidal settings to authenticate."));
      }
      else {
        ui_->login_state->SetLoggedIn(provider->IsAuthenticated() ? LoginStateWidget::LoggedIn : LoginStateWidget::LoggedOut);
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

void CoversSettingsPage::ItemSelectionChanged() {

  if (ui_->providers->selectedItems().count() == 0) {
    DisableAuthentication();
    NoProviderSelected();
    ui_->providers_up->setEnabled(false);
    ui_->providers_down->setEnabled(false);
    provider_selected_ = false;
  }
  else {
    if (ui_->providers->currentItem() && !provider_selected_) {
      CurrentItemChanged(ui_->providers->currentItem(), nullptr);
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

}

void CoversSettingsPage::ItemChanged(QListWidgetItem *item) {

  item->setForeground((item->checkState() == Qt::Checked) ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));

}

void CoversSettingsPage::NoProviderSelected() {
  ui_->label_auth_info->setText(tr("No provider selected."));
}

void CoversSettingsPage::DisableAuthentication() {

  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
  ui_->button_authenticate->setEnabled(false);
  ui_->login_state->hide();
  ui_->button_authenticate->hide();

}

void CoversSettingsPage::DisconnectAuthentication(CoverProvider *provider) {

  disconnect(provider, SIGNAL(AuthenticationFailure(QStringList)), this, SLOT(AuthenticationFailure(QStringList)));
  disconnect(provider, SIGNAL(AuthenticationSuccess()), this, SLOT(AuthenticationSuccess()));

}

void CoversSettingsPage::AuthenticateClicked() {

  if (!ui_->providers->currentItem()) return;
  CoverProvider *provider = dialog()->app()->cover_providers()->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;
  ui_->button_authenticate->setEnabled(false);
  connect(provider, SIGNAL(AuthenticationFailure(QStringList)), this, SLOT(AuthenticationFailure(QStringList)));
  connect(provider, SIGNAL(AuthenticationSuccess()), this, SLOT(AuthenticationSuccess()));
  provider->Authenticate();

}

void CoversSettingsPage::LogoutClicked() {

  if (!ui_->providers->currentItem()) return;
  CoverProvider *provider = dialog()->app()->cover_providers()->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;
  provider->Deauthenticate();

  if (provider->name() == "Tidal") {
    DisableAuthentication();
    ui_->label_auth_info->setText(tr("Use Tidal settings to authenticate."));
  }
  else {
    ui_->button_authenticate->setEnabled(true);
    ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
  }

}

void CoversSettingsPage::AuthenticationSuccess() {

  CoverProvider *provider = qobject_cast<CoverProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!this->isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedIn);
  ui_->button_authenticate->setEnabled(true);

}

void CoversSettingsPage::AuthenticationFailure(const QStringList &errors) {

  CoverProvider *provider = qobject_cast<CoverProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!this->isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  QMessageBox::warning(this, tr("Authentication failed"), errors.join("\n"));

  ui_->login_state->SetLoggedIn(LoginStateWidget::LoggedOut);
  ui_->button_authenticate->setEnabled(true);

}

bool CoversSettingsPage::ProviderCompareOrder(CoverProvider *a, CoverProvider *b) {
  return a->order() < b->order();
}
