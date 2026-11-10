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

#include "config.h"

#include "gtest_include.h"

#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include <QSignalSpy>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/sqlquery.h"
#include "device/devicedatabasebackend.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class DeviceDatabaseBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {

    ASSERT_TRUE(database_dir_.isValid());
    database_ = make_shared<Database>(nullptr, nullptr, database_dir_.filePath(u"strawberry.db"_s));
    ASSERT_TRUE(database_->startup_error().isEmpty()) << database_->startup_error().toStdString();
    backend_ = make_shared<DeviceDatabaseBackend>();
    backend_->Init(database_);

    device_.unique_id_ = u"test"_s;
    device_.friendly_name_ = u"Test device"_s;

  }

  void TearDown() override {
    database_->Close();
  }

  int DeviceCount() {
    QSqlDatabase db = database_->Connect();
    SqlQuery q(db);
    q.prepare(u"SELECT COUNT(*) FROM devices"_s);
    if (!q.Exec() || !q.next()) return -1;
    return q.value(0).toInt();
  }

  QStringList DeviceTables() {
    QSqlDatabase db = database_->Connect();
    return db.tables().filter(u"device_"_s);
  }

  QTemporaryDir database_dir_;
  SharedPtr<Database> database_;
  SharedPtr<DeviceDatabaseBackend> backend_;
  DeviceDatabaseBackend::Device device_;
};

TEST_F(DeviceDatabaseBackendTest, AddDeviceAddsRowAndTables) {

  const int id = backend_->AddDevice(device_);

  ASSERT_NE(-1, id);
  EXPECT_EQ(1, DeviceCount());
  EXPECT_TRUE(DeviceTables().contains(QStringLiteral("device_%1_songs").arg(id))) << DeviceTables().join(u' ').toStdString();

}

// Regression test: when the transaction couldn't be started, the device row and its tables were saved outside of it, but the device wasn't connected.
TEST_F(DeviceDatabaseBackendTest, AddDeviceWithoutTransactionAddsNothing) {

  // A transaction which is already open on the same connection makes starting another one fail.
  QSqlDatabase db = database_->Connect();
  ASSERT_TRUE(db.transaction());

  QSignalSpy spy_error(&*database_, &Database::Error);
  const int id = backend_->AddDevice(device_);
  ASSERT_TRUE(db.commit());

  EXPECT_EQ(-1, id);
  EXPECT_EQ(0, DeviceCount());
  EXPECT_TRUE(DeviceTables().isEmpty()) << DeviceTables().join(u' ').toStdString();
  ASSERT_EQ(1, spy_error.count());
  EXPECT_TRUE(spy_error.at(0).at(0).toString().contains(device_.friendly_name_)) << spy_error.at(0).at(0).toString().toStdString();

}

}  // namespace
