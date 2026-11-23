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
#include <QMutexLocker>
#include <QMutex>
#include <QAbstractButton>
#include <QList>
#include <QMetaObject>
#include <QPersistentModelIndex>
#include <QSignalSpy>
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
#include "device/connecteddevice.h"
#include "device/devicedatabasebackend.h"
#include "device/deviceinfo.h"
#include "device/devicelister.h"
#include "device/devicemanager.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

// Declared at file scope so that DeviceManager's "friend class DeviceManagerTestAccess;" resolves to this class.
class DeviceManagerTestAccess {
 public:
  // The device manager owns the lister, and starts it in its own thread.
  static void AddLister(DeviceManager &device_manager, DeviceLister *lister) { device_manager.AddLister(lister); }
};

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
    return urls;
  }
  void UpdateDeviceFreeSpace(const QString &id) override { Q_UNUSED(id) }
  void UnmountDevice(const QString &id) override { unmounted_ids << id; }

  std::atomic<int> connect_requests = 0;
  QStringList unmounted_ids;
  QList<QUrl> urls;

 protected:
  bool Init() override { return true; }
};

// The order of the requests to a lister which runs in its own thread, the lister is deleted by the device manager, the order is kept.
struct ListerEvents {
  QMutex mutex;
  QStringList events;
  QStringList Events() {
    QMutexLocker l(&mutex);
    return events;
  }
};

class EventsLister : public TestLister {
 public:
  explicit EventsLister(const SharedPtr<ListerEvents> &lister_events) : lister_events_(lister_events) {}
  void UnmountDevice(const QString &id) override {
    QMutexLocker l(&lister_events_->mutex);
    lister_events_->events << u"unmount "_s + id;
  }
  void ShutDown() override {
    QMutexLocker l(&lister_events_->mutex);
    lister_events_->events << u"shutdown"_s;
  }

 private:
  SharedPtr<ListerEvents> lister_events_;
};

// A device which is connected without a real device.
class TestDevice : public ConnectedDevice {
 public:
  using ConnectedDevice::ConnectedDevice;

  bool Init() override { return true; }
  bool CopyToStorage(const CopyJob &job, QString &error_text) override {
    Q_UNUSED(job)
    Q_UNUSED(error_text)
    return true;
  }
  bool DeleteFromStorage(const DeleteJob &job) override {
    Q_UNUSED(job)
    return true;
  }
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

  // Connects the device, and closes it to unmount it, like safely removing it does when the device has been closed.
  SharedPtr<TestDevice> ConnectTestDevice() {
    DeviceInfo *device_info = device_manager_->GetDevice(index_);
    if (!device_info) return SharedPtr<TestDevice>();
    device_info->database_id_ = 1;
    SharedPtr<TestDevice> device = make_shared<TestDevice>(QUrl(u"test:device"_s), &lister_, u"test"_s, &*device_manager_, task_manager_, database_, tagreader_client_, albumcover_loader_, 1, false);
    device_info->device_ = device;
    device_info->unmount_ = true;
    return device;
  }

  void DeviceCloseFinished() {
    QMetaObject::invokeMethod(&*device_manager_, "DeviceCloseFinished", Qt::DirectConnection, Q_ARG(QString, u"test"_s));
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

// Regression test: when a forgotten device had a backend which was not attached, removing the attached backend kept the device without a lister, and connecting it crashed.
TEST_F(DeviceManagerTest, ForgottenDeviceIsRemovedWithLastAttachedBackend) {

  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  device_info->backends_ = QList<DeviceInfo::Backend>() << DeviceInfo::Backend(nullptr, u"other"_s) << DeviceInfo::Backend(&lister_, u"test"_s);

  QMetaObject::invokeMethod(&*device_manager_, "PhysicalDeviceRemoved", Qt::DirectConnection, Q_ARG(QString, u"test"_s));

  EXPECT_FALSE(index_.isValid());

}

// Regression test: connecting a device which was never saved and which has no lister crashed, it is not attached, so it is not connected.
TEST_F(DeviceManagerTest, ConnectingNewDeviceWithoutListerDoesNothing) {

  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  device_info->backends_ = QList<DeviceInfo::Backend>() << DeviceInfo::Backend(nullptr, u"test"_s);
  ConnectWhenAsked([]() {});

  const QVariant storage = device_manager_->data(index_, MusicStorage::Role_StorageForceConnect);

  EXPECT_FALSE(asked_);
  EXPECT_FALSE(storage.value<SharedPtr<MusicStorage>>());
  EXPECT_TRUE(index_.isValid());

}

// Regression test: safely removing a saved device which was attached, but not connected, did nothing.
TEST_F(DeviceManagerTest, SafelyRemovingSavedDeviceUnmountsIt) {

  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  device_info->database_id_ = 1;
  ASSERT_FALSE(device_info->device_);

  device_manager_->Unmount(index_);

  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));
  EXPECT_EQ(QStringList() << u"test"_s, lister_.unmounted_ids);

}

