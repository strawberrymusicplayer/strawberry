/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "settings/settingspage.h"

class QListWidgetItem;

class CoverProvider;
class SettingsDialog;
class Ui_CoversSettingsPage;

class CoversSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit CoversSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~CoversSettingsPage() override;

  static const char *kSettingsGroup;
  static const char *kProviders;
  static const char *kTypes;
  static const char *kSaveType;
  static const char *kSaveFilename;
  static const char *kSavePattern;
  static const char *kSaveOverwrite;
  static const char *kSaveLowercase;
  static const char *kSaveReplaceSpaces;

  void Load() override;
  void Save() override;

 private:
  void NoProviderSelected();
  void ProvidersMove(const int d);
  void DisableAuthentication();
  void DisconnectAuthentication(CoverProvider *provider) const;
  static bool ProviderCompareOrder(CoverProvider *a, CoverProvider *b);
  void AddAlbumCoverArtType(const QString &name, const QString &description, const bool enabled);
  QString AlbumCoverArtTypeDescription(const QString &type) const;
  void TypesMove(const int d);

 private slots:
  void ProvidersCurrentItemChanged(QListWidgetItem *item_current, QListWidgetItem *item_previous);
  void ProvidersItemSelectionChanged();
  void ProvidersItemChanged(QListWidgetItem *item);
  void ProvidersMoveUp();
  void ProvidersMoveDown();
  void AuthenticateClicked();
  void LogoutClicked();
  void AuthenticationSuccess();
  void AuthenticationFailure(const QStringList &errors);
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
  bool provider_selected_;
  bool types_selected_;
};

#endif  // COVERSSETTINGSPAGE_H
