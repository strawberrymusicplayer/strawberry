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

#include "includes/shared_ptr.h"
#include "core/settings.h"
#include "core/memorydatabase.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/urlhandlers.h"
#include "covermanager/albumcoverloader.h"
#include "credentialsmanager/credentialsmanager.h"
#include "constants/subsonicsettings.h"
#include "subsonic/subsonicservice.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class SubsonicServiceTest : public ::testing::Test {
 protected:
  void SetUp() override {

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
    service_ = std::make_unique<SubsonicService>(make_shared<TaskManager>(), database_, make_shared<NetworkAccessManager>(), make_shared<CredentialsManager>(), make_shared<UrlHandlers>(), make_shared<AlbumCoverLoader>(nullptr));

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

  SharedPtr<MemoryDatabase> database_;
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

}  // namespace
