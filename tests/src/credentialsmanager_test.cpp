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

#include "gtest_include.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QCoreApplication>
#include <QElapsedTimer>

#include "includes/scoped_ptr.h"
#include "core/settings.h"
#include "credentialsmanager/credentialsmanager.h"
#include "credentialsmanager/credentialsbackendinterface.h"
#include "credentialsmanager/credentialsreply.h"
#include "credentialsmanager/credentialsresult.h"

using namespace Qt::Literals::StringLiterals;

namespace {

using BackendType = CredentialsManager::BackendType;

// The passwords stored in each fake backend, kept between credentials managers, like a system keyring.
using BackendStores = QMap<BackendType, QMap<QString, QString>>;

class FakeCredentialsBackend : public CredentialsBackendInterface {
 public:
  FakeCredentialsBackend(const BackendType backend_type, const bool available, const bool writes_fail, QMap<QString, QString> *passwords) : backend_type_(backend_type), available_(available), writes_fail_(writes_fail), passwords_(passwords) {}

  QString name() const override { return CredentialsManager::BackendToString(backend_type_); }

  void CheckAvailable(const AvailableCallback &available_callback) override { available_callback(available_); }

  void ReadPassword(const QString &service, const ResultCallback &callback) override {
    callback(passwords_->contains(service) ? CredentialsResult::Success(passwords_->value(service)) : CredentialsResult::NotFound());
  }

  void SavePassword(const QString &service, const QString &password, const ResultCallback &callback) override {
    if (writes_fail_) {
      callback(CredentialsResult::Error(u"Save failed."_s));
      return;
    }
    passwords_->insert(service, password);
    callback(CredentialsResult::Success());
  }

  void DeletePassword(const QString &service, const ResultCallback &callback) override {
    if (writes_fail_) {
      callback(CredentialsResult::Error(u"Delete failed."_s));
      return;
    }
    passwords_->remove(service);
    callback(CredentialsResult::Success());
  }

 private:
  const BackendType backend_type_;
  const bool available_;
  const bool writes_fail_;
  QMap<QString, QString> *passwords_;
};

// Detects the fake backends in the given order, like the desktop decides the order of the real backends.
class TestCredentialsManager : public CredentialsManager {
 public:
  TestCredentialsManager(const QList<BackendType> &auto_backend_types, const QList<BackendType> &unavailable_backend_types, BackendStores *stores, const QList<BackendType> &write_failing_backend_types = QList<BackendType>()) : auto_backend_types_(auto_backend_types), unavailable_backend_types_(unavailable_backend_types), write_failing_backend_types_(write_failing_backend_types), stores_(stores) {}

 protected:
  QList<BackendType> AutoBackendTypes() const override { return auto_backend_types_; }

  ScopedPtr<CredentialsBackendInterface> CreateBackend(const BackendType backend_type) override {
    return ScopedPtr<CredentialsBackendInterface>(new FakeCredentialsBackend(backend_type, !unavailable_backend_types_.contains(backend_type), write_failing_backend_types_.contains(backend_type), &(*stores_)[backend_type]));
  }

 public:
  // The availability is checked when a backend is created, like a keyring daemon which is started later.
  void SetUnavailableBackendTypes(const QList<BackendType> &unavailable_backend_types) { unavailable_backend_types_ = unavailable_backend_types; }

 private:
  const QList<BackendType> auto_backend_types_;
  QList<BackendType> unavailable_backend_types_;
  const QList<BackendType> write_failing_backend_types_;
  BackendStores *stores_;
};

class CredentialsManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    Settings s;
    s.remove(QLatin1String(CredentialsManager::kSettingsGroup));
  }

  static bool WaitForReply(const CredentialsReplyPtr &reply) {
    QElapsedTimer timer;
    timer.start();
    while (!reply->finished() && timer.elapsed() < 5000) {
      QCoreApplication::processEvents();
    }
    return reply->finished();
  }

  static QString SettingsValue(const char *key) {
    Settings s;
    s.beginGroup(CredentialsManager::kSettingsGroup);
    const QString value = s.value(key).toString();
    s.endGroup();
    return value;
  }

  static void SetSettingsValue(const char *key, const QString &value) {
    Settings s;
    s.beginGroup(CredentialsManager::kSettingsGroup);
    s.setValue(key, value);
    s.endGroup();
  }

  BackendStores stores_;
};

// In Auto mode, a different backend can be detected on a later start, for example when the desktop is not detected, the passwords must still be read from the backend which stored them.
TEST_F(CredentialsManagerTest, PasswordReadFromBackendWhichStoredItWhenAnotherIsDetected) {

  {
    TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::LibSecret << BackendType::Settings, QList<BackendType>(), &stores_);
    const CredentialsReplyPtr reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"secret"_s);
    ASSERT_TRUE(WaitForReply(reply));
    ASSERT_TRUE(reply->success());
  }
  EXPECT_EQ(u"secret"_s, stores_[BackendType::KWallet].value(u"subsonic"_s));
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, reply->password());

}

