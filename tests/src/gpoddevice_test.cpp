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

#include <glib.h>
#include <gpod/itdb.h>

#include <QCoreApplication>
#include <QFuture>
#include <QList>
#include <QSemaphore>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariant>
#include <QtConcurrentRun>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "covermanager/albumcoverloader.h"
#include "device/devicedatabasebackend.h"
#include "device/devicelister.h"
#include "device/gpoddevice.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

class TestDeviceLister : public DeviceLister {
 public:
  QStringList DeviceUniqueIDs() override { return QStringList(); }
  QVariantList DeviceIcons(const QString &id) override { Q_UNUSED(id) return QVariantList(); }
  QString DeviceManufacturer(const QString &id) override { Q_UNUSED(id) return QString(); }
  QString DeviceModel(const QString &id) override { Q_UNUSED(id) return QString(); }
  quint64 DeviceCapacity(const QString &id) override { Q_UNUSED(id) return 0; }
  quint64 DeviceFreeSpace(const QString &id) override { Q_UNUSED(id) return 0; }
  QVariantMap DeviceHardwareInfo(const QString &id) override { Q_UNUSED(id) return QVariantMap(); }
  QString MakeFriendlyName(const QString &id) override { Q_UNUSED(id) return QString(); }
  QList<QUrl> MakeDeviceUrls(const QString &id) override { Q_UNUSED(id) return QList<QUrl>(); }
  void UpdateDeviceFreeSpace(const QString &id) override { Q_UNUSED(id) }

 protected:
  bool Init() override { return true; }
};

// Reports when a copy waits for the database, db_mutex_ is locked until the wait started, so loading can't finish before it.
class TestGPodDevice : public GPodDevice {
 public:
  using GPodDevice::GPodDevice;

  QSemaphore waiting_for_database;

 protected:
  void WaitForDatabase() override {
    waiting_for_database.release();
    GPodDevice::WaitForDatabase();
  }
};

class GPodDeviceTest : public ::testing::Test {
 protected:
  void SetUp() override {

    ASSERT_TRUE(database_dir_.isValid());
    ASSERT_TRUE(mount_dir_.isValid());

    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<Database>(nullptr, nullptr, database_dir_.filePath(u"strawberry.db"_s));
    device_backend_ = make_shared<DeviceDatabaseBackend>();
    device_backend_->Init(database_);
    DeviceDatabaseBackend::Device device;
    device.unique_id_ = u"test"_s;
    database_id_ = device_backend_->AddDevice(device);
    ASSERT_NE(-1, database_id_);

    albumcover_loader_ = make_shared<AlbumCoverLoader>(SharedPtr<TagReaderClient>());

  }

  void TearDown() override {
    device_.reset();
    QCoreApplication::processEvents();
    database_->Close();
  }

  void CreateDevice() {
    device_ = std::make_unique<TestGPodDevice>(QUrl::fromLocalFile(mount_dir_.path()), &lister_, u"test"_s, nullptr, task_manager_, database_, SharedPtr<TagReaderClient>(), albumcover_loader_, database_id_, true);
    ASSERT_TRUE(device_->Init());
  }

  void Connect(const bool expected_success) {
    QSignalSpy spy(&*device_, &ConnectedDevice::DeviceConnectFinished);
    device_->ConnectAsync();
    ASSERT_TRUE(spy.wait(10000));
    ASSERT_EQ(1, spy.count());
    EXPECT_EQ(expected_success, spy.first().at(1).toBool());
  }

  void InitIpod() const {
    GError *error = nullptr;
    const bool success = itdb_init_ipod(mount_dir_.path().toLocal8Bit().constData(), nullptr, "Test", &error);
    if (error) g_error_free(error);
    ASSERT_TRUE(success);
  }

  QTemporaryDir database_dir_;
  QTemporaryDir mount_dir_;
  TestDeviceLister lister_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<DeviceDatabaseBackend> device_backend_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  int database_id_ = -1;
  std::unique_ptr<TestGPodDevice> device_;
};

// Regression test: a database which couldn't be loaded was reported as connected.
TEST_F(GPodDeviceTest, FailedLoadIsReported) {

  CreateDevice();
  Connect(false);

}

// Regression test: starting a copy after the load failed waited forever for the database.
TEST_F(GPodDeviceTest, CopyFailsAfterFailedLoad) {

  CreateDevice();
  Connect(false);

  EXPECT_FALSE(device_->StartCopy(nullptr));
  QString error_text;
  EXPECT_FALSE(device_->FinishCopy(false, error_text));

}

// Regression test: a copy waiting for the database continued without one when the load failed.
TEST_F(GPodDeviceTest, CopyWaitingForLoadFailsWhenLoadFails) {

  CreateDevice();
  GPodDevice *device = &*device_;
  QFuture<bool> future = QtConcurrent::run([device]() { return device->StartCopy(nullptr); });
  // Not fatal, finishing the load and waiting for the copy below keeps the copy from using the device after it's deleted.
  EXPECT_TRUE(device_->waiting_for_database.tryAcquire(1, 5000));
  Connect(false);

  future.waitForFinished();
  EXPECT_FALSE(future.result());

}

// Regression test: deleting after the load failed used the missing database.
TEST_F(GPodDeviceTest, DeleteFailsAfterFailedLoad) {

  CreateDevice();
  Connect(false);

  device_->StartDelete();
  Song song;
  song.set_url(QUrl::fromLocalFile(mount_dir_.filePath(u"song.mp3"_s)));
  MusicStorage::DeleteJob job;
  job.metadata_ = song;
  EXPECT_FALSE(device_->DeleteFromStorage(job));
  QString error_text;
  EXPECT_FALSE(device_->FinishDelete(false, error_text));

}

// A loaded database still connects, and every copy releases the database for the next one.
TEST_F(GPodDeviceTest, CopyStartsAfterSuccessfulLoad) {

  InitIpod();
  CreateDevice();
  Connect(true);

  for (int i = 0; i < 2; ++i) {
    ASSERT_TRUE(device_->StartCopy(nullptr));
    QString error_text;
    EXPECT_TRUE(device_->FinishCopy(true, error_text));
  }

}

}  // namespace
