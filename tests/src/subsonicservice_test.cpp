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

#include <memory>

#include "gtest_include.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QString>
#include <QUrl>
#include <QSignalSpy>
#include <QHostAddress>
#include <QTcpServer>

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/song.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/urlhandlers.h"
#include "covermanager/albumcoverloader.h"
#include "credentialsmanager/credentialsmanager.h"
#include "credentialsmanager/credentialsreply.h"
#include "constants/subsonicsettings.h"
#include "subsonic/subsonicservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class SubsonicServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {

    // Settings without an organization name always have an access error, which makes the settings backend of the credentials manager fail to save passwords.
    organization_name_ = QCoreApplication::organizationName();
    QCoreApplication::setOrganizationName(u"Strawberry"_s);

    // The settings backend stores nothing for Subsonic, so the password is read as not found, and a released songs request fails at once with a missing password instead of going to the network.
    Settings s;
    s.beginGroup(CredentialsManager::kSettingsGroup);
    s.setValue(CredentialsManager::kBackend, CredentialsManager::BackendToString(CredentialsManager::BackendType::Settings));
    s.endGroup();
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.remove(SubsonicSettings::kPassword);
    s.setValue(SubsonicSettings::kEnabled, true);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:1"_s));
    s.setValue(SubsonicSettings::kUsername, u"user"_s);
    s.endGroup();

    database_ = make_shared<MemoryDatabase>(nullptr);
    // The tests save and delete passwords with the credentials manager of the service, so they are queued after the requests of the service.
    credentials_manager_ = make_shared<CredentialsManager>();
    service_ = std::make_unique<SubsonicService>(make_shared<TaskManager>(), database_, make_shared<NetworkAccessManager>(), credentials_manager_, make_shared<UrlHandlers>(), make_shared<AlbumCoverLoader>(nullptr));

  }

  void TearDown() override {
    service_.reset();
    QCoreApplication::setOrganizationName(organization_name_);
  }

  // Processes events until the password is loaded, returns false on timeout.
  bool WaitForPassword(const int timeout_msec = 10000) const {
    QElapsedTimer timer;
    timer.start();
    while (service_->password_loading()) {
      if (timer.elapsed() > timeout_msec) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    // Let queued signals from the password reply finish.
    QCoreApplication::processEvents();
    return true;
  }

  QString organization_name_;
  SharedPtr<MemoryDatabase> database_;
  SharedPtr<CredentialsManager> credentials_manager_;
  std::unique_ptr<SubsonicService> service_;
};

TEST_F(SubsonicServiceTest, GetSongsWaitsForPassword) {

  ASSERT_TRUE(service_->password_loading());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  EXPECT_EQ(spy.count(), 0);

  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(1).toString(), u"Missing username or password."_s);

}

TEST_F(SubsonicServiceTest, AbortWhilePasswordLoadingCancelsGetSongs) {

  ASSERT_TRUE(service_->password_loading());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  service_->ResetSongsRequest();

  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(spy.count(), 0);

}

// Waits for a credentials reply, returns false on timeout or failure.
bool WaitForCredentialsReply(const CredentialsReplyPtr &reply) {
  QSignalSpy spy(&*reply, &CredentialsReply::Finished);
  return spy.wait(10000) && reply->success();
}

// Applying the settings while a collection refresh is running must not send the requests that follow with an empty password.
TEST_F(SubsonicServiceTest, ReloadSettingsKeepsPasswordWhileLoading) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));

  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"secret"_s);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(service_->password(), u"secret"_s);

  // A password which was deleted is cleared when it has been read.
  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"secret"_s);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_TRUE(service_->password().isEmpty());

}

// The password is cleared when Subsonic is disabled, since it's not read then.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenDisabled) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.endGroup();
  }
  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A password in the settings file from an older version is used at once, and migrated to the credentials manager.