// Reading doesn't decide the backend, since nothing is stored yet, so a backend detected later is used.
TEST_F(CredentialsManagerTest, BackendNotRememberedWhenOnlyReading) {

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_EQ(CredentialsResult::Status::NotFound, reply->result().status);
  EXPECT_TRUE(SettingsValue(CredentialsManager::kAutoBackend).isEmpty());

}

// When the backend which stored the passwords is not available, another backend is used for reading, and the remembered backend is kept, since it's usually only temporarily unavailable.
// Saving fails, since a password saved in another backend would be hidden by the password in the remembered backend when it's available again.
TEST_F(CredentialsManagerTest, UnavailableRememberedBackendKept) {

  SetSettingsValue(CredentialsManager::kAutoBackend, u"kwallet"_s);
  stores_[BackendType::LibSecret].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr read_reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(read_reply));
  ASSERT_TRUE(read_reply->success());
  EXPECT_EQ(u"secret"_s, read_reply->password());
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

  const CredentialsReplyPtr save_reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"new"_s);
  ASSERT_TRUE(WaitForReply(save_reply));
  EXPECT_EQ(CredentialsResult::Status::Error, save_reply->result().status);
  EXPECT_EQ(u"secret"_s, stores_[BackendType::LibSecret].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::KWallet].contains(u"subsonic"_s));
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

}

// Deleting fails while the remembered backend is not available, since the password in it would be returned again when it's available.
TEST_F(CredentialsManagerTest, DeleteFailsWhileRememberedBackendUnavailable) {

  SetSettingsValue(CredentialsManager::kAutoBackend, u"kwallet"_s);
  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr reply = credentials_manager.DeletePasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_EQ(CredentialsResult::Status::Error, reply->result().status);
  EXPECT_EQ(u"secret"_s, stores_[BackendType::KWallet].value(u"subsonic"_s));

}

// The remembered backend is used when it's available later, like a keyring daemon which is started after Strawberry.
TEST_F(CredentialsManagerTest, RememberedBackendUsedWhenAvailableLater) {

  SetSettingsValue(CredentialsManager::kAutoBackend, u"kwallet"_s);
  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr read_reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(read_reply));
  EXPECT_EQ(CredentialsResult::Status::NotFound, read_reply->result().status);

  credentials_manager.SetUnavailableBackendTypes(QList<BackendType>());

  const CredentialsReplyPtr read_available_reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(read_available_reply));
  ASSERT_TRUE(read_available_reply->success());
  EXPECT_EQ(u"secret"_s, read_available_reply->password());

  const CredentialsReplyPtr save_reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"new"_s);
  ASSERT_TRUE(WaitForReply(save_reply));
  ASSERT_TRUE(save_reply->success());
  EXPECT_EQ(u"new"_s, stores_[BackendType::KWallet].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::LibSecret].contains(u"subsonic"_s));
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

}

// A password in the settings file is not moved to a backend which is only used because the remembered backend is not available.
TEST_F(CredentialsManagerTest, PasswordNotMovedFromSettingsWhenRememberedBackendUnavailable) {

  SetSettingsValue(CredentialsManager::kAutoBackend, u"kwallet"_s);
  stores_[BackendType::Settings].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, reply->password());
  EXPECT_EQ(u"secret"_s, stores_[BackendType::Settings].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::LibSecret].contains(u"subsonic"_s));

}

// A backend selected in the settings is used, and the backend remembered in Auto mode is not changed.
TEST_F(CredentialsManagerTest, SelectedBackendNotRemembered) {

  SetSettingsValue(CredentialsManager::kBackend, u"libsecret"_s);
  SetSettingsValue(CredentialsManager::kAutoBackend, u"kwallet"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::LibSecret << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"secret"_s);
  ASSERT_TRUE(WaitForReply(reply));
  ASSERT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, stores_[BackendType::LibSecret].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::KWallet].contains(u"subsonic"_s));
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

}

// Passwords stored before the backend was remembered are only read at startup, so reading a password remembers the backend too.
TEST_F(CredentialsManagerTest, SuccessfulReadRemembersBackend) {

  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"secret"_s);

  {
    TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::LibSecret << BackendType::Settings, QList<BackendType>(), &stores_);
    const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
    ASSERT_TRUE(WaitForReply(reply));
    ASSERT_TRUE(reply->success());
  }
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::LibSecret << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, reply->password());

}

// The settings file is only used when no secure backend is available, it's not remembered, so a secure backend is used when it's available later.
TEST_F(CredentialsManagerTest, SettingsBackendNotRemembered) {

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"secret"_s);
  ASSERT_TRUE(WaitForReply(reply));
  ASSERT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, stores_[BackendType::Settings].value(u"subsonic"_s));
  EXPECT_TRUE(SettingsValue(CredentialsManager::kAutoBackend).isEmpty());

}