// A device which is not attached has nothing to unmount.
TEST_F(DeviceManagerTest, SafelyRemovingDeviceWithoutListerDoesNothing) {

  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  device_info->database_id_ = 1;
  device_info->backends_ = QList<DeviceInfo::Backend>() << DeviceInfo::Backend(nullptr, u"test"_s);

  device_manager_->Unmount(index_);
  QCoreApplication::processEvents();

  EXPECT_TRUE(lister_.unmounted_ids.isEmpty());
  EXPECT_TRUE(index_.isValid());

}

// Regression test: safely removing a device while files were copied to it unmounted it in the middle of copying, the copy keeps the device.
TEST_F(DeviceManagerTest, DeviceInUseIsUnmountedWhenNoLongerUsed) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  // The device is held by the copy, like Organize holds it.
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();

  DeviceCloseFinished();
  QCoreApplication::processEvents();
  EXPECT_FALSE(device_manager_->GetDevice(index_)->device_);
  EXPECT_TRUE(lister_.unmounted_ids.isEmpty());

  // The copy finished.
  copy_destination.reset();
  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));
  EXPECT_EQ(QStringList() << u"test"_s, lister_.unmounted_ids);

}

// Regression test: safely removing the device again while it was still used unmounted it right away.
TEST_F(DeviceManagerTest, RemovingDeviceInUseAgainDoesNotUnmountIt) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();
  DeviceCloseFinished();

  device_manager_->Unmount(index_);
  QCoreApplication::processEvents();
  EXPECT_TRUE(lister_.unmounted_ids.isEmpty());

  // It is unmounted once when the copy finished.
  copy_destination.reset();
  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));
  QCoreApplication::processEvents();
  EXPECT_EQ(QStringList() << u"test"_s, lister_.unmounted_ids);

}

// Regression test: the device could be connected again while it was still used after it was closed to unmount it, and the new device was unmounted when the old one was no longer used.
TEST_F(DeviceManagerTest, DeviceInUseIsNotConnectedAgainBeforeUnmounting) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();
  DeviceCloseFinished();

  const int connect_requests = lister_.connect_requests;
  EXPECT_FALSE(device_manager_->Connect(index_));
  EXPECT_EQ(connect_requests, lister_.connect_requests);

  // It can be connected again when it was unmounted.
  copy_destination.reset();
  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));
  device_manager_->Connect(index_);
  EXPECT_EQ(connect_requests + 1, lister_.connect_requests);

}

// Regression test: exiting while a device which was safely removed was still used closed the listers before the device was unmounted, so it was not unmounted.
TEST_F(DeviceManagerTest, ExitWaitsForDeviceInUseToUnmount) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();
  DeviceCloseFinished();

  QSignalSpy exit_finished_spy(&*device_manager_, &DeviceManager::ExitFinished);
  device_manager_->Exit();
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < 1000 && exit_finished_spy.isEmpty()) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
  }
  EXPECT_TRUE(exit_finished_spy.isEmpty());

  // The copy finished, the device is unmounted, then exiting finishes.
  copy_destination.reset();
  EXPECT_TRUE(WaitFor([&exit_finished_spy]() { return !exit_finished_spy.isEmpty(); }));
  EXPECT_EQ(QStringList() << u"test"_s, lister_.unmounted_ids);

}

