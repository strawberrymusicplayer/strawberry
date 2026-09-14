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

 protected:
  bool eventFilter(QObject *object, QEvent *event) override;

 Q_SIGNALS:
  void Test(const QUrl &url, const QString &username, const QString &password);

 private:
  Ui_JellyfinSettingsPage *ui_;
  const SharedPtr<JellyfinService> service_;
  bool test_pending_ = false;
};

#endif  // JELLYFINSETTINGSPAGE_H