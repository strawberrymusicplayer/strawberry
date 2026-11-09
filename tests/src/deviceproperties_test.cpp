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

#include <QCoreApplication>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRadioButton>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantMap>

#include "includes/shared_ptr.h"
#include "core/database.h"
#include "core/musicstorage.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "covermanager/albumcoverloader.h"
#include "device/connecteddevice.h"
#include "device/devicedatabasebackend.h"
#include "device/deviceinfo.h"
#include "device/devicelister.h"
#include "device/devicemanager.h"
#include "device/deviceproperties.h"
#include "tagreader/tagreaderclient.h"

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

namespace {

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
    return QList<QUrl>();
  }
  void UpdateDeviceFreeSpace(const QString &id) override { Q_UNUSED(id) }

 protected:
  bool Init() override { return true; }
};

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
  bool GetSupportedFiletypes(QList<Song::FileType> *ret) override {
    ++format_requests;
    *ret << Song::FileType::FLAC << Song::FileType::MPEG;
    return true;
  }

  std::atomic<int> format_requests = 0;
};

class DevicePropertiesTest : public ::testing::Test {
 protected:
  void SetUp() override {

    gst_init(nullptr, nullptr);
    ASSERT_TRUE(temp_dir_.isValid());

    task_manager_ = make_shared<TaskManager>();
    database_ = make_shared<Database>(nullptr, nullptr, temp_dir_.filePath(u"strawberry.db"_s));
    tagreader_client_ = make_shared<TagReaderClient>();
    albumcover_loader_ = make_shared<AlbumCoverLoader>(tagreader_client_);

    DeviceDatabaseBackend backend;
    backend.Init(database_);
    DeviceDatabaseBackend::Device device;
    device.unique_id_ = u"test"_s;
    device.friendly_name_ = u"Test"_s;
    const int database_id = backend.AddDevice(device);
    ASSERT_NE(-1, database_id);

    device_manager_ = make_shared<DeviceManager>(task_manager_, database_, tagreader_client_, albumcover_loader_);
    ASSERT_TRUE(WaitFor([this]() { return device_manager_->rowCount(QModelIndex()) > 0; }));
    index_ = device_manager_->index(0, 0, QModelIndex());

    DeviceInfo *info = device_manager_->GetDevice(index_);
    ASSERT_TRUE(info);
    info->transcode_mode_ = MusicStorage::TranscodeMode::Transcode_Always;
    info->transcode_format_ = Song::FileType::FLAC;
    info->backends_ << DeviceInfo::Backend(&lister_, u"test"_s);
    device_ = make_shared<TestDevice>(QUrl(u"test:device"_s), &lister_, u"test"_s, &*device_manager_, task_manager_, database_, tagreader_client_, albumcover_loader_, database_id, false);
    info->device_ = device_;

    dialog_ = std::make_unique<DeviceProperties>();
    dialog_->Init(device_manager_);

  }

  void TearDown() override {
    dialog_.reset();
    DeviceInfo *info = device_manager_->GetDevice(index_);
    info->device_.reset();
    info->backends_.clear();
    device_.reset();
    device_manager_.reset();
    QCoreApplication::processEvents();
    database_->Close();
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

  QRadioButton *RadioButton(const QString &name) const { return dialog_->findChild<QRadioButton*>(name); }
  QComboBox *FormatComboBox() const { return dialog_->findChild<QComboBox*>(u"transcode_format"_s); }
  QStackedWidget *FormatsStack() const { return dialog_->findChild<QStackedWidget*>(u"formats_stack"_s); }

  bool FormatsLoaded() const { return FormatsStack()->currentWidget()->objectName() == u"formats_page"_s; }

  // Progress of a scan or copy changes the device in the model.
  void ProgressChanged() {
    for (int i = 0; i < 3; ++i) {
      Q_EMIT device_manager_->dataChanged(index_, index_);
      QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
  }

  QTemporaryDir temp_dir_;
  SharedPtr<TaskManager> task_manager_;
  SharedPtr<Database> database_;
  SharedPtr<TagReaderClient> tagreader_client_;
  SharedPtr<AlbumCoverLoader> albumcover_loader_;
  SharedPtr<DeviceManager> device_manager_;
  QPersistentModelIndex index_;
  TestLister lister_;
  SharedPtr<TestDevice> device_;
  std::unique_ptr<DeviceProperties> dialog_;
};

// Regression test: every change of the device in the model set the saved choices again, and replaced the page with the choices with the loading page.
TEST_F(DevicePropertiesTest, ProgressKeepsUnsavedChoices) {

  dialog_->ShowDevice(index_);
  ASSERT_TRUE(WaitFor([this]() { return FormatsLoaded(); }));
  ASSERT_TRUE(RadioButton(u"transcode_all"_s)->isChecked());
  ASSERT_EQ(1, device_->format_requests.load());

  const QString saved_format = FormatComboBox()->currentText();
  int other_format_index = -1;
  for (int i = 0; i < FormatComboBox()->count() && other_format_index == -1; ++i) {
    if (FormatComboBox()->itemText(i) != saved_format) other_format_index = i;
  }
  ASSERT_NE(-1, other_format_index);
  FormatComboBox()->setCurrentIndex(other_format_index);
  const QString chosen_format = FormatComboBox()->currentText();
  RadioButton(u"transcode_off"_s)->setChecked(true);

  ProgressChanged();

  EXPECT_TRUE(FormatsLoaded());
  EXPECT_EQ(1, device_->format_requests.load());
  EXPECT_TRUE(RadioButton(u"transcode_off"_s)->isChecked());
  EXPECT_EQ(chosen_format, FormatComboBox()->currentText());

}

// Opening the dialog again shows what's saved, not what was not saved the last time.
TEST_F(DevicePropertiesTest, OpeningDialogLoadsSavedChoices) {

  dialog_->ShowDevice(index_);
  ASSERT_TRUE(WaitFor([this]() { return FormatsLoaded(); }));
  const QString saved_format = FormatComboBox()->currentText();
  RadioButton(u"transcode_off"_s)->setChecked(true);
  FormatComboBox()->setCurrentIndex(FormatComboBox()->currentIndex() == 0 ? 1 : 0);
  ASSERT_NE(saved_format, FormatComboBox()->currentText());

  dialog_->ShowDevice(index_);

  EXPECT_TRUE(RadioButton(u"transcode_all"_s)->isChecked());
  ASSERT_TRUE(WaitFor([this]() { return device_->format_requests.load() == 2 && FormatsLoaded(); }));
  EXPECT_EQ(saved_format, FormatComboBox()->currentText());

}

}  // namespace