// Regression test: a saved device which was safely removed while it was still used, and then removed and attached again, was unmounted when the old device was no longer used, and could not be connected until then.
TEST_F(DeviceManagerTest, DeviceAttachedAgainIsNotUnmountedByOlderRemoval) {

  QObject::connect(&lister_, SIGNAL(DeviceRemoved(QString)), &*device_manager_, SLOT(PhysicalDeviceRemoved(QString)));

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();
  DeviceCloseFinished();

  // The device is removed, and attached again.
  Q_EMIT lister_.DeviceRemoved(u"test"_s);
  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  ASSERT_EQ(1, device_info->backends_.count());
  ASSERT_EQ(nullptr, device_info->backends_[0].lister_);
  device_info->backends_[0].lister_ = &lister_;

  const int connect_requests = lister_.connect_requests;
  device_manager_->Connect(index_);
  EXPECT_EQ(connect_requests + 1, lister_.connect_requests);

  copy_destination.reset();
  QCoreApplication::processEvents();
  EXPECT_TRUE(lister_.unmounted_ids.isEmpty());

}

// Regression test: exiting before the device which was safely removed was closed, while it was still used, closed the listers before the unmount was requested.
TEST_F(DeviceManagerTest, ExitBeforeCloseFinishedUnmountsBeforeClosingListers) {

  SharedPtr<ListerEvents> lister_events = make_shared<ListerEvents>();
  EventsLister *events_lister = new EventsLister(lister_events);
  DeviceManagerTestAccess::AddLister(*device_manager_, events_lister);

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  device_manager_->GetDevice(index_)->backends_ = QList<DeviceInfo::Backend>() << DeviceInfo::Backend(events_lister, u"test"_s);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();

  QSignalSpy exit_finished_spy(&*device_manager_, &DeviceManager::ExitFinished);
  device_manager_->Exit();
  DeviceCloseFinished();

  copy_destination.reset();
  EXPECT_TRUE(WaitFor([&exit_finished_spy]() { return !exit_finished_spy.isEmpty(); }));
  EXPECT_EQ(QStringList() << u"unmount test"_s << u"shutdown"_s, lister_events->Events());

}

// Regression test: the device could be connected while the warning for the device was shown, after it was closed to unmount it, and the new device was unmounted when the old one was no longer used.
TEST_F(DeviceManagerTest, DeviceInUseIsNotConnectedAfterWarning) {

  DeviceInfo *device_info = device_manager_->GetDevice(index_);
  ASSERT_TRUE(device_info);
  device_info->database_id_ = 1;
  // The first URL shows the warning, the second can be connected.
  lister_.urls = QList<QUrl>() << QUrl(u"gphoto2://test"_s) << QUrl::fromLocalFile(temp_dir_.path());

  SharedPtr<MusicStorage> copy_destination;
  ConnectWhenAsked([this, &copy_destination]() {
    SharedPtr<TestDevice> device = ConnectTestDevice();
    copy_destination = device;
    DeviceCloseFinished();
  });
  EXPECT_FALSE(device_manager_->Connect(index_));
  EXPECT_TRUE(asked_);
  EXPECT_FALSE(device_manager_->GetDevice(index_)->device_);

  copy_destination.reset();
  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));

}

// Regression test: the device could be connected again while it was reported as disconnected, before the unmount was pending, and that was unmounted when the old device was no longer used.
TEST_F(DeviceManagerTest, DeviceInUseIsNotConnectedWhenReportedDisconnected) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  SharedPtr<MusicStorage> copy_destination = device;
  device.reset();

  const int connect_requests = lister_.connect_requests;
  bool reported = false;
  QObject::connect(&*device_manager_, &DeviceManager::DeviceDisconnected, &*device_manager_, [this, &reported]() {
    reported = true;
    device_manager_->Connect(index_);
  });
  DeviceCloseFinished();
  EXPECT_TRUE(reported);
  EXPECT_EQ(connect_requests, lister_.connect_requests);

}

// A device which is not used any more is unmounted when it is closed.
TEST_F(DeviceManagerTest, DeviceNotInUseIsUnmountedWhenClosed) {

  SharedPtr<TestDevice> device = ConnectTestDevice();
  ASSERT_TRUE(device);
  device.reset();

  DeviceCloseFinished();

  EXPECT_TRUE(WaitFor([this]() { return !lister_.unmounted_ids.isEmpty(); }));
  EXPECT_EQ(QStringList() << u"test"_s, lister_.unmounted_ids);

}

}  // namespace
