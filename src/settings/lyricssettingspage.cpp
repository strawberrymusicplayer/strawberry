/*
 * Strawberry Music Player
 * Copyright 2020-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>

#include "settingsdialog.h"
#include "lyricssettingspage.h"
#include "ui_lyricssettingspage.h"
#include "constants/lyricssettings.h"
#include "core/iconloader.h"
#include "core/settings.h"
#include "lyrics/lyricsproviders.h"
#include "lyrics/lyricsprovider.h"
#include "widgets/loginstatewidget.h"

using namespace Qt::Literals::StringLiterals;
using namespace LyricsSettings;

LyricsSettingsPage::LyricsSettingsPage(SettingsDialog *dialog, const SharedPtr<LyricsProviders> lyrics_providers, QWidget *parent)
    : SettingsPage(dialog, parent),
      ui_(new Ui::LyricsSettingsPage),
      lyrics_providers_(lyrics_providers),
      provider_selected_(false) {

  ui_->setupUi(this);
  setWindowIcon(IconLoader::Load(u"view-media-lyrics"_s, true, 0, 32));

  QObject::connect(ui_->providers_up, &QPushButton::clicked, this, &LyricsSettingsPage::ProvidersMoveUp);
  QObject::connect(ui_->providers_down, &QPushButton::clicked, this, &LyricsSettingsPage::ProvidersMoveDown);
  QObject::connect(ui_->providers, &QListWidget::currentItemChanged, this, &LyricsSettingsPage::CurrentItemChanged);
  QObject::connect(ui_->providers, &QListWidget::itemSelectionChanged, this, &LyricsSettingsPage::ItemSelectionChanged);
  QObject::connect(ui_->providers, &QListWidget::itemChanged, this, &LyricsSettingsPage::ItemChanged);

  QObject::connect(ui_->button_authenticate, &QPushButton::clicked, this, &LyricsSettingsPage::AuthenticateClicked);
  QObject::connect(ui_->login_state, &LoginStateWidget::LogoutClicked, this, &LyricsSettingsPage::LogoutClicked);

  QObject::connect(ui_->checkbox_custom_api_credentials, &QCheckBox::toggled, ui_->lineedit_api_credential_id, &QLineEdit::setEnabled);
  QObject::connect(ui_->checkbox_custom_api_credentials, &QCheckBox::toggled, ui_->lineedit_api_credential_secret, &QLineEdit::setEnabled);
  QObject::connect(ui_->checkbox_custom_api_credentials, &QCheckBox::toggled, ui_->label_api_credential_id, &QLabel::setEnabled);
  QObject::connect(ui_->checkbox_custom_api_credentials, &QCheckBox::toggled, ui_->label_api_credential_secret, &QLabel::setEnabled);
  QObject::connect(ui_->checkbox_custom_api_credentials, &QCheckBox::toggled, this, &LyricsSettingsPage::CredentialsUiChanged);
  QObject::connect(ui_->lineedit_api_credential_id, &QLineEdit::textEdited, this, &LyricsSettingsPage::CredentialsUiChanged);
  QObject::connect(ui_->lineedit_api_credential_secret, &QLineEdit::textEdited, this, &LyricsSettingsPage::CredentialsUiChanged);

  ui_->login_state->AddCredentialGroup(ui_->widget_authenticate);

  NoProviderSelected();
  DisableAuthentication();
  ui_->groupbox_api_credentials->setVisible(false);

  dialog->installEventFilter(this);

}

LyricsSettingsPage::~LyricsSettingsPage() { delete ui_; }

void LyricsSettingsPage::Load() {

  credential_drafts_.clear();
  current_credentials_provider_.clear();

  ui_->providers->clear();

  QList<LyricsProvider*> lyrics_providers_sorted = lyrics_providers_->List();
  std::stable_sort(lyrics_providers_sorted.begin(), lyrics_providers_sorted.end(), ProviderCompareOrder);

  for (LyricsProvider *provider : std::as_const(lyrics_providers_sorted)) {
    QListWidgetItem *item = new QListWidgetItem(ui_->providers);
    item->setText(provider->name());
    item->setCheckState(provider->is_enabled() ? Qt::Checked : Qt::Unchecked);
    item->setForeground(provider->is_enabled() ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));
  }

  Init(ui_->layout_lyricssettingspage->parentWidget());

  if (!Settings().childGroups().contains(QLatin1String(kSettingsGroup))) set_changed();

}

void LyricsSettingsPage::Save() {

  QStringList providers;
  for (int i = 0; i < ui_->providers->count(); ++i) {
    const QListWidgetItem *item = ui_->providers->item(i);
    if (item->checkState() == Qt::Checked) providers << item->text();  // clazy:exclude=reserve-candidates
  }

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(kProviders, providers);
  s.endGroup();

  for (auto it = credential_drafts_.constBegin(); it != credential_drafts_.constEnd(); ++it) {
    LyricsProvider *provider = lyrics_providers_->ProviderByName(it.key());
    if (!provider) continue;
    SaveCredentialsUi(provider, it.value());
  }

}

void LyricsSettingsPage::CurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous) {

  if (item_previous) {
    LyricsProvider *provider = lyrics_providers_->ProviderByName(item_previous->text());
    if (provider && provider->authentication_required()) DisconnectAuthentication(provider);
  }

  if (item_current) {
    const int row = ui_->providers->row(item_current);
    ui_->providers_up->setEnabled(row != 0);
    ui_->providers_down->setEnabled(row != ui_->providers->count() - 1);
    LyricsProvider *provider = lyrics_providers_->ProviderByName(item_current->text());
    if (provider) {
      if (provider->authentication_required()) {
        ui_->login_state->SetLoggedIn(provider->authenticated() ? LoginStateWidget::State::LoggedIn : LoginStateWidget::State::LoggedOut);
        ui_->button_authenticate->setEnabled(true);
        ui_->button_authenticate->show();
        ui_->login_state->show();
        ui_->label_auth_info->setText(QStringLiteral("%1 needs authentication.").arg(provider->name()));
      }
      else {
        DisableAuthentication();
        ui_->label_auth_info->setText(QStringLiteral("%1 does not need authentication.").arg(provider->name()));
      }
      UpdateCredentialsUi(provider);
      provider_selected_ = true;
    }
  }
  else {
    DisableAuthentication();
    UpdateCredentialsUi(nullptr);
    NoProviderSelected();
    ui_->providers_up->setEnabled(false);
    ui_->providers_down->setEnabled(false);
    provider_selected_ = false;
  }

}

void LyricsSettingsPage::ItemSelectionChanged() {

  if (ui_->providers->selectedItems().count() == 0) {
    DisableAuthentication();
    UpdateCredentialsUi(nullptr);
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

void LyricsSettingsPage::ProvidersMoveUp() { ProvidersMove(-1); }

void LyricsSettingsPage::ProvidersMoveDown() { ProvidersMove(+1); }

void LyricsSettingsPage::ProvidersMove(const int d) {

  const int row = ui_->providers->currentRow();
  QListWidgetItem *item = ui_->providers->takeItem(row);
  ui_->providers->insertItem(row + d, item);
  ui_->providers->setCurrentRow(row + d);

  set_changed();

}

void LyricsSettingsPage::ItemChanged(QListWidgetItem *item) {

  item->setForeground((item->checkState() == Qt::Checked) ? palette().color(QPalette::Active, QPalette::Text) : palette().color(QPalette::Disabled, QPalette::Text));

  set_changed();

}

void LyricsSettingsPage::NoProviderSelected() {
  ui_->label_auth_info->setText(tr("No provider selected."));
}

void LyricsSettingsPage::DisableAuthentication() {

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_authenticate->setEnabled(false);
  ui_->login_state->hide();
  ui_->button_authenticate->hide();

}

void LyricsSettingsPage::DisconnectAuthentication(LyricsProvider *provider) const {

  QObject::disconnect(provider, &LyricsProvider::AuthenticationFailure, this, &LyricsSettingsPage::AuthenticationFailure);
  QObject::disconnect(provider, &LyricsProvider::AuthenticationSuccess, this, &LyricsSettingsPage::AuthenticationSuccess);

}

void LyricsSettingsPage::UpdateCredentialsUi(LyricsProvider *provider) {

  // Cleared up-front so CredentialsUiChanged() ignores the programmatic setChecked()/setText() calls below, which fire the very signals it listens to.
  current_credentials_provider_.clear();

  if (!provider || !provider->supports_custom_api_credentials()) {
    ui_->groupbox_api_credentials->setVisible(false);
    return;
  }

  ui_->groupbox_api_credentials->setVisible(true);

  ui_->label_api_credential_id->setText(provider->api_credentials_id_label());
  ui_->label_api_credential_secret->setText(provider->api_credentials_secret_label());
  ui_->label_api_credential_secret->setVisible(provider->api_credentials_use_secret());
  ui_->lineedit_api_credential_secret->setVisible(provider->api_credentials_use_secret());

  // A credentials_draft captured earlier this session takes precedence over what's currently saved, so switching providers back and forth doesn't lose in-progress edits before Save()/Cancel().
  CredentialsDraft credentials_draft;
  const bool has_credentials_draft = credential_drafts_.contains(provider->name());
  if (has_credentials_draft) {
    credentials_draft = credential_drafts_.value(provider->name());
  }
  else {
    Settings s;
    s.beginGroup(provider->api_credentials_settings_group());
    credentials_draft.use_custom_api_credentials = s.value(provider->api_credentials_use_custom_key(), false).toBool();
    credentials_draft.id = s.value(provider->api_credentials_id_key()).toString();
    credentials_draft.secret = s.value(provider->api_credentials_secret_key()).toString();
    s.endGroup();
  }

  if (!provider->has_compiled_api_credentials()) {
    ui_->checkbox_custom_api_credentials->hide();
    ui_->lineedit_api_credential_id->setEnabled(true);
    ui_->lineedit_api_credential_secret->setEnabled(true);
    ui_->label_api_credential_id->setEnabled(true);
    ui_->label_api_credential_secret->setEnabled(true);
  }
  else {
    ui_->checkbox_custom_api_credentials->show();
    ui_->checkbox_custom_api_credentials->setChecked(credentials_draft.use_custom_api_credentials);
    ui_->lineedit_api_credential_id->setEnabled(credentials_draft.use_custom_api_credentials);
    ui_->lineedit_api_credential_secret->setEnabled(credentials_draft.use_custom_api_credentials);
    ui_->label_api_credential_id->setEnabled(credentials_draft.use_custom_api_credentials);
    ui_->label_api_credential_secret->setEnabled(credentials_draft.use_custom_api_credentials);
  }

  ui_->lineedit_api_credential_id->setText(credentials_draft.id);
  ui_->lineedit_api_credential_secret->setText(credentials_draft.secret);

  // Seed the credentials_draft map with this provider's current (possibly just-loaded) values so Save() has something to persist for it even if the user never touches its fields this session.
  if (!has_credentials_draft) {
    credential_drafts_.insert(provider->name(), credentials_draft);
  }

  current_credentials_provider_ = provider->name();

}

void LyricsSettingsPage::SaveCredentialsUi(LyricsProvider *provider, const CredentialsDraft &credentials_draft) {

  if (!provider || !provider->supports_custom_api_credentials()) return;

  Settings s;
  s.beginGroup(provider->api_credentials_settings_group());
  s.setValue(provider->api_credentials_use_custom_key(), credentials_draft.use_custom_api_credentials);
  s.setValue(provider->api_credentials_id_key(), credentials_draft.id);
  s.setValue(provider->api_credentials_secret_key(), credentials_draft.secret);
  s.endGroup();

}

void LyricsSettingsPage::CredentialsUiChanged() {

  if (current_credentials_provider_.isEmpty()) return;

  CredentialsDraft credentials_draft;
  credentials_draft.use_custom_api_credentials = ui_->checkbox_custom_api_credentials->isChecked();
  credentials_draft.id = ui_->lineedit_api_credential_id->text();
  credentials_draft.secret = ui_->lineedit_api_credential_secret->text();

  credential_drafts_.insert(current_credentials_provider_, credentials_draft);

}

void LyricsSettingsPage::AuthenticateClicked() {

  if (!ui_->providers->currentItem()) return;
  LyricsProvider *provider = lyrics_providers_->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;

  // Persist the currently-edited custom credential selection first and reload it into the provider, so Authenticate() below uses whatever is currently shown in the UI rather than whatever was last saved.
  CredentialsDraft credentials_draft;
  credentials_draft.use_custom_api_credentials = ui_->checkbox_custom_api_credentials->isChecked();
  credentials_draft.id = ui_->lineedit_api_credential_id->text();
  credentials_draft.secret = ui_->lineedit_api_credential_secret->text();
  SaveCredentialsUi(provider, credentials_draft);
  provider->ReloadSettings();

  ui_->button_authenticate->setEnabled(false);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoginInProgress);
  QObject::connect(provider, &LyricsProvider::AuthenticationFailure, this, &LyricsSettingsPage::AuthenticationFailure);
  QObject::connect(provider, &LyricsProvider::AuthenticationSuccess, this, &LyricsSettingsPage::AuthenticationSuccess);
  provider->Authenticate();

}

void LyricsSettingsPage::LogoutClicked() {

  if (!ui_->providers->currentItem()) return;
  LyricsProvider *provider = lyrics_providers_->ProviderByName(ui_->providers->currentItem()->text());
  if (!provider) return;
  provider->ClearSession();

  ui_->button_authenticate->setEnabled(true);
  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);

}

void LyricsSettingsPage::AuthenticationSuccess() {

  LyricsProvider *provider = qobject_cast<LyricsProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedIn);
  ui_->button_authenticate->setEnabled(true);

}

void LyricsSettingsPage::AuthenticationFailure(const QString &error) {

  LyricsProvider *provider = qobject_cast<LyricsProvider*>(sender());
  if (!provider) return;
  DisconnectAuthentication(provider);

  if (!isVisible() || !ui_->providers->currentItem() || ui_->providers->currentItem()->text() != provider->name()) return;

  QMessageBox::warning(this, tr("Authentication failed"), error);

  ui_->login_state->SetLoggedIn(LoginStateWidget::State::LoggedOut);
  ui_->button_authenticate->setEnabled(true);

}

bool LyricsSettingsPage::ProviderCompareOrder(LyricsProvider *a, LyricsProvider *b) {
  return a->order() < b->order();
}