TEST_F(SubsonicServiceTest, ReloadSettingsUsesPasswordFromSettingsFile) {

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kPassword, QString::fromLatin1(QByteArray("old_secret").toBase64()));
    s.endGroup();
  }

  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_EQ(service_->password(), u"old_secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password from the settings file is migrated, but not used, when Subsonic is disabled.
TEST_F(SubsonicServiceTest, ReloadSettingsDoesNotUsePasswordFromSettingsFileWhenDisabled) {

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.setValue(SubsonicSettings::kPassword, QString::fromLatin1(QByteArray("old_secret").toBase64()));
    s.endGroup();
  }

  service_->ReloadSettings();
  EXPECT_FALSE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());

  // The read is queued after the migration.
  CredentialsReplyPtr read_reply = credentials_manager_->ReadPasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService));
  ASSERT_TRUE(WaitForCredentialsReply(read_reply));
  EXPECT_EQ(read_reply->password(), u"old_secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password for the previous server must not be sent to a new server while the password is read again.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenServerChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:2"_s));
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_TRUE(WaitForPassword());
  EXPECT_EQ(service_->password(), u"secret"_s);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// The password for the previous user must not be sent for a new user while the password is read again.
TEST_F(SubsonicServiceTest, ReloadSettingsClearsPasswordWhenUsernameChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUsername, u"other_user"_s);
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_TRUE(WaitForPassword());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which is running when the server is changed is stopped, so it doesn't send the next requests for the previous server to the new server.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenServerChanges) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  // The refresh is started and the server is changed before the first reply arrives.
  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_EQ(spy.count(), 0);
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(u"http://localhost:2"_s));
    s.endGroup();
  }
  service_->ReloadSettings();
  ASSERT_EQ(spy.count(), 1);
  EXPECT_TRUE(spy.at(0).at(0).value<SongMap>().isEmpty());
  EXPECT_FALSE(spy.at(0).at(1).toString().isEmpty());

  // The stopped refresh doesn't send results anymore.
  ASSERT_TRUE(WaitForPassword());
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 500) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  EXPECT_EQ(spy.count(), 1);

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which is running when Subsonic is disabled is stopped, since the password is cleared.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenDisabled) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_EQ(spy.count(), 0);
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kEnabled, false);
    s.endGroup();
  }
  service_->ReloadSettings();
  EXPECT_EQ(spy.count(), 1);
  EXPECT_TRUE(service_->password().isEmpty());

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

// A collection refresh which was kept running while the password was read again is stopped when the password isn't found anymore.
TEST_F(SubsonicServiceTest, ReloadSettingsStopsRefreshWhenPasswordIsNotFound) {

  // The server accepts the requests, but doesn't reply, so the refresh keeps running.
  QTcpServer server;
  ASSERT_TRUE(server.listen(QHostAddress::LocalHost));
  {
    Settings s;
    s.beginGroup(SubsonicSettings::kSettingsGroup);
    s.setValue(SubsonicSettings::kUrl, QUrl(QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort())));
    s.endGroup();
  }

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());
  ASSERT_EQ(service_->password(), u"secret"_s);

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));
  ASSERT_EQ(spy.count(), 0);

  service_->ReloadSettings();
  ASSERT_TRUE(service_->password_loading());
  EXPECT_EQ(spy.count(), 0);
  ASSERT_TRUE(WaitForPassword());
  EXPECT_TRUE(service_->password().isEmpty());
  ASSERT_EQ(spy.count(), 1);
  EXPECT_EQ(spy.at(0).at(1).toString(), u"Missing username or password."_s);

}

// Applying the settings without changing the server keeps a running collection refresh.
TEST_F(SubsonicServiceTest, ReloadSettingsKeepsRefreshWhenServerIsUnchanged) {

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->SavePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService), u"secret"_s)));
  service_->ReloadSettings();
  ASSERT_TRUE(WaitForPassword());

  QSignalSpy spy(&*service_, &SubsonicService::SongsResults);
  service_->GetSongs();
  service_->ReloadSettings();
  EXPECT_EQ(spy.count(), 0);

  // The refresh fails, since there's no server, but it's the result of the refresh which was kept running.
  EXPECT_TRUE(spy.wait(10000));
  service_->ResetSongsRequest();

  ASSERT_TRUE(WaitForCredentialsReply(credentials_manager_->DeletePasswordAsync(QLatin1String(SubsonicSettings::kCredentialsService))));

}

}  // namespace
