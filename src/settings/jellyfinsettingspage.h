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

#ifndef JELLYFINSETTINGSPAGE_H
#define JELLYFINSETTINGSPAGE_H

#include "config.h"

#include <QUrl>
#include <QString>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"
#include "credentialsmanager/credentialsreply.h"

class QEvent;
class JellyfinService;
class Ui_JellyfinSettingsPage;

class JellyfinSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit JellyfinSettingsPage(SettingsDialog *dialog, const SharedPtr<JellyfinService> service, QWidget *parent = nullptr);
  ~JellyfinSettingsPage() override;

  void Load() override;
  void Save() override;

 private Q_SLOTS:
  void TestClicked();
  void TestSuccess();
  void TestFailure(const QString &failure_reason);
  void ReadPasswordFinished();
  void SavePasswordFinished();
  void EnableToggled(const bool enabled);

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 Q_SIGNALS:
  void Test(const QUrl &url, const QString &username, const QString &password, const bool http2, const bool verify_certificate);

 private:
  static bool IsValidServerUrl(const QUrl &server_url);
  void ReadPassword();
  bool ConfirmServerUrl(const QUrl &server_url);
  Ui_JellyfinSettingsPage *ui_;
  const SharedPtr<JellyfinService> service_;
  bool test_pending_;
  CredentialsReplyPtr read_password_reply_;
  CredentialsReplyPtr save_password_reply_;
  QString loaded_password_;
  QString saving_password_;  // The password being written to the credentials manager, empty to remove it.
  bool password_save_failed_;  // Writing the password failed, unsaved_password_ is written again on the next save until it succeeds.
  QString unsaved_password_;
  bool password_requested_;  // The password was read, or is being read, from the credentials manager.
  QUrl confirmed_server_url_;  // HTTP server URL the user accepted sending the credentials unencrypted to.
};

#endif  // JELLYFINSETTINGSPAGE_H
