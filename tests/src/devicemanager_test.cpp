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

#include <atomic>
#include <memory>
#include <functional>

#include "gtest_include.h"

#include <gst/gst.h>

#include <QApplication>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMessageBox>
#include <QModelIndex>
#include <QAbstractButton>
#include <QList>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QString>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QVariantMap>
#include <QWidget>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/musicstorage.h"
#include "core/taskmanager.h"
#include "covermanager/albumcoverloader.h"
#include "device/devicedatabasebackend.h"
#include "device/deviceinfo.h"
#include "device/devicelister.h"
#include "device/devicemanager.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

// A lister which counts how many times the URLs of the device are requested, which is done when connecting it, without giving URLs, so no device is created.
class TestLister : public DeviceLister {
 public:
  QStringList DeviceUniqueIDs() override { return QStringList(); }
  QVariantList DeviceIcons(const QString &id) override {
    Q_UNUSED(id)
    return QVariantList();
  }
  QString DeviceManufacturer(const QString &id) override {
    Q_UNUSED(id)
    return QString();
  }
  QString DeviceModel(const QString &id) override {
    Q_UNUSED(id)
    return QString();
  }
  quint64 DeviceCapacity(const QString &id) override {
    Q_UNUSED(id)
    return 0;
  }
  quint64 DeviceFreeSpace(const QString &id) override {
    Q_UNUSED(id)
    return 0;
  }
  QVariantMap DeviceHardwareInfo(const QString &id) override {
    Q_UNUSED(id)
    return QVariantMap();
  }
  QString MakeFriendlyName(const QString &id) override {
    Q_UNUSED(id)
    return QString();
  }
  QList<QUrl> MakeDeviceUrls(const QString &id) override {
    Q_UNUSED(id)
    ++connect_requests;
    return QList<QUrl>();
  }
  void UpdateDeviceFreeSpace(const QString &id) override { Q_UNUSED(id) }

  std::atomic<int> connect_requests = 0;

 protected:
  bool Init() override { return true; }
};

class DeviceManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {

    gst_init(nullptr, nullptr);
    ASSERT_TRUE(temp_dir_.isValid());

    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    tagreader_client_ = make_shared<TagReaderClient>();
    albumcover_loader_ = make_shared<AlbumCoverLoader>(tagreader_client_);

    // The device is loaded from the database to have it in the model, then it is made a device which was never connected.
    DeviceDatabaseBackend backend;
    backend.Init(database_);
    DeviceDatabaseBackend::Device device;
    device.unique_id_ = u"test"_s;
    device.friendly_name_ = u"Test"_s;
    ASSERT_NE(-1, backend.AddDevice(device));

    device_manager_ = make_shared<DeviceManager>(task_manager_, database_, tagreader_client_, albumcover_loader_);
    // The listers of this computer can add its devices before the device is loaded from the database, so the device is found by its unique ID, not by its row.
    ASSERT_TRUE(WaitFor([this]() { return TestDeviceIndex().isValid(); }));
    index_ = TestDeviceIndex();

    DeviceInfo *device_info = device_manager_->GetDevice(index_);
    ASSERT_TRUE(device_info);
    device_info->database_id_ = -1;
    // The device loaded from the database has a backend without a lister, which is replaced by the backend of the attached device.
    device_info->backends_ = QList<DeviceInfo::Backend>() << DeviceInfo::Backend(&lister_, u"test"_s);

  }

  void TearDown() override {
    if (index_.isValid()) {
      device_manager_->GetDevice(index_)->backends_.clear();
    }
    device_manager_.reset();
    QCoreApplication::processEvents();
    database_->Close();
  }

  QModelIndex TestDeviceIndex() const {
    for (int row = 0; row < device_manager_->rowCount(QModelIndex()); ++row) {
      const QModelIndex idx = device_manager_->index(row, 0, QModelIndex());
      if (idx.data(DeviceManager::Role_UniqueId).toString() == "test"_L1) return idx;
    }
    return QModelIndex();
  }

  static bool WaitFor(const std::function<bool()> &condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition()) {
      if (timer.elapsed() > 10000) return false;
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return true;
  }

  // Waits for the dialog asking to connect the device, runs the action while it is open, then clicks "Connect device".
  void ConnectWhenAsked(const std::function<void()> &action) {
    QTimer *timer = new QTimer(&*device_manager_);
    QObject::connect(timer, &QTimer::timeout, timer, [this, timer, action]() {
      QMessageBox *messagebox = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
      if (!messagebox) return;
      timer->stop();
      timer->deleteLater();
      asked_ = true;
      action();
      // The URLs are also requested when the real listers add devices of this computer, so only the requests after this are counted.
      connect_requests_when_asked_ = lister_.connect_requests.load();
      for (QAbstractButton *button : messagebox->buttons()) {
        if (messagebox->buttonRole(button) == QMessageBox::AcceptRole) button->click();
      }
    });
    timer->start(10);
  }

  QTemporaryDir temp_dir_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  SharedPtr<DeviceManager> device_manager_;
  QPersistentModelIndex index_;
  TestLister lister_;
  bool asked_ = false;
  int connect_requests_when_asked_ = 0;
};

TEST_F(DeviceManagerTest, ConnectingNewDeviceAsksAndConnects) {

  ConnectWhenAsked([]() {});

  device_manager_->data(index_, MusicStorage::Role_StorageForceConnect);

  EXPECT_TRUE(asked_);
  EXPECT_EQ(connect_requests_when_asked_ + 1, lister_.connect_requests.load());

}

// Regression test: the device was used after it was deleted, when it was removed while the dialog asking to connect it was open.
TEST_F(DeviceManagerTest, DeviceRemovedWhileAskingIsNotConnected) {

  ConnectWhenAsked([this]() {
    QMetaObject::invokeMethod(&*device_manager_, "PhysicalDeviceRemoved", Qt::DirectConnection, Q_ARG(QString, u"test"_s));
  });

  const QVariant storage = device_manager_->data(index_, MusicStorage::Role_StorageForceConnect);

  EXPECT_TRUE(asked_);
  EXPECT_FALSE(index_.isValid());
  EXPECT_FALSE(storage.value<SharedPtr<MusicStorage>>());
  EXPECT_EQ(connect_requests_when_asked_, lister_.connect_requests.load());

}

}  // namespace
