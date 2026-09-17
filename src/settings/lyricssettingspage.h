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

#ifndef LYRICSSETTINGSPAGE_H
#define LYRICSSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"

class QListWidgetItem;

class LyricsProviders;
class LyricsProvider;
class SettingsDialog;
class Ui_LyricsSettingsPage;

class LyricsSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit LyricsSettingsPage(SettingsDialog *dialog, const SharedPtr<LyricsProviders> lyrics_providers, QWidget *parent = nullptr);
  ~LyricsSettingsPage() override;

  void Load() override;
  void Save() override;

 private:
  struct CredentialsDraft {
    bool use_custom_api_credentials = false;
    QString id;
    QString secret;
  };

  void NoProviderSelected();
  void ProvidersMove(const int d);
  void DisableAuthentication();
  void DisconnectAuthentication(LyricsProvider *provider) const;
  void UpdateCredentialsUi(LyricsProvider *provider);
  void SaveCredentialsUi(LyricsProvider *provider, const CredentialsDraft &credentials_draft);
  static bool ProviderCompareOrder(LyricsProvider *a, LyricsProvider *b);

 private Q_SLOTS:
  void CurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous);
  void ItemSelectionChanged();
  void ItemChanged(QListWidgetItem *item);
  void ProvidersMoveUp();
  void ProvidersMoveDown();
  void AuthenticateClicked();
  void LogoutClicked();
  void AuthenticationSuccess();
  void AuthenticationFailure(const QString &error);
  void CredentialsUiChanged();

 private:
  Ui_LyricsSettingsPage *ui_;
  const SharedPtr<LyricsProviders> lyrics_providers_;
  bool provider_selected_;
  QHash<QString, CredentialsDraft> credential_drafts_;
  QString current_credentials_provider_;
};

#endif  // LYRICSSETTINGSPAGE_H
