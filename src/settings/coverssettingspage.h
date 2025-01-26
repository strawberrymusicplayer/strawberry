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

#ifndef COVERSSETTINGSPAGE_H
#define COVERSSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"

class QListWidgetItem;
class QShowEvent;

class CoverProviders;
class CoverProvider;
class SettingsDialog;
class Ui_CoversSettingsPage;

class CoversSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit CoversSettingsPage(SettingsDialog *dialog, const SharedPtr<CoverProviders> cover_providers, QWidget *parent = nullptr);
  ~CoversSettingsPage() override;

  void Load() override;
  void Save() override;

 protected:
  void showEvent(QShowEvent *e) override;

 private:
  void NoProviderSelected();
  void ProvidersMove(const int d);
  void DisableAuthentication();
  void DisconnectAuthentication(CoverProvider *provider) const;
  static bool ProviderCompareOrder(CoverProvider *a, CoverProvider *b);
  void AddAlbumCoverArtType(const QString &name, const QString &description, const bool enabled);
  QString AlbumCoverArtTypeDescription(const QString &type) const;
  void TypesMove(const int d);

 private Q_SLOTS:
  void ProvidersCurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous);
  void ProvidersItemSelectionChanged();
  void ProvidersItemChanged(QListWidgetItem *item);
  void ProvidersMoveUp();
  void ProvidersMoveDown();
  void AuthenticateClicked();
  void LogoutClicked();
  void AuthenticationSuccess();
  void AuthenticationFailure(const QString &error);
  void CoverSaveInAlbumDirChanged();
  void TypesCurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous);
  void TypesItemSelectionChanged();
  void TypesItemChanged(QListWidgetItem *item);
  void TypesMoveUp();
  void TypesMoveDown();

 private:
  enum Type_Role {
    Type_Role_Name = Qt::UserRole + 1
  };

  Ui_CoversSettingsPage *ui_;

  const SharedPtr<CoverProviders> cover_providers_;

  bool provider_selected_;
  bool types_selected_;
};

#endif  // COVERSSETTINGSPAGE_H