// A password stored in the settings file while no secure backend was available is moved to the secure backend when it's available.
TEST_F(CredentialsManagerTest, PasswordMovedFromSettingsToSecureBackend) {

  stores_[BackendType::Settings].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, reply->password());
  EXPECT_EQ(u"secret"_s, stores_[BackendType::KWallet].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::Settings].contains(u"subsonic"_s));
  EXPECT_EQ(u"kwallet"_s, SettingsValue(CredentialsManager::kAutoBackend));

}

// When the password can't be moved to the secure backend, it's still returned, and kept in the settings file.
TEST_F(CredentialsManagerTest, PasswordKeptInSettingsWhenMovingFails) {

  stores_[BackendType::Settings].insert(u"subsonic"_s, u"secret"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_, QList<BackendType>() << BackendType::KWallet);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_TRUE(reply->success());
  EXPECT_EQ(u"secret"_s, reply->password());
  EXPECT_EQ(u"secret"_s, stores_[BackendType::Settings].value(u"subsonic"_s));
  EXPECT_TRUE(SettingsValue(CredentialsManager::kAutoBackend).isEmpty());

}

// A deleted password is deleted from the settings file too, so it's not moved back from there when it's read.
TEST_F(CredentialsManagerTest, DeletedPasswordNotMovedBackFromSettings) {

  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"new"_s);
  stores_[BackendType::Settings].insert(u"subsonic"_s, u"old"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr delete_reply = credentials_manager.DeletePasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(delete_reply));
  ASSERT_TRUE(delete_reply->success());
  const CredentialsReplyPtr read_reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(read_reply));
  EXPECT_EQ(CredentialsResult::Status::NotFound, read_reply->result().status);
  EXPECT_FALSE(stores_[BackendType::KWallet].contains(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::Settings].contains(u"subsonic"_s));

}

// When the password can't be deleted from the settings file, deleting fails, since it would be moved back from there when it's read.
TEST_F(CredentialsManagerTest, DeleteFailsWhenDeletingFromSettingsFails) {

  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"new"_s);
  stores_[BackendType::Settings].insert(u"subsonic"_s, u"old"_s);

  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_, QList<BackendType>() << BackendType::Settings);
  const CredentialsReplyPtr reply = credentials_manager.DeletePasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_EQ(CredentialsResult::Status::Error, reply->result().status);

}

// A password deleted with a selected backend is deleted from the settings file too, otherwise it would be moved back from there when Auto mode is selected later.
TEST_F(CredentialsManagerTest, PasswordDeletedWithSelectedBackendNotMovedBackFromSettings) {

  SetSettingsValue(CredentialsManager::kBackend, u"kwallet"_s);
  stores_[BackendType::KWallet].insert(u"subsonic"_s, u"new"_s);
  stores_[BackendType::Settings].insert(u"subsonic"_s, u"old"_s);

  {
    TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
    const CredentialsReplyPtr reply = credentials_manager.DeletePasswordAsync(u"subsonic"_s);
    ASSERT_TRUE(WaitForReply(reply));
    ASSERT_TRUE(reply->success());
  }
  EXPECT_FALSE(stores_[BackendType::Settings].contains(u"subsonic"_s));

  SetSettingsValue(CredentialsManager::kBackend, u"auto"_s);
  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_EQ(CredentialsResult::Status::NotFound, reply->result().status);
  EXPECT_FALSE(stores_[BackendType::KWallet].contains(u"subsonic"_s));

}

// Saving a password in a secure backend deletes the old password in the settings file, otherwise it would be used when no secure backend is available.
TEST_F(CredentialsManagerTest, SavingDeletesOldPasswordFromSettings) {

  stores_[BackendType::Settings].insert(u"subsonic"_s, u"old"_s);

  {
    TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>(), &stores_);
    const CredentialsReplyPtr reply = credentials_manager.SavePasswordAsync(u"subsonic"_s, u"new"_s);
    ASSERT_TRUE(WaitForReply(reply));
    ASSERT_TRUE(reply->success());
  }
  EXPECT_EQ(u"new"_s, stores_[BackendType::KWallet].value(u"subsonic"_s));
  EXPECT_FALSE(stores_[BackendType::Settings].contains(u"subsonic"_s));

  // The settings file is used when the secure backend is not available, and the old password is not returned.
  SetSettingsValue(CredentialsManager::kAutoBackend, QString());
  TestCredentialsManager credentials_manager(QList<BackendType>() << BackendType::KWallet << BackendType::Settings, QList<BackendType>() << BackendType::KWallet, &stores_);
  const CredentialsReplyPtr reply = credentials_manager.ReadPasswordAsync(u"subsonic"_s);
  ASSERT_TRUE(WaitForReply(reply));
  EXPECT_EQ(CredentialsResult::Status::NotFound, reply->result().status);

}

}  // namespace
